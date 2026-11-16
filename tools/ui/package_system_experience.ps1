param(
    [Parameter(Mandatory)][string]$InitialFarm,
    [string]$Destination = 'generated/submissions/game_system_20261111',
    [string]$PackageName = 'LE3C_28_ヨシノ_ゲント_水路農業'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output = [IO.Path]::GetFullPath((Join-Path $root $Destination))
if (!$output.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Workspace destination only' }
if ([string]::IsNullOrWhiteSpace($PackageName) -or $PackageName -in @('.', '..') -or
    $PackageName.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) { throw 'Invalid package name' }
if (Test-Path -LiteralPath $output) { throw 'Refusing to replace an existing delivery' }
$runtime = Join-Path $output $PackageName
$verification = Join-Path $output 'verification'
$extract = Join-Path $verification 'extracted'
$farm = (Resolve-Path -LiteralPath $InitialFarm).Path
$preparedFiles = @(Get-ChildItem -LiteralPath $farm -Recurse -File)
if ($preparedFiles.Count -ne 2 -or !($preparedFiles | Where-Object Name -eq 'farm_documents.json')) {
    throw 'Expected only a prepared catalog and one synthetic farm'
}
$save = @($preparedFiles | Where-Object { $_.Directory.Name -eq 'saves' -and $_.Extension -eq '.json' })
if ($save.Count -ne 1 -or !([IO.File]::ReadAllText($save[0].FullName).Contains('FreeFarming'))) {
    throw 'Expected one prepared FreeFarming save'
}
& (Join-Path $PSScriptRoot 'prepare_system_experience.ps1') -InitialFarm $farm -OutputDirectory ([IO.Path]::GetRelativePath($root, $runtime))
$allowedSettings = @($preparedFiles | ForEach-Object { 'Settings/farm/' + [IO.Path]::GetRelativePath($farm, $_.FullName).Replace('\','/') })
$files = @(Get-ChildItem -LiteralPath $runtime -Recurse -File)
$manifest = @($files | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($runtime, $_.FullName).Replace('\','/')
    if ($relative -match '(^|/)(\.git|\.vs|\.codex|human)(/|$)' -or
        $relative -match '\.(pdb|ilk|dmp|log|sln|vcxproj|user|zip|mp4|lib)$') { throw "Unexpected file: $relative" }
    if ($relative.StartsWith('Settings/') -and $relative -notin $allowedSettings) { throw "Unexpected settings: $relative" }
    [pscustomobject]@{Path=$relative; Bytes=$_.Length; SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
if (Get-ChildItem -LiteralPath $runtime -Recurse -Attributes ReparsePoint) { throw 'Linked runtime file' }
foreach ($required in @('SuiroNogyo.exe','README.txt','CREDITS.txt','dxcompiler.dll','dxil.dll',
    'Licenses/DXC/LICENSE-MS.txt','Resources/fonts/NotoSansJP-VF.ttf','Resources/fonts/OFL.txt',
    'Resources/fonts/NotoSerifJP-OFL.txt','Resources/ui/farm_runtime_menu.png',
    'Resources/title/sky.png','Resources/shader/TitleSky.PS.hlsl','Resources/levels/farm_scene.json')) {
    if ($required -notin $manifest.Path) { throw "Missing dependency: $required" }
}
$releaseHash = (Get-FileHash -LiteralPath (Join-Path $root 'generated/outputs/Release/CG2_00_01.exe')).Hash
if (($manifest | Where-Object Path -eq 'SuiroNogyo.exe').SHA256 -ne $releaseHash) { throw 'Stale EXE' }
foreach ($file in $manifest | Where-Object { $_.Path.StartsWith('Resources/') }) {
    $source = Join-Path $root ('project/' + $file.Path)
    if ((Get-FileHash -LiteralPath $source).Hash -ne $file.SHA256) { throw "Changed resource: $($file.Path)" }
}
foreach ($file in $preparedFiles) {
    $dest = Join-Path $runtime ('Settings/farm/' + [IO.Path]::GetRelativePath($farm,$file.FullName))
    if ((Get-FileHash -LiteralPath $dest).Hash -ne (Get-FileHash -LiteralPath $file.FullName).Hash) { throw 'Prepared data changed' }
}
New-Item -ItemType Directory -Path $verification | Out-Null
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
    if ((Get-FileHash -LiteralPath (Join-Path $extractedRuntime $file.Path)).Hash -ne $file.SHA256) { throw "Extraction mismatch: $($file.Path)" }
}
$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $verification 'runtime_manifest.json') -Encoding utf8
$report = [ordered]@{
    Runtime=$runtime; Zip=$zip; ExtractedRuntime=$extractedRuntime
    Files=$manifest.Count; RuntimeBytes=($files | Measure-Object Length -Sum).Sum
    ZipBytes=(Get-Item -LiteralPath $zip).Length; ZipSHA256=(Get-FileHash -LiteralPath $zip).Hash
    ExeSHA256=$releaseHash; Extraction='All paths, counts and SHA256 values match'
    InitialMode='FreeFarming'; PreparedSaves=1; PersonalSavesCopied=0
    NativeSmoke='Pending'; Video='Pending'; OtherPC='Not tested'; Upload='Not performed'
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $verification 'package_report.json') -Encoding utf8
$report | ConvertTo-Json -Depth 4
