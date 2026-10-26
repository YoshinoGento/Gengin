#define NOMINMAX
#include "Audio.h"
#include "base/LoadTiming.h"

#include "base/Logger.h"

#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace {
constexpr float kRateComparisonEpsilon = 0.0001f;

void LogHResult(const char* operation, HRESULT result)
{
	std::ostringstream message;
	message << operation << " failed. HRESULT=0x"
		<< std::hex << std::uppercase << static_cast<uint32_t>(result);
	Logger::Log(message.str());
}

std::wstring Utf8ToWide(const std::string& text)
{
	if (text.empty() || text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
		return {};
	}

	const int requiredLength = MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		text.data(),
		static_cast<int>(text.size()),
		nullptr,
		0);
	if (requiredLength <= 0) {
		return {};
	}

	std::wstring result(static_cast<std::size_t>(requiredLength), L'\0');
	if (MultiByteToWideChar(
		CP_UTF8,
		MB_ERR_INVALID_CHARS,
		text.data(),
		static_cast<int>(text.size()),
		result.data(),
		requiredLength) != requiredLength) {
		return {};
	}
	return result;
}

std::wstring MakePathCacheKey(std::wstring path)
{
	std::transform(path.begin(), path.end(), path.begin(), [](wchar_t character) {
		return static_cast<wchar_t>(std::towlower(character));
	});
	return path;
}

AudioPlaybackDirection OppositeDirection(AudioPlaybackDirection direction)
{
	return direction == AudioPlaybackDirection::Forward
		? AudioPlaybackDirection::Reverse
		: AudioPlaybackDirection::Forward;
}

float ClampPlaybackRate(float playbackRate)
{
	return std::clamp(playbackRate, XAUDIO2_MIN_FREQ_RATIO, XAUDIO2_MAX_FREQ_RATIO);
}

float ClampVolume(float volume)
{
	return std::clamp(volume, 0.0f, XAUDIO2_MAX_VOLUME_LEVEL);
}

bool NearlyEqual(float lhs, float rhs)
{
	return std::abs(lhs - rhs) <= kRateComparisonEpsilon;
}
}

Audio::~Audio()
{
	Finalize();
}

bool Audio::Initialize()
{
	if (initialized_) {
		return IsOwnerThread("Audio::Initialize");
	}

	ownerThread_ = std::this_thread::get_id();
	HRESULT result = XAudio2Create(xAudio2_.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(result)) {
		LogHResult("XAudio2Create", result);
		ownerThread_ = {};
		return false;
	}

	result = xAudio2_->CreateMasteringVoice(&masterVoice_);
	if (FAILED(result)) {
		LogHResult("IXAudio2::CreateMasteringVoice", result);
		xAudio2_.Reset();
		ownerThread_ = {};
		return false;
	}

	result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	if (FAILED(result)) {
		LogHResult("MFStartup", result);
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
		xAudio2_.Reset();
		ownerThread_ = {};
		return false;
	}

	mediaFoundationStarted_ = true;
	initialized_ = true;
	return true;
}

void Audio::Finalize()
{
	for (auto& [voiceId, voice] : voices_) {
		(void)voiceId;
		DestroyVoice(voice);
	}
	voices_.clear();
	clips_.clear();
	clipCache_.clear();

	if (masterVoice_ != nullptr) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
	}
	xAudio2_.Reset();

	if (mediaFoundationStarted_) {
		const HRESULT result = MFShutdown();
		if (FAILED(result)) {
			LogHResult("MFShutdown", result);
		}
		mediaFoundationStarted_ = false;
	}

	nextClipId_ = 1;
	nextVoiceId_ = 1;
	temporalDirection_ = AudioPlaybackDirection::Forward;
	currentTemporalPlaybackRate_ = 1.0f;
	targetTemporalPlaybackRate_ = 1.0f;
	temporalRateRampStart_ = 1.0f;
	temporalRateRampDuration_ = 0.0f;
	temporalRateRampElapsed_ = 0.0f;
	initialized_ = false;
	ownerThread_ = {};
}

