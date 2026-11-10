#include "farm/system/FarmLayoutSystem.h"
#include "externals/nlohmann/json.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
constexpr int kMaxDimension = 128;
constexpr std::size_t kMaxEntries = 256;
constexpr std::uintmax_t kMaxFileBytes = 1024 * 1024;
fs::path Utf8Path(const std::string& text) { return fs::path(std::u8string(text.begin(), text.end())); }

void Require(bool condition) { if (!condition) throw std::runtime_error("Invalid layout"); }
bool ValidName(const std::string& name) {
    if (name.empty() || name.size() > 192 || name.front() == ' ' || name.back() == ' ') return false;
    for (unsigned char c : name) if (c < 32 || c == 127) return false;
    // The JSON serializer rejects malformed UTF-8 before any file is written.
    try { static_cast<void>(Json(name).dump()); } catch (...) { return false; }
    return true;
}
bool ValidId(const std::string& id) {
    return !id.empty() && id.size() < 80 && std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= '0' && c <= '9') || c == '_' || (c >= 'a' && c <= 'z');
    });
}
int Integer(const Json& value, int low, int high) {
    Require(value.is_number_integer());
    Require(value >= low && value <= high);
    return value.get<int>();
}
Json Read(const fs::path& path) {
    Require(fs::is_regular_file(path) && fs::file_size(path) <= kMaxFileBytes);
    std::ifstream stream(path);
    Require(stream.is_open());
    Json json; stream >> json; return json;
}
farm::FarmGrid::Snapshot Decode(const Json& j) {
    Require(j.is_object() && j.at("kind") == "farm_layout");
    const int version = Integer(j.at("version"), 1, 3);
    Require(ValidName(j.at("name").get<std::string>()));
    farm::FarmGrid::Snapshot result;
    result.width = Integer(j.at("width"), 1, kMaxDimension);
    result.height = Integer(j.at("height"), 1, kMaxDimension);
    const auto& tiles = j.at("tiles");
    Require(tiles.is_array() && tiles.size() == static_cast<std::size_t>(result.width * result.height));
    result.tiles.reserve(tiles.size());
    for (const auto& value : tiles) {
        farm::FarmTile tile{};
        tile.heightLevel = Integer(value.at("height"), 0, 2);
        tile.feature = static_cast<farm::FarmTileFeature>(Integer(value.at("feature"), 0, 2));
        if (version >= 2) {
            Require(value.at("irrigationEnabled").is_boolean());
            tile.irrigationEnabled = value.at("irrigationEnabled").get<bool>();
        }
        Require(value.at("cultivated").is_boolean());
        const bool cultivated = value.at("cultivated").get<bool>();
        Require(!cultivated || tile.feature == farm::FarmTileFeature::None);
        tile.state = cultivated ? farm::FarmTileState::Tilled : farm::FarmTileState::Empty;
        if (version >= 3) {
            Require(value.at("repellentPlant").is_boolean());
            tile.repellentPlant = value.at("repellentPlant").get<bool>();
            Require(!tile.repellentPlant || tile.feature == farm::FarmTileFeature::None);
        }
        tile.waterAmount = tile.feature == farm::FarmTileFeature::WaterSource ? 1.0f : 0.0f;
        result.tiles.push_back(tile);
    }
    return result;
}
}

