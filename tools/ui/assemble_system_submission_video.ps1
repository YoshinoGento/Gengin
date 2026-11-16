param(
    [Parameter(Mandatory)][string]$EditList,
    [Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$list = Get-Content -LiteralPath $EditList -Raw -Encoding UTF8 | ConvertFrom-Json
$dest = [IO.Path]::GetFullPath($Output)
if (!$dest.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Workspace output only' }
if (Test-Path -LiteralPath $dest) { throw 'Refusing to overwrite an existing video' }
$work = Join-Path (Split-Path $dest) 'verification/video_edit'
if (Test-Path -LiteralPath $work) { throw 'Use a fresh edit directory' }
New-Item -ItemType Directory -Path $work -Force | Out-Null
$font = Join-Path $root 'project/Resources/fonts/NotoSansJP-VF.ttf'
Copy-Item -LiteralPath $font -Destination (Join-Path $work 'font.ttf')
$parts = [Collections.Generic.List[string]]::new()
Push-Location $work
try {
    $index = 0
    foreach ($clip in $list.clips) {
        if ($clip.start -lt 0 -or $clip.duration -le 0 -or !(Test-Path -LiteralPath $clip.source -PathType Leaf)) {
            throw 'Invalid source or trim range'
        }
        $stem = 'part_{0:D2}' -f $index
        [IO.File]::WriteAllText((Join-Path $work ($stem + '.txt')), $clip.caption, [Text.UTF8Encoding]::new($false))
        # Keep all game pixels visible; captions occupy a separate top strip.
        $filter = "fps=30,scale=1760:990:flags=lanczos,pad=1920:1080:80:90:color=0x182422,drawtext=fontfile=font.ttf:textfile=$stem.txt:expansion=none:fontsize=34:fontcolor=white:x=80:y=25"
        & ffmpeg -hide_banner -loglevel warning -nostdin -ss $clip.start -i $clip.source -t $clip.duration -an -vf $filter -c:v libx264 -preset medium -crf 22 -pix_fmt yuv420p -movflags +faststart ($stem + '.mp4')
        if ($LASTEXITCODE -ne 0) { throw "Encoding failed: $stem" }
        $parts.Add("file '$stem.mp4'")
        $index++
    }
    [IO.File]::WriteAllLines((Join-Path $work 'concat.txt'),$parts,[Text.UTF8Encoding]::new($false))
    & ffmpeg -hide_banner -loglevel warning -nostdin -f concat -safe 0 -i concat.txt -c copy -movflags +faststart $dest
    if ($LASTEXITCODE -ne 0) { throw 'Concat failed' }
    & ffmpeg -hide_banner -loglevel error -nostdin -i $dest -f null -
    if ($LASTEXITCODE -ne 0) { throw 'Full decode failed' }
    & ffprobe -v error -show_entries 'format=duration,size:stream=codec_name,width,height,avg_frame_rate' -of json $dest
} finally { Pop-Location }