void Audio::Update(float realDeltaSeconds)
{
	if (!initialized_ || !IsOwnerThread("Audio::Update")) {
		return;
	}
	if (!std::isfinite(realDeltaSeconds) || realDeltaSeconds < 0.0f) {
		Logger::Log("Audio::Update rejected an invalid delta time.");
		return;
	}

	const float previousTemporalRate = currentTemporalPlaybackRate_;
	UpdateTemporalRate(realDeltaSeconds);
	const bool temporalRateChanged = !NearlyEqual(previousTemporalRate, currentTemporalPlaybackRate_);

	for (auto iterator = voices_.begin(); iterator != voices_.end();) {
		Voice& voice = iterator->second;
		const float previousBaseRate = voice.currentBasePlaybackRate;
		UpdateBaseRate(voice, realDeltaSeconds);
		if (temporalRateChanged || !NearlyEqual(previousBaseRate, voice.currentBasePlaybackRate)) {
			(void)ApplyVoiceFrequencyRatio(voice);
		}

		XAUDIO2_VOICE_STATE state{};
		voice.sourceVoice->GetState(&state);
		if (!voice.loop && state.BuffersQueued == 0) {
			DestroyVoice(voice);
			iterator = voices_.erase(iterator);
			continue;
		}
		++iterator;
	}
}

AudioClipHandle Audio::LoadAudio(const std::string& filename)
{
	if (!initialized_ || !IsOwnerThread("Audio::LoadAudio")) {
		return {};
	}

	const std::wstring canonicalPath = ResolveCanonicalPath(filename);
	if (canonicalPath.empty()) {
		Logger::Log("Audio::LoadAudio received an invalid or missing path: " + filename);
		return {};
	}

	const std::wstring cacheKey = MakePathCacheKey(canonicalPath);
	if (const auto cached = clipCache_.find(cacheKey); cached != clipCache_.end()) {
		return cached->second;
	}

	Clip clip{};
	clip.canonicalPath = canonicalPath;
	if (!DecodeAudioFile(canonicalPath, clip)) {
		return {};
	}

	const AudioClipHandle handle = AllocateClipHandle();
	if (!handle) {
		Logger::Log("Audio::LoadAudio exhausted the clip handle space.");
		return {};
	}

	clips_.emplace(handle.value, std::move(clip));
	clipCache_.emplace(cacheKey, handle);
	return handle;
}

bool Audio::UnloadAudio(AudioClipHandle clipHandle)
{
	if (!initialized_ || !IsOwnerThread("Audio::UnloadAudio") || !clipHandle) {
		return false;
	}

	const auto clipIterator = clips_.find(clipHandle.value);
	if (clipIterator == clips_.end()) {
		return false;
	}

	for (auto iterator = voices_.begin(); iterator != voices_.end();) {
		if (iterator->second.clipHandle == clipHandle) {
			DestroyVoice(iterator->second);
			iterator = voices_.erase(iterator);
		} else {
			++iterator;
		}
	}

	clipCache_.erase(MakePathCacheKey(clipIterator->second.canonicalPath));
	clips_.erase(clipIterator);
	return true;
}

void Audio::UnloadAllAudio()
{
	if (!initialized_ || !IsOwnerThread("Audio::UnloadAllAudio")) {
		return;
	}
	StopAllVoices();
	clips_.clear();
	clipCache_.clear();
}

AudioVoiceHandle Audio::PlayWave(AudioClipHandle clipHandle, bool loop)
{
	PlaySettings settings{};
	settings.loop = loop;
	return Play(clipHandle, settings);
}

