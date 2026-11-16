param(
    [Parameter(Mandatory)][string]$ScenarioRoot,
    [string]$OutputDirectory = 'generated/exhibitions/game_system_exhibition_20261111'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$target = [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
$scenarios = (Resolve-Path -LiteralPath $ScenarioRoot).Path
foreach ($path in @($target, $scenarios)) {
    if (!$path.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Path must stay in workspace' }
    $parent = $path
    while ($parent) {
        if ((Test-Path -LiteralPath $parent) -and ((Get-Item -LiteralPath $parent).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Linked path' }
        $parent = Split-Path -Parent $parent
    }
}
if (Test-Path -LiteralPath $target) { throw 'Refusing to replace an existing exhibition' }
if (Get-ChildItem -LiteralPath $scenarios -Recurse -Force -Attributes ReparsePoint) { throw 'Linked scenario' }
foreach ($name in @('01_water','02_pests','03_rain','04_expansion','05_judging','06_recovery')) {
    if (!(Test-Path -LiteralPath (Join-Path $scenarios "$name/farm/farm_documents.json"))) { throw "Missing scenario: $name" }
}
New-Item -ItemType Directory -Path $target | Out-Null
$resourceRoot = Join-Path $root 'project/Resources'
$extensions = @('.obj','.png','.json','.txt','.wav','.hlsl','.hlsli')
$assets = foreach ($folder in @('farm','ui','shader','generated','title','transition')) {
    Get-ChildItem -LiteralPath (Join-Path $resourceRoot $folder) -File -Recurse | Where-Object { $_.Extension -in $extensions }
}
$assets += Get-Item -LiteralPath (Join-Path $resourceRoot 'text_textures.json'), (Join-Path $resourceRoot 'levels/farm_scene.json')
$assets += Get-ChildItem -LiteralPath (Join-Path $resourceRoot 'fonts') -File
foreach ($asset in $assets) {
    $dest = Join-Path $target ('Resources/' + [IO.Path]::GetRelativePath($resourceRoot, $asset.FullName))
    New-Item -ItemType Directory -Path (Split-Path $dest) -Force | Out-Null
    Copy-Item -LiteralPath $asset.FullName -Destination $dest
}
$release = Join-Path $root 'generated/outputs/Release'
Copy-Item -LiteralPath (Join-Path $release 'CG2_00_01.exe') -Destination (Join-Path $target 'SuiroNogyo.exe')
foreach ($name in @('dxcompiler.dll','dxil.dll')) { Copy-Item -LiteralPath (Join-Path $release $name) -Destination $target }
Get-ChildItem -LiteralPath $release -File -Filter '*.cso' | Copy-Item -Destination $target
$licenses = Join-Path $root 'generated/codex_checks/contest_finish_20261027/review/Licenses'
Copy-Item -LiteralPath $licenses -Destination $target -Recurse
Copy-Item -LiteralPath $scenarios -Destination (Join-Path $target 'Scenarios') -Recurse
New-Item -ItemType Directory -Path (Join-Path $target 'Settings') | Out-Null
Copy-Item -LiteralPath (Join-Path $scenarios '01_water/farm') -Destination (Join-Path $target 'Settings') -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'system_exhibition/StartExhibition.cmd') -Destination $target
foreach ($name in @('README.txt')) {
    $text = [IO.File]::ReadAllText((Join-Path $PSScriptRoot "system_exhibition/$name"))
    [IO.File]::WriteAllText((Join-Path $target $name), $text, [Text.UTF8Encoding]::new($true))
}
$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
& $compiler /nologo /target:winexe /optimize /warnaserror /codepage:65001 /reference:System.Windows.Forms.dll /reference:System.Drawing.dll ("/out:" + (Join-Path $target 'SuiroNogyoExhibition.exe')) (Join-Path $PSScriptRoot 'system_exhibition/ExhibitionLauncher.cs')
if ($LASTEXITCODE -ne 0) { throw 'Exhibition launcher build failed' }
$credits = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'exhibition/CREDITS.txt')).Replace('ゲームオーバー演出デモ版','ゲームシステム展示版')
$credits += "`r`n雨音・害虫駆除音: プロジェクト内の音声合成で生成。Resources/farm/weather/audio と pests/audio の manifest.json を参照。`r`n"
[IO.File]::WriteAllText((Join-Path $target 'CREDITS.txt'), $credits, [Text.UTF8Encoding]::new($true))
$files = @(Get-ChildItem -LiteralPath $target -File -Recurse)
if ($files.Extension -contains '.pdb' -or (Get-ChildItem -LiteralPath $target -Recurse -Attributes ReparsePoint)) { throw 'Unexpected runtime contents' }
$manifest = @($files | ForEach-Object {
    [pscustomobject]@{Path=[IO.Path]::GetRelativePath($target,$_.FullName); Bytes=$_.Length; SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($target + '_manifest.json') -Encoding utf8
[pscustomobject]@{Runtime=$target;Files=$files.Count;MiB=[Math]::Round(($files | Measure-Object Length -Sum).Sum/1MB,2);PersonalSavesCopied=0} | ConvertTo-Json
