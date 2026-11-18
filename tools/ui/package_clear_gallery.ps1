param(
    [string]$Destination = 'generated/submissions/clear_20261117',
    [string]$PackageName = 'LE3C_28_ヨシノ_ゲント_水路農業'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output = [IO.Path]::GetFullPath((Join-Path $root $Destination))
if (!$output.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Workspace destination only' }
if ([string]::IsNullOrWhiteSpace($PackageName) -or $PackageName -in @('.', '..') -or
    $PackageName.TrimEnd(' ', '.') -ne $PackageName -or
    $PackageName.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) { throw 'Invalid package name' }
$parent = $output
while ($parent) {
    if ((Test-Path -LiteralPath $parent) -and ((Get-Item -LiteralPath $parent).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Linked destination'
    }
    $parent = Split-Path -Parent $parent
}
if (Test-Path -LiteralPath $output) { throw 'Refusing to replace an existing submission' }
$runtime = Join-Path $output $PackageName
$verification = Join-Path $output 'verification'
$staged = Join-Path $verification 'staged'
$extract = Join-Path $verification 'extracted'

# Use only the prepared fixture and contract-checked Release, never the user's Settings.
& (Join-Path $PSScriptRoot 'prepare_contest_check.ps1') -Preview -ClearGallery `
    -OutputDirectory ([IO.Path]::GetRelativePath($root, $staged))
if (Get-ChildItem -LiteralPath $staged -Recurse -Attributes ReparsePoint) { throw 'Linked staged file' }
Copy-Item -LiteralPath $staged -Destination $runtime -Recurse
$oldLauncher = [IO.Path]::GetFullPath((Join-Path $runtime 'Start_Check.cmd'))
$newLauncher = [IO.Path]::GetFullPath((Join-Path $runtime 'StartGame.cmd'))
foreach ($path in @($oldLauncher,$newLauncher)) {
    if (!$path.StartsWith($runtime + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Launcher outside runtime' }
}
Move-Item -LiteralPath $oldLauncher -Destination $newLauncher
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'contest_check/README_Submission.txt') -Destination (Join-Path $runtime 'README.txt')
$creditsPath = Join-Path $runtime 'CREDITS.txt'
$credits = [IO.File]::ReadAllText($creditsPath).Replace('水路農業 / ゲームシステム体験版','水路農業 / クリア演出提出版')
[IO.File]::WriteAllText($creditsPath, $credits, [Text.UTF8Encoding]::new($true))

$fixture = Join-Path $root 'project/Resources/farm/experience/final_submission.json'
$data = Get-Content -LiteralPath $fixture -Raw -Encoding utf8 | ConvertFrom-Json
$allowedSettings = @('Settings/runtime/contest_exhibition.json','Settings/farm/farm_documents.json',
    ('Settings/farm/saves/' + $data.document.id + '.json'))
$files = @(Get-ChildItem -LiteralPath $runtime -Recurse -File)
$manifest = @($files | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($runtime,$_.FullName).Replace('\','/')
    if ($relative -match '(^|/)(\.git|\.vs|\.codex|human|__pycache__|site-packages)(/|$)' -or
        $relative -match '\.(pdb|ilk|dmp|log|sln|vcxproj|user|zip|mp4|lib|py|pyc|safetensors|pt|pth|onnx)$') {
        throw "Unexpected runtime file: $relative"
    }
    if ($relative.StartsWith('Settings/') -and $relative -notin $allowedSettings) { throw "Unexpected settings: $relative" }
    [pscustomobject]@{Path=$relative; Bytes=$_.Length; SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
foreach ($required in @('SuiroNogyo.exe','StartGame.cmd','README.txt','CREDITS.txt','dxcompiler.dll','dxil.dll',
    'Licenses/DXC/LICENSE-MS.txt','Resources/fonts/NotoSansJP-VF.ttf','Resources/fonts/OFL.txt',
    'Resources/fonts/NotoSerifJP-OFL.txt','Resources/ui/farm_runtime_menu.png',
    'Resources/title/sky.png','Resources/shader/TitleSky.PS.hlsl','Resources/levels/farm_scene.json',
    'Resources/exhibition/failed.json','Resources/exhibition/passed.json','Resources/exhibition/missed.json',
    'Resources/farm/contest/audio/cheers/manifest.json','Resources/farm/contest/audio/cheers/CREDITS.txt') + $allowedSettings) {
    if ($required -notin $manifest.Path) { throw "Missing dependency: $required" }
}
$releaseHash = (Get-FileHash -LiteralPath (Join-Path $root 'generated/outputs/Release/CG2_00_01.exe')).Hash
if (($manifest | Where-Object Path -eq 'SuiroNogyo.exe').SHA256 -ne $releaseHash) { throw 'Stale EXE' }
foreach ($file in $manifest | Where-Object { $_.Path.StartsWith('Resources/') }) {
    if ((Get-FileHash -LiteralPath (Join-Path $root ('project/' + $file.Path))).Hash -ne $file.SHA256) {
        throw "Resource mismatch: $($file.Path)"
    }
}
if ((Get-FileHash -LiteralPath (Join-Path $runtime $allowedSettings[2])).Hash -ne (Get-FileHash -LiteralPath $fixture).Hash) {
    throw 'Prepared fixture changed'
}
$config = Get-Content -LiteralPath (Join-Path $runtime $allowedSettings[0]) -Raw | ConvertFrom-Json
if ($config.enabled -ne $true -or $config.clearGallery -ne $true) { throw 'Clear gallery is not enabled' }

$zip = Join-Path $output ($PackageName + '.zip')
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($runtime,$zip,[IO.Compression.CompressionLevel]::Optimal,$true,[Text.Encoding]::UTF8)
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $archive.Entries) {
        $name = $entry.FullName.Replace('\','/')
        if (!$name.StartsWith($PackageName + '/', [StringComparison]::Ordinal) -or
            $name -match '(^|/)\.\.(/|$)|:|^/' -or !$names.Add($name)) { throw "Unsafe ZIP entry: $name" }
    }
    if ($archive.Entries.Count -ne $manifest.Count) { throw 'ZIP count mismatch' }
} finally { $archive.Dispose() }
[IO.Compression.ZipFile]::ExtractToDirectory($zip,$extract,[Text.Encoding]::UTF8)
$extractedRuntime = Join-Path $extract $PackageName
if (@(Get-ChildItem -LiteralPath $extractedRuntime -Recurse -File).Count -ne $manifest.Count) { throw 'Extraction count mismatch' }
foreach ($file in $manifest) {
    if ((Get-FileHash -LiteralPath (Join-Path $extractedRuntime $file.Path)).Hash -ne $file.SHA256) {
        throw "Extraction mismatch: $($file.Path)"
    }
}
$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $verification 'runtime_manifest.json') -Encoding utf8
$report = [ordered]@{
    Runtime=$runtime; Zip=$zip; ExtractedRuntime=$extractedRuntime
    Files=$manifest.Count; RuntimeBytes=($files | Measure-Object Length -Sum).Sum
    ZipBytes=(Get-Item -LiteralPath $zip).Length; ZipSHA256=(Get-FileHash -LiteralPath $zip).Hash
    ExeSHA256=$releaseHash; Extraction='All paths, counts and SHA256 values match'
    Mode='ClearGallery'; DialoguePairs=16; SpeechClips=32; PersonalSavesCopied=0
    NativeSmoke='Pending on extracted copy'; FullListening='Pending'; OtherPC='Not tested'
    Video='Not created for this packaging request'; Upload='Not performed'
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $verification 'package_report.json') -Encoding utf8
$guide = @"
提出するファイル

$PackageName.zip

今回の要素は「クリア演出」です。上記ZIPが実行用の提出物です。
ZIPをすべて展開し、StartGame.cmd を起動してください。
「はじめる」→16項目から選択すると、対応する野菜でクリア演出が始まります。
全32台詞の掛け合い、同じ演出の再生、別のセリフへの切替ができます。

同名の展開済みフォルダは、手元で確認するときに使えます。
verification は検証記録・作業用コピーです。提出する必要はありません。
今回は新しい提出動画は作成していません。
外部サイトへのアップロードや提出操作は行っていません。
"@
[IO.File]::WriteAllText((Join-Path $output '提出案内.txt'), $guide, [Text.UTF8Encoding]::new($true))
$report | ConvertTo-Json -Depth 4