AudioVoiceHandle Audio::Play(AudioClipHandle clipHandle, const PlaySettings& settings)
{
	loading::Scope timing(loading::Stage::AudioVoiceSetup);
	if (!initialized_ || !IsOwnerThread("Audio::Play") || !clipHandle) {
		return {};
	}
	if (!std::isfinite(settings.volume) || !std::isfinite(settings.playbackRate) ||
		settings.volume < 0.0f || settings.playbackRate <= 0.0f) {
		Logger::Log("Audio::Play rejected invalid volume or playback rate settings.");
		return {};
	}

	const auto clipIterator = clips_.find(clipHandle.value);
	if (clipIterator == clips_.end()) {
		Logger::Log("Audio::Play rejected a stale clip handle.");
		return {};
	}
	const Clip& clip = clipIterator->second;
	const WAVEFORMATEX* waveFormat = &clip.waveFormat.Format;

	Voice voice{};
	voice.clipHandle = clipHandle;
	voice.baseDirection = settings.direction;
	voice.appliedDirection = ResolveAppliedDirection(voice);
	voice.loop = settings.loop;
	voice.paused = settings.startPaused;
	voice.volume = ClampVolume(settings.volume);
	voice.currentBasePlaybackRate = ClampPlaybackRate(settings.playbackRate);
	voice.targetBasePlaybackRate = voice.currentBasePlaybackRate;
	voice.baseRateRampStart = voice.currentBasePlaybackRate;

	HRESULT result = xAudio2_->CreateSourceVoice(
		&voice.sourceVoice,
		waveFormat,
		0,
		XAUDIO2_MAX_FREQ_RATIO);
	if (FAILED(result)) {
		LogHResult("IXAudio2::CreateSourceVoice", result);
		return {};
	}

	result = voice.sourceVoice->SetVolume(voice.volume);
	if (FAILED(result) || !ApplyVoiceFrequencyRatio(voice) ||
		!ResubmitVoice(voice, clip, voice.appliedDirection, 0)) {
		if (FAILED(result)) {
			LogHResult("IXAudio2SourceVoice::SetVolume", result);
		}
		DestroyVoice(voice);
		return {};
	}

	if (!voice.paused) {
		result = voice.sourceVoice->Start();
		if (FAILED(result)) {
			LogHResult("IXAudio2SourceVoice::Start", result);
			DestroyVoice(voice);
			return {};
		}
	}

	const AudioVoiceHandle handle = AllocateVoiceHandle();
	if (!handle) {
		Logger::Log("Audio::Play exhausted the voice handle space.");
		DestroyVoice(voice);
		return {};
	}
	voices_.emplace(handle.value, std::move(voice));
	return handle;
}

bool Audio::StopVoice(AudioVoiceHandle voiceHandle)
{
	if (!initialized_ || !IsOwnerThread("Audio::StopVoice")) {
		return false;
	}
	const auto iterator = voices_.find(voiceHandle.value);
	if (iterator == voices_.end()) {
		return false;
	}
	DestroyVoice(iterator->second);
	voices_.erase(iterator);
	return true;
}

bool Audio::PauseVoice(AudioVoiceHandle voiceHandle)
{
	Voice* voice = FindVoice(voiceHandle);
	if (!voice || voice->paused) {
		return voice != nullptr;
	}
	const HRESULT result = voice->sourceVoice->Stop();
	if (FAILED(result)) {
		LogHResult("IXAudio2SourceVoice::Stop", result);
		return false;
	}
	voice->paused = true;
	return true;
}

bool Audio::ResumeVoice(AudioVoiceHandle voiceHandle)
{
	Voice* voice = FindVoice(voiceHandle);
	if (!voice || !voice->paused) {
		return voice != nullptr;
	}
	const HRESULT result = voice->sourceVoice->Start();
	if (FAILED(result)) {
		LogHResult("IXAudio2SourceVoice::Start", result);
		return false;
	}
	voice->paused = false;
	return true;
}

bool Audio::SetVoiceVolume(AudioVoiceHandle voiceHandle, float volume)
{
	if (!std::isfinite(volume) || volume < 0.0f) {
		return false;
	}
	Voice* voice = FindVoice(voiceHandle);
	if (!voice) {
		return false;
	}
	const float clampedVolume = ClampVolume(volume);
	const HRESULT result = voice->sourceVoice->SetVolume(clampedVolume);
	if (FAILED(result)) {
		LogHResult("IXAudio2SourceVoice::SetVolume", result);
		return false;
	}
	voice->volume = clampedVolume;
	return true;
}