bool FarmLayoutSystem::Initialize(const std::string& directory) {
    directory_ = directory;
    try { fs::create_directories(Utf8Path(directory_)); return Refresh(); }
    catch (...) { error_ = "配置フォルダーを開けません。"; return false; }
}
bool FarmLayoutSystem::Refresh() {
    try {
        std::vector<FarmLayoutEntry> found;
        for (const auto& file : fs::directory_iterator(Utf8Path(directory_))) {
            if (file.path().extension() != ".json" || !file.is_regular_file()) continue;
            Require(found.size() < kMaxEntries);
            const auto id = file.path().stem().string();
            if (!ValidId(id)) continue;
            // Damaged entries cannot be selected; Load still validates again before applying.
            try {
                const auto json = Read(file.path());
                static_cast<void>(Decode(json));
                found.push_back({id, json.at("name").get<std::string>()});
            } catch (...) { continue; }
        }
        std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
        entries_ = std::move(found); error_.clear(); return true;
    } catch (...) { error_ = "配置一覧を読み込めません。"; return false; }
}
bool FarmLayoutSystem::SaveNew(const std::string& name, const farm::FarmGrid& grid) {
    if (!ValidName(name)) { error_ = "名前を入力してください（UTF-8で192バイト以内、前後の空白・改行は不可）。"; return false; }
    if (!Refresh()) return false;
    if (entries_.size() >= kMaxEntries) { error_ = "配置記録は256件までです。"; return false; }
    for (const auto& entry : entries_) if (entry.name == name) { error_ = "同じ名前の配置があります。別の名前にしてください。"; return false; }
    fs::path temporary;
    bool ownsTemporary = false;
    try {
        Json tiles = Json::array();
        for (int i = 0; i < grid.GetTileCount(); ++i) {
            const auto* tile = grid.GetTile(i); Require(tile != nullptr);
            tiles.push_back({{"height", tile->heightLevel}, {"feature", static_cast<int>(tile->feature)},
                {"irrigationEnabled", tile->irrigationEnabled},
                {"repellentPlant", tile->repellentPlant},
                {"cultivated", tile->feature == farm::FarmTileFeature::None && tile->state != farm::FarmTileState::Empty}});
        }
        const Json json{{"kind", "farm_layout"}, {"version", 3}, {"name", name},
            {"width", grid.GetWidth()}, {"height", grid.GetHeight()}, {"tiles", std::move(tiles)}};
        static_cast<void>(Decode(json));
        const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
        std::string id;
        fs::path path;
        for (int attempt = 0; attempt < 100; ++attempt) {
            id = "layout_" + std::to_string(stamp) + "_" + std::to_string(attempt);
            path = Utf8Path(directory_) / (id + ".json");
            temporary = Utf8Path(directory_) / (id + ".tmp");
            if (!fs::exists(path) && !fs::exists(temporary)) break;
            Require(attempt < 99);
        }
        const auto serialized = json.dump(2); Require(serialized.size() <= kMaxFileBytes);
        std::ofstream stream(temporary, std::ios::binary);
        Require(stream.is_open()); ownsTemporary = true;
        stream << serialized; stream.flush(); Require(stream.good()); stream.close(); Require(!stream.fail());
        fs::rename(temporary, path);
        entries_.push_back({id, name}); error_.clear(); return true;
    } catch (...) {
        std::error_code ignored; if (ownsTemporary) fs::remove(temporary, ignored);
        error_ = "配置を保存できません。ディスク容量・配置データを確認してください。"; return false;
    }
}
bool FarmLayoutSystem::Load(const std::string& id, farm::FarmGrid& grid, int maximumHeight) {
    if (!ValidId(id)) { error_ = "配置IDが不正です。"; return false; }
    try {
        auto snapshot = Decode(Read(Utf8Path(directory_) / (id + ".json")));
        Require(snapshot.width == grid.GetWidth() && snapshot.height == grid.GetHeight());
        const int selection = grid.GetSelectedIndex();
        snapshot.selectedX = selection >= 0 ? selection % snapshot.width : 0;
        snapshot.selectedY = selection >= 0 ? selection / snapshot.width : 0;
        // Validation completes before one atomic grid replacement; economy/date are not inputs.
        if (maximumHeight < 0 || maximumHeight > 2) { error_ = "地形の高さ上限が不正です。"; return false; }
        for (const auto& tile : snapshot.tiles) if (tile.heightLevel > maximumHeight) {
            error_ = "未解放の高さを含む配置です。現在の畑は変更していません。"; return false;
        }
        Require(grid.RestoreSnapshot(snapshot)); error_.clear(); return true;
    } catch (...) { error_ = "配置を読み込めません。形式・マス数・高さを確認してください。現在の畑は変更していません。"; return false; }
}
