param(
    [string]$Destination = 'generated/exhibitions/gameover_demo_submission_20261028',
    [string]$PackageName = 'LE3C_28_ヨシノ_ゲント_水路農業'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output = [IO.Path]::GetFullPath((Join-Path $root $Destination))
if (!$output.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Destination must stay within this workspace'
}
if ([string]::IsNullOrWhiteSpace($PackageName) -or $PackageName -in @('.', '..') -or
    $PackageName.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) {
    throw 'PackageName must be a single valid folder name'
}
if (Test-Path -LiteralPath $output) { throw 'Refusing to replace an existing delivery' }
$runtime = Join-Path $output $PackageName
$verification = Join-Path $output 'verification'
$extract = Join-Path $verification 'extracted'
& (Join-Path $PSScriptRoot 'prepare_contest_exhibition.ps1') -OutputDirectory ([IO.Path]::GetRelativePath($root, $runtime))

$files = @(Get-ChildItem -LiteralPath $runtime -Recurse -File)
$manifest = @($files | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($runtime, $_.FullName).Replace('\', '/')
    if ($relative -match '(^|/)(\.git|\.vs|\.codex|human)(/|$)' -or
        $relative -match '\.(pdb|ilk|dmp|log|sln|vcxproj|user|zip|mp4)$') {
        throw "Unexpected runtime file: $relative"
    }
    # Resource OBJ files are runtime models, not disposable compiler objects.
    if ($relative.StartsWith('Settings/') -and $relative -ne 'Settings/runtime/contest_exhibition.json') {
        throw "Unexpected saved settings: $relative"
    }
    [pscustomobject]@{Path=$relative; Bytes=$_.Length; SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
if (Get-ChildItem -LiteralPath $runtime -Recurse -Attributes ReparsePoint) { throw 'Runtime contains a reparse point' }
foreach ($required in @('SuiroNogyoGameOverDemo.exe','StartGameOverDemo.cmd','README.txt','CREDITS.txt',
    'dxcompiler.dll','dxil.dll','Licenses/DXC/LICENSE-MS.txt','Resources/fonts/OFL.txt',
    'Resources/exhibition/failed.json','Resources/exhibition/passed.json','Resources/exhibition/missed.json',
    'Resources/title/sky.png','Resources/shader/TitleSky.VS.hlsl','Resources/shader/TitleSky.PS.hlsl',
    'Resources/shader/TitleSky.hlsli','Resources/ui/farm_runtime_menu.png',
    'Resources/farm/contest/ceremony/manifest.json','Resources/farm/contest/ceremony/venue.obj',
    'Resources/farm/contest/ceremony/palette.png',
    'Settings/runtime/contest_exhibition.json')) {
    if ($required -notin $manifest.Path) { throw "Missing runtime dependency: $required" }
}
$releaseHash = (Get-FileHash -LiteralPath (Join-Path $root 'generated/outputs/Release/CG2_00_01.exe')).Hash
if (($manifest | Where-Object Path -eq 'SuiroNogyoGameOverDemo.exe').SHA256 -ne $releaseHash) {
    throw 'Packaged EXE does not match current Release'
}
foreach ($file in $manifest | Where-Object { $_.Path.StartsWith('Resources/') }) {
    $source = Join-Path $root ('project/' + $file.Path)
    if ((Test-Path -LiteralPath $source -PathType Leaf) -and
        (Get-FileHash -LiteralPath $source).Hash -ne $file.SHA256) {
        throw "Resource does not match current source: $($file.Path)"
    }
}
$config = Get-Content -LiteralPath (Join-Path $runtime 'Settings/runtime/contest_exhibition.json') -Raw | ConvertFrom-Json
if ($config.enabled -ne $true) { throw 'Exhibition startup must be enabled' }
New-Item -ItemType Directory -Path $verification | Out-Null
$zip = Join-Path $output ($PackageName + '.zip')
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($runtime, $zip, [IO.Compression.CompressionLevel]::Optimal, $true, [Text.Encoding]::UTF8)
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $archive.Entries) {
        $name = $entry.FullName.Replace('\', '/')
        if (!$name.StartsWith($PackageName + '/', [StringComparison]::Ordinal) -or
            $name -match '(^|/)\.\.(/|$)|:|^/' -or !$names.Add($name)) {
            throw "Unsafe or duplicate ZIP entry: $name"
        }
    }
    if ($archive.Entries.Count -ne $manifest.Count) { throw 'ZIP file count mismatch' }
} finally { $archive.Dispose() }
[IO.Compression.ZipFile]::ExtractToDirectory($zip, $extract, [Text.Encoding]::UTF8)
$extractedRuntime = Join-Path $extract $PackageName
$extractedFiles = @(Get-ChildItem -LiteralPath $extractedRuntime -Recurse -File)
if ($extractedFiles.Count -ne $manifest.Count) { throw 'Extracted file count mismatch' }
foreach ($file in $manifest) {
    $path = Join-Path $extractedRuntime $file.Path
    if (!(Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-FileHash -LiteralPath $path).Hash -ne $file.SHA256) { throw "Extraction mismatch: $($file.Path)" }
}
$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $verification 'runtime_manifest.json') -Encoding utf8
$report = [ordered]@{
    Runtime=$runtime; Zip=$zip; ExtractedRuntime=$extractedRuntime
    Files=$manifest.Count; RuntimeBytes=($files | Measure-Object Length -Sum).Sum
    ZipBytes=(Get-Item -LiteralPath $zip).Length; ZipSHA256=(Get-FileHash -LiteralPath $zip).Hash
    ExeSHA256=$releaseHash; Extraction='All file paths, counts and SHA256 values match'
    PersonalSavesCopied=0; NativeSmoke='Pending'; Video='Pending'; OtherPC='Not tested'; Upload='Not performed'
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $verification 'package_report.json') -Encoding utf8
$report | ConvertTo-Json -Depth 4