bool Audio::SetVoicePlaybackRate(
	AudioVoiceHandle voiceHandle,
	float playbackRate,
	float transitionSeconds)
{
	if (!std::isfinite(playbackRate) || playbackRate <= 0.0f ||
		!std::isfinite(transitionSeconds) || transitionSeconds < 0.0f) {
		return false;
	}
	Voice* voice = FindVoice(voiceHandle);
	if (!voice) {
		return false;
	}

	const float clampedRate = ClampPlaybackRate(playbackRate);
	if (NearlyEqual(voice->targetBasePlaybackRate, clampedRate) &&
		(voice->baseRateRampDuration > 0.0f || NearlyEqual(voice->currentBasePlaybackRate, clampedRate))) {
		return true;
	}

	voice->targetBasePlaybackRate = clampedRate;
	if (transitionSeconds <= 0.0f) {
		voice->currentBasePlaybackRate = clampedRate;
		voice->baseRateRampStart = clampedRate;
		voice->baseRateRampDuration = 0.0f;
		voice->baseRateRampElapsed = 0.0f;
		return ApplyVoiceFrequencyRatio(*voice);
	}

	voice->baseRateRampStart = voice->currentBasePlaybackRate;
	voice->baseRateRampDuration = transitionSeconds;
	voice->baseRateRampElapsed = 0.0f;
	return true;
}

bool Audio::SetVoiceDirection(
	AudioVoiceHandle voiceHandle,
	AudioPlaybackDirection direction)
{
	Voice* voice = FindVoice(voiceHandle);
	if (!voice) {
		return false;
	}
	const auto clipIterator = clips_.find(voice->clipHandle.value);
	if (clipIterator == clips_.end()) {
		return false;
	}

	const AudioPlaybackDirection desiredDirection =
		temporalDirection_ == AudioPlaybackDirection::Forward
		? direction
		: OppositeDirection(direction);
	if (desiredDirection != voice->appliedDirection) {
		const uint32_t originalFrame = GetCurrentOriginalFrame(*voice, clipIterator->second);
		if (!ResubmitVoice(*voice, clipIterator->second, desiredDirection, originalFrame)) {
			return false;
		}
		if (!voice->paused) {
			const HRESULT result = voice->sourceVoice->Start();
			if (FAILED(result)) {
				LogHResult("IXAudio2SourceVoice::Start", result);
				return false;
			}
		}
	}
	voice->baseDirection = direction;
	return true;
}

bool Audio::IsVoiceActive(AudioVoiceHandle voiceHandle) const
{
	if (!initialized_ || !IsOwnerThread("Audio::IsVoiceActive")) {
		return false;
	}
	return voices_.contains(voiceHandle.value);
}

void Audio::StopAllVoices()
{
	if (!initialized_ || !IsOwnerThread("Audio::StopAllVoices")) {
		return;
	}
	for (auto& [voiceId, voice] : voices_) {
		(void)voiceId;
		DestroyVoice(voice);
	}
	voices_.clear();
}

void Audio::SetGlobalTemporalState(
	AudioPlaybackDirection direction,
	float playbackRate,
	float transitionSeconds)
{
	if (!initialized_ || !IsOwnerThread("Audio::SetGlobalTemporalState")) {
		return;
	}
	if (!std::isfinite(playbackRate) || playbackRate <= 0.0f ||
		!std::isfinite(transitionSeconds) || transitionSeconds < 0.0f) {
		Logger::Log("Audio::SetGlobalTemporalState rejected invalid settings.");
		return;
	}

	if (direction != temporalDirection_) {
		for (auto& [voiceId, voice] : voices_) {
			(void)voiceId;
			const auto clipIterator = clips_.find(voice.clipHandle.value);
			if (clipIterator == clips_.end()) {
				continue;
			}
			const AudioPlaybackDirection desiredDirection =
				direction == AudioPlaybackDirection::Forward
				? voice.baseDirection
				: OppositeDirection(voice.baseDirection);
			const uint32_t originalFrame = GetCurrentOriginalFrame(voice, clipIterator->second);
			if (!ResubmitVoice(voice, clipIterator->second, desiredDirection, originalFrame)) {
				continue;
			}
			if (!voice.paused) {
				const HRESULT result = voice.sourceVoice->Start();
				if (FAILED(result)) {
					LogHResult("IXAudio2SourceVoice::Start", result);
				}
			}
		}
		temporalDirection_ = direction;
	}

	const float clampedRate = ClampPlaybackRate(playbackRate);
	if (NearlyEqual(targetTemporalPlaybackRate_, clampedRate) &&
		(temporalRateRampDuration_ > 0.0f || NearlyEqual(currentTemporalPlaybackRate_, clampedRate))) {
		return;
	}

	targetTemporalPlaybackRate_ = clampedRate;
	if (transitionSeconds <= 0.0f) {
		currentTemporalPlaybackRate_ = clampedRate;
		temporalRateRampStart_ = clampedRate;
		temporalRateRampDuration_ = 0.0f;
		temporalRateRampElapsed_ = 0.0f;
		for (auto& [voiceId, voice] : voices_) {
			(void)voiceId;
			(void)ApplyVoiceFrequencyRatio(voice);
		}
		return;
	}

	temporalRateRampStart_ = currentTemporalPlaybackRate_;
	temporalRateRampDuration_ = transitionSeconds;
	temporalRateRampElapsed_ = 0.0f;
}

