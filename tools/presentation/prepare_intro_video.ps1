$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$source = 'C:/Users/K024G/Videos/Captures/水路農業 2026-11-06 08-10-57.mp4'
$out = Join-Path $root 'generated/ui_intro_video_20261106'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$destination = Join-Path $out 'ゲーム紹介_28秒.mp4'
$ranges = @(@(0,5), @(91,99), @(110,119), @(141,147))
$filters = @()
for ($i=0; $i -lt $ranges.Count; $i++) {
    $start=$ranges[$i][0]; $end=$ranges[$i][1]
    $filters += "[0:v]trim=start=${start}:end=${end},setpts=PTS-STARTPTS,fps=30[v$i]"
    $filters += "[0:a]atrim=start=${start}:end=${end},asetpts=PTS-STARTPTS,afade=t=in:d=0.05,areverse,afade=t=in:d=0.05,areverse[a$i]"
}
$filters += '[v0][a0][v1][a1][v2][a2][v3][a3]concat=n=4:v=1:a=1[v][a]'
& ffmpeg -hide_banner -loglevel error -y -i $source -filter_complex ($filters -join ';') -map '[v]' -map '[a]' -c:v libx264 -preset medium -crf 23 -pix_fmt yuv420p -c:a aac -b:a 96k -movflags +faststart $destination
if ($LASTEXITCODE -ne 0) { throw 'Introduction encode failed' }
& ffmpeg -hide_banner -loglevel error -y -ss 16 -i $destination -frames:v 1 (Join-Path $out 'poster.png')
if ($LASTEXITCODE -ne 0) { throw 'Poster encode failed' }
& ffmpeg -hide_banner -loglevel error -y -i $destination -vf 'fps=1/3.5,scale=480:-1,tile=4x2' -frames:v 1 (Join-Path $out 'final-contact.png')
if ($LASTEXITCODE -ne 0) { throw 'Contact encode failed' }
$metadata = & ffprobe -v error -show_entries format=duration,size:stream=codec_name,width,height,r_frame_rate -of json $destination | ConvertFrom-Json
if ([Math]::Abs([double]$metadata.format.duration-28) -gt 0.2) { throw 'Unexpected duration' }
$metadata | ConvertTo-Json -Depth 6
