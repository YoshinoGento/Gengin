param([string]$OutputPath = 'generated/exhibitions/gameover_demo_submission_20261028/verification/edit_01.mp4')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$work = Join-Path $root 'generated/exhibitions/gameover_demo_submission_20261028/verification'
$output = [IO.Path]::GetFullPath((Join-Path $root $OutputPath))
if (!$output.StartsWith($work + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Render into this delivery verification directory first'
}
if (Test-Path -LiteralPath $output) { throw 'Refusing to replace a rendered video' }
$timeline = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'edit_timeline.json') -Raw | ConvertFrom-Json
$inputs = @()
foreach ($relative in $timeline.sources) {
    $source = [IO.Path]::GetFullPath((Join-Path $work $relative))
    if (!$source.StartsWith($work + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Capture must stay within this delivery'
    }
    $inputs += $source
}
$filters = [Collections.Generic.List[string]]::new()
$videoLabels = @{}
$audioLabels = @{}
for ($source = 0; $source -lt $inputs.Count; ++$source) {
    $meta = (& ffprobe -v error -show_streams -show_format -of json $inputs[$source]) | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or @($meta.streams | Where-Object codec_type -eq 'video').Count -ne 1 -or
        @($meta.streams | Where-Object codec_type -eq 'audio').Count -ne 1) { throw 'Capture needs one video and audio stream' }
    $uses = @(for ($i=0; $i -lt $timeline.segments.Count; ++$i) {
        if ($timeline.segments[$i].source -eq $source) { $i }
    })
    if (!$uses.Count) { throw 'Unused input' }
    foreach ($i in $uses) {
        $segment = $timeline.segments[$i]
        if ($segment.start -lt 0 -or $segment.duration -le .1 -or
            $segment.start+$segment.duration -gt [double]$meta.format.duration) { throw 'Invalid capture interval' }
        $videoLabels[$i] = "srcv$i"
        $audioLabels[$i] = "srca$i"
    }
    if ($uses.Count -eq 1) {
        $videoLabels[$uses[0]] = "${source}:v:0"
        $audioLabels[$uses[0]] = "${source}:a:0"
    } else {
        $filters.Add("[${source}:v:0]split=$($uses.Count)" + (($uses | ForEach-Object { "[srcv$_]" }) -join ''))
        $filters.Add("[${source}:a:0]asplit=$($uses.Count)" + (($uses | ForEach-Object { "[srca$_]" }) -join ''))
    }
}
$invariant = [Globalization.CultureInfo]::InvariantCulture
$chapters = [Collections.Generic.List[string]]::new()
$chapters.Add(';FFMETADATA1')
$chapters.Add('title=水路農業 ゲームオーバー演出デモ版')
$duration = 0.0
$join = ''
for ($i=0; $i -lt $timeline.segments.Count; ++$i) {
    $segment = $timeline.segments[$i]
    $start = ([double]$segment.start).ToString($invariant)
    $length = ([double]$segment.duration).ToString($invariant)
    $fadeOut = ([double]$segment.duration-.04).ToString($invariant)
    $filters.Add("[$($videoLabels[$i])]trim=start=${start}:duration=${length},setpts=PTS-STARTPTS,fps=30,setsar=1,format=yuv420p[v$i]")
    # Short edge fades suppress splice clicks without speeding up judging or inventing audio.
    $filters.Add("[$($audioLabels[$i])]atrim=start=${start}:duration=${length},asetpts=PTS-STARTPTS,afade=t=in:d=0.04,afade=t=out:st=${fadeOut}:d=0.04[a$i]")
    $join += "[v$i][a$i]"
    $chapters.Add('[CHAPTER]')
    $chapters.Add('TIMEBASE=1/1000')
    $chapters.Add("START=$([int]($duration*1000))")
    $duration += [double]$segment.duration
    $chapters.Add("END=$([int]($duration*1000))")
    $chapters.Add('title='+$segment.title)
}
$gain = [double]$timeline.audioGainDb
if (![double]::IsFinite($gain) -or $gain -lt 0 -or $gain -gt 24) { throw 'Invalid video-only audio gain' }
$filters.Add($join + "concat=n=$($timeline.segments.Count):v=1:a=1[video][rawAudio]")
$filters.Add('[rawAudio]volume='+$gain.ToString($invariant)+'dB[audio]')
$chapterPath = Join-Path $work 'chapters.ffmeta'
$chapters | Set-Content -LiteralPath $chapterPath -Encoding utf8NoBOM
$argsList = @('-nostdin','-hide_banner','-loglevel','warning')
foreach ($input in $inputs) { $argsList += @('-i',$input) }
$argsList += @('-i',$chapterPath,'-filter_complex',($filters -join ';'),'-map','[video]','-map','[audio]',
    '-map_metadata',"$($inputs.Count)",'-map_chapters',"$($inputs.Count)",'-c:v','libx264','-preset','medium',
    '-crf','19','-pix_fmt','yuv420p','-r','30','-c:a','aac','-b:a','128k','-movflags','+faststart',$output)
& ffmpeg @argsList 2> (Join-Path $work 'edit_encode.log')
if ($LASTEXITCODE -ne 0) { throw 'Video encoding failed' }
& ffmpeg -nostdin -v error -i $output -map 0:v:0 -map 0:a:0 -f null NUL 2> (Join-Path $work 'edit_decode.log')
if ($LASTEXITCODE -ne 0) { throw 'Final video decode failed' }
$final = (& ffprobe -v error -show_streams -show_format -show_chapters -of json $output) | ConvertFrom-Json
$video = @($final.streams | Where-Object codec_type -eq 'video')[0]
$audio = @($final.streams | Where-Object codec_type -eq 'audio')[0]
if ([math]::Abs([double]$final.format.duration-$duration) -gt .1 -or $video.width -ne 1280 -or
    $video.height -ne 720 -or $video.codec_name -ne 'h264' -or $audio.codec_name -ne 'aac' -or
    [int]$video.nb_frames -ne [int]($duration*30)) { throw 'Final video metadata mismatch' }
[ordered]@{Path=$output; Duration=$duration; Bytes=(Get-Item -LiteralPath $output).Length;
    SHA256=(Get-FileHash -LiteralPath $output).Hash; Metadata=$final;
    SourceTimeline=$timeline; Decode='PASS'; VisualReview='Pending'; AudioReview='Pending'} |
    ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $work 'video_report.json') -Encoding utf8
Write-Output "PASS encoded/decoded $duration seconds: $output"