Audio::Statistics Audio::GetStatistics() const
{
	Statistics statistics{};
	if (!initialized_ || !IsOwnerThread("Audio::GetStatistics")) {
		return statistics;
	}
	statistics.loadedClipCount = clips_.size();
	statistics.activeVoiceCount = voices_.size();
	statistics.temporalPlaybackRate = currentTemporalPlaybackRate_;
	statistics.targetTemporalPlaybackRate = targetTemporalPlaybackRate_;
	statistics.temporalDirection = temporalDirection_;
	for (const auto& [clipId, clip] : clips_) {
		(void)clipId;
		statistics.decodedPcmBytes += clip.forwardPcm.size();
		statistics.decodedPcmBytes += clip.reversePcm.size();
	}
	return statistics;
}

bool Audio::IsOwnerThread(const char* operation) const
{
	if (ownerThread_ == std::this_thread::get_id()) {
		return true;
	}
	Logger::Log(std::string(operation) + " must be called from the Audio owner thread.");
	return false;
}

bool Audio::DecodeAudioFile(const std::wstring& filename, Clip& outputClip) const
{
	loading::Scope timing(loading::Stage::AudioDecode);
	Microsoft::WRL::ComPtr<IMFSourceReader> sourceReader;
	HRESULT result = MFCreateSourceReaderFromURL(filename.c_str(), nullptr, sourceReader.GetAddressOf());
	if (FAILED(result)) {
		LogHResult("MFCreateSourceReaderFromURL", result);
		return false;
	}

	result = sourceReader->SetStreamSelection(
		static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS),
		FALSE);
	if (FAILED(result)) {
		LogHResult("IMFSourceReader::SetStreamSelection(all)", result);
		return false;
	}
	result = sourceReader->SetStreamSelection(
		static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
		TRUE);
	if (FAILED(result)) {
		LogHResult("IMFSourceReader::SetStreamSelection(audio)", result);
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaType> requestedType;
	result = MFCreateMediaType(requestedType.GetAddressOf());
	if (FAILED(result)) {
		LogHResult("MFCreateMediaType", result);
		return false;
	}
	if (FAILED(result = requestedType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
		FAILED(result = requestedType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
		FAILED(result = sourceReader->SetCurrentMediaType(
			static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
			nullptr,
			requestedType.Get()))) {
		LogHResult("IMFSourceReader::SetCurrentMediaType(PCM)", result);
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaType> outputType;
	result = sourceReader->GetCurrentMediaType(
		static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
		outputType.GetAddressOf());
	if (FAILED(result)) {
		LogHResult("IMFSourceReader::GetCurrentMediaType", result);
		return false;
	}

	WAVEFORMATEX* allocatedWaveFormat = nullptr;
	UINT32 waveFormatSize = 0;
	result = MFCreateWaveFormatExFromMFMediaType(
		outputType.Get(),
		&allocatedWaveFormat,
		&waveFormatSize,
		MFWaveFormatExConvertFlag_Normal);
	if (FAILED(result)) {
		LogHResult("MFCreateWaveFormatExFromMFMediaType", result);
		return false;
	}
	if (allocatedWaveFormat == nullptr || waveFormatSize < sizeof(WAVEFORMATEX) ||
		waveFormatSize > sizeof(WAVEFORMATEXTENSIBLE) ||
		allocatedWaveFormat->nBlockAlign == 0) {
		CoTaskMemFree(allocatedWaveFormat);
		Logger::Log("Audio::DecodeAudioFile received an invalid PCM format.");
		return false;
	}

	const uint16_t blockAlignment = allocatedWaveFormat->nBlockAlign;
	std::memcpy(&outputClip.waveFormat, allocatedWaveFormat, waveFormatSize);
	CoTaskMemFree(allocatedWaveFormat);

	while (true) {
		DWORD streamFlags = 0;
		Microsoft::WRL::ComPtr<IMFSample> sample;
		result = sourceReader->ReadSample(
			static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
			0,
			nullptr,
			&streamFlags,
			nullptr,
			sample.GetAddressOf());
		if (FAILED(result)) {
			LogHResult("IMFSourceReader::ReadSample", result);
			return false;
		}
		if ((streamFlags & MF_SOURCE_READERF_ERROR) != 0) {
			Logger::Log("IMFSourceReader::ReadSample reported a stream error.");
			return false;
		}
		if ((streamFlags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0) {
			Logger::Log("Audio::DecodeAudioFile does not concatenate changing PCM formats.");
			return false;
		}

		if (sample != nullptr) {
			Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
			result = sample->ConvertToContiguousBuffer(buffer.GetAddressOf());
			if (FAILED(result)) {
				LogHResult("IMFSample::ConvertToContiguousBuffer", result);
				return false;
			}

			BYTE* sourceData = nullptr;
			DWORD sourceByteCount = 0;
			result = buffer->Lock(&sourceData, nullptr, &sourceByteCount);
			if (FAILED(result)) {
				LogHResult("IMFMediaBuffer::Lock", result);
				return false;
			}
			if (sourceByteCount > 0 && sourceData == nullptr) {
				(void)buffer->Unlock();
				Logger::Log("IMFMediaBuffer::Lock returned a null PCM buffer.");
				return false;
			}

			const std::size_t oldSize = outputClip.forwardPcm.size();
			const std::size_t newSize = oldSize + static_cast<std::size_t>(sourceByteCount);
			if (newSize < oldSize || newSize > static_cast<std::size_t>(XAUDIO2_MAX_BUFFER_BYTES)) {
				(void)buffer->Unlock();
				Logger::Log("Audio::DecodeAudioFile exceeded the XAudio2 in-memory buffer limit.");
				return false;
			}
			outputClip.forwardPcm.resize(newSize);
			if (sourceByteCount > 0) {
				std::memcpy(outputClip.forwardPcm.data() + oldSize, sourceData, sourceByteCount);
			}
			result = buffer->Unlock();
			if (FAILED(result)) {
				LogHResult("IMFMediaBuffer::Unlock", result);
				return false;
			}
		}

		if ((streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
			break;
		}
	}

	if (outputClip.forwardPcm.empty() || outputClip.forwardPcm.size() % blockAlignment != 0) {
		Logger::Log("Audio::DecodeAudioFile produced empty or misaligned PCM data.");
		return false;
	}

	const std::size_t frameCount = outputClip.forwardPcm.size() / blockAlignment;
	if (frameCount == 0 || frameCount > std::numeric_limits<uint32_t>::max()) {
		Logger::Log("Audio::DecodeAudioFile exceeded the supported sample-frame count.");
		return false;
	}
	outputClip.frameCount = static_cast<uint32_t>(frameCount);
	outputClip.reversePcm.resize(outputClip.forwardPcm.size());
	for (std::size_t destinationFrame = 0; destinationFrame < frameCount; ++destinationFrame) {
		const std::size_t sourceFrame = frameCount - 1 - destinationFrame;
		std::memcpy(
			outputClip.reversePcm.data() + destinationFrame * blockAlignment,
			outputClip.forwardPcm.data() + sourceFrame * blockAlignment,
			blockAlignment);
	}
	return true;
}

std::wstring Audio::ResolveCanonicalPath(const std::string& filename) const
{
	const std::wstring wideFilename = Utf8ToWide(filename);
	if (wideFilename.empty()) {
		return {};
	}

	std::error_code error;
	std::filesystem::path path(wideFilename);
	if (!path.is_absolute()) {
		path = std::filesystem::absolute(path, error);
		if (error) {
			return {};
		}
	}
	if (!std::filesystem::is_regular_file(path, error) || error) {
		return {};
	}

	const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, error);
	return error ? path.lexically_normal().wstring() : canonicalPath.wstring();
}

AudioClipHandle Audio::AllocateClipHandle()
{
	if (clips_.size() >= static_cast<std::size_t>(std::numeric_limits<uint32_t>::max() - 1u)) {
		return {};
	}
	while (true) {
		const uint32_t candidate = nextClipId_++;
		if (nextClipId_ == AudioClipHandle::kInvalidValue) {
			nextClipId_ = 1;
		}
		if (candidate != AudioClipHandle::kInvalidValue && !clips_.contains(candidate)) {
			return { candidate };
		}
	}
}

AudioVoiceHandle Audio::AllocateVoiceHandle()
{
	if (voices_.size() >= static_cast<std::size_t>(std::numeric_limits<uint32_t>::max() - 1u)) {
		return {};
	}
	while (true) {
		const uint32_t candidate = nextVoiceId_++;
		if (nextVoiceId_ == AudioVoiceHandle::kInvalidValue) {
			nextVoiceId_ = 1;
		}
		if (candidate != AudioVoiceHandle::kInvalidValue && !voices_.contains(candidate)) {
			return { candidate };
		}
	}
}

Audio::Voice* Audio::FindVoice(AudioVoiceHandle voiceHandle)
{
	if (!initialized_ || !IsOwnerThread("Audio voice operation") || !voiceHandle) {
		return nullptr;
	}
	const auto iterator = voices_.find(voiceHandle.value);
	return iterator == voices_.end() ? nullptr : &iterator->second;
}

const Audio::Voice* Audio::FindVoice(AudioVoiceHandle voiceHandle) const
{
	if (!initialized_ || !IsOwnerThread("Audio voice query") || !voiceHandle) {
		return nullptr;
	}
	const auto iterator = voices_.find(voiceHandle.value);
	return iterator == voices_.end() ? nullptr : &iterator->second;
}

AudioPlaybackDirection Audio::ResolveAppliedDirection(const Voice& voice) const
{
	return temporalDirection_ == AudioPlaybackDirection::Forward
		? voice.baseDirection
		: OppositeDirection(voice.baseDirection);
}

uint32_t Audio::GetCurrentOriginalFrame(const Voice& voice, const Clip& clip) const
{
	if (voice.sourceVoice == nullptr || clip.frameCount == 0) {
		return 0;
	}

	XAUDIO2_VOICE_STATE state{};
	voice.sourceVoice->GetState(&state);
	const uint64_t elapsedFrames = state.SamplesPlayed >= voice.samplesPlayedAtSubmit
		? state.SamplesPlayed - voice.samplesPlayedAtSubmit
		: 0;
	uint64_t directionFrame = static_cast<uint64_t>(voice.submittedDirectionFrame) + elapsedFrames;
	if (voice.loop) {
		directionFrame %= clip.frameCount;
	} else {
		directionFrame = (std::min)(directionFrame, static_cast<uint64_t>(clip.frameCount - 1));
	}

	const uint32_t frame = static_cast<uint32_t>(directionFrame);
	return voice.appliedDirection == AudioPlaybackDirection::Forward
		? frame
		: clip.frameCount - 1 - frame;
}

bool Audio::ResubmitVoice(
	Voice& voice,
	const Clip& clip,
	AudioPlaybackDirection direction,
	uint32_t originalFrame)
{
	if (voice.sourceVoice == nullptr || clip.frameCount == 0) {
		return false;
	}

	HRESULT result = voice.sourceVoice->Stop();
	if (FAILED(result)) {
		LogHResult("IXAudio2SourceVoice::Stop", result);
		return false;
	}
	result = voice.sourceVoice->FlushSourceBuffers();
	if (FAILED(result)) {
		LogHResult("IXAudio2SourceVoice::FlushSourceBuffers", result);
		return false;
	}

	originalFrame %= clip.frameCount;
	const uint32_t directionFrame = direction == AudioPlaybackDirection::Forward
		? originalFrame
		: clip.frameCount - 1 - originalFrame;
	const std::vector<uint8_t>& pcm = direction == AudioPlaybackDirection::Forward
		? clip.forwardPcm
		: clip.reversePcm;

	if (voice.loop && directionFrame != 0) {
		XAUDIO2_BUFFER tailBuffer{};
		tailBuffer.AudioBytes = static_cast<UINT32>(pcm.size());
		tailBuffer.pAudioData = pcm.data();
		tailBuffer.PlayBegin = directionFrame;
		result = voice.sourceVoice->SubmitSourceBuffer(&tailBuffer);
		if (FAILED(result)) {
			LogHResult("IXAudio2SourceVoice::SubmitSourceBuffer(tail)", result);
			return false;
		}

		XAUDIO2_BUFFER loopBuffer{};
		loopBuffer.Flags = XAUDIO2_END_OF_STREAM;
		loopBuffer.AudioBytes = static_cast<UINT32>(pcm.size());
		loopBuffer.pAudioData = pcm.data();
		loopBuffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		result = voice.sourceVoice->SubmitSourceBuffer(&loopBuffer);
	} else {
		XAUDIO2_BUFFER buffer{};
		buffer.Flags = XAUDIO2_END_OF_STREAM;
		buffer.AudioBytes = static_cast<UINT32>(pcm.size());
		buffer.pAudioData = pcm.data();
		buffer.PlayBegin = directionFrame;
		buffer.LoopCount = voice.loop ? XAUDIO2_LOOP_INFINITE : 0;
		result = voice.sourceVoice->SubmitSourceBuffer(&buffer);
	}

	if (FAILED(result)) {
		LogHResult("IXAudio2SourceVoice::SubmitSourceBuffer", result);
		(void)voice.sourceVoice->FlushSourceBuffers();
		return false;
	}

	XAUDIO2_VOICE_STATE state{};
	voice.sourceVoice->GetState(&state);
	voice.appliedDirection = direction;
	voice.submittedDirectionFrame = directionFrame;
	voice.samplesPlayedAtSubmit = state.SamplesPlayed;
	return true;
}

bool Audio::ApplyVoiceFrequencyRatio(Voice& voice) const
{
	if (voice.sourceVoice == nullptr) {
		return false;
	}
	const float frequencyRatio = ClampPlaybackRate(
		voice.currentBasePlaybackRate * currentTemporalPlaybackRate_);
	const HRESULT result = voice.sourceVoice->SetFrequencyRatio(frequencyRatio);
	if (FAILED(result)) {
		LogHResult("IXAudio2SourceVoice::SetFrequencyRatio", result);
		return false;
	}
	return true;
}

void Audio::DestroyVoice(Voice& voice) noexcept
{
	if (voice.sourceVoice == nullptr) {
		return;
	}
	(void)voice.sourceVoice->Stop();
	(void)voice.sourceVoice->FlushSourceBuffers();
	voice.sourceVoice->DestroyVoice();
	voice.sourceVoice = nullptr;
}

void Audio::UpdateTemporalRate(float realDeltaSeconds)
{
	if (temporalRateRampDuration_ <= 0.0f) {
		currentTemporalPlaybackRate_ = targetTemporalPlaybackRate_;
		return;
	}

	temporalRateRampElapsed_ = (std::min)(
		temporalRateRampElapsed_ + realDeltaSeconds,
		temporalRateRampDuration_);
	const float interpolation = temporalRateRampElapsed_ / temporalRateRampDuration_;
	currentTemporalPlaybackRate_ = temporalRateRampStart_ +
		(targetTemporalPlaybackRate_ - temporalRateRampStart_) * interpolation;
	if (temporalRateRampElapsed_ >= temporalRateRampDuration_) {
		currentTemporalPlaybackRate_ = targetTemporalPlaybackRate_;
		temporalRateRampDuration_ = 0.0f;
		temporalRateRampElapsed_ = 0.0f;
	}
}

void Audio::UpdateBaseRate(Voice& voice, float realDeltaSeconds) const
{
	if (voice.baseRateRampDuration <= 0.0f) {
		voice.currentBasePlaybackRate = voice.targetBasePlaybackRate;
		return;
	}

	voice.baseRateRampElapsed = (std::min)(
		voice.baseRateRampElapsed + realDeltaSeconds,
		voice.baseRateRampDuration);
	const float interpolation = voice.baseRateRampElapsed / voice.baseRateRampDuration;
	voice.currentBasePlaybackRate = voice.baseRateRampStart +
		(voice.targetBasePlaybackRate - voice.baseRateRampStart) * interpolation;
	if (voice.baseRateRampElapsed >= voice.baseRateRampDuration) {
		voice.currentBasePlaybackRate = voice.targetBasePlaybackRate;
		voice.baseRateRampDuration = 0.0f;
		voice.baseRateRampElapsed = 0.0f;
	}
}
