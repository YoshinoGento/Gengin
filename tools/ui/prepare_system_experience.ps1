param(
    [Parameter(Mandatory)][string]$InitialFarm,
    [string]$OutputDirectory = 'generated/exhibitions/system_experience_20261111',
    [string]$BuildDirectory = 'generated/outputs/Release'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$target = [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
$farm = (Resolve-Path -LiteralPath $InitialFarm).Path
$release = (Resolve-Path -LiteralPath (Join-Path $root $BuildDirectory)).Path
foreach ($path in @($target, $farm, $release)) {
    if (!$path.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Workspace paths only' }
    $parent = $path
    while ($parent) {
        if ((Test-Path -LiteralPath $parent) -and ((Get-Item -LiteralPath $parent).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Linked path' }
        $parent = Split-Path -Parent $parent
    }
}
if (Test-Path -LiteralPath $target) { throw 'Refusing to overwrite a runtime' }
if (!(Test-Path -LiteralPath (Join-Path $farm 'farm_documents.json'))) { throw 'Missing prepared catalog' }
if (Get-ChildItem -LiteralPath $farm -Recurse -Attributes ReparsePoint) { throw 'Linked farm' }
foreach ($name in @('CG2_00_01.exe','dxcompiler.dll','dxil.dll')) {
    if (!(Test-Path -LiteralPath (Join-Path $release $name) -PathType Leaf)) { throw "Missing build file: $name" }
}
$resources = Join-Path $root 'project/Resources'
$atlasHash = (Get-FileHash -LiteralPath (Join-Path $resources 'ui/farm_runtime_menu.png') -Algorithm SHA256).Hash.ToLowerInvariant()
$contract = 'FarmRuntimeAtlas:sha256:' + $atlasHash
$binary = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Join-Path $release 'CG2_00_01.exe')))
if (!$binary.Contains($contract)) { throw 'EXE and farm UI atlas do not match. Rebuild Release before packaging.' }
$binary = $null
New-Item -ItemType Directory -Path $target | Out-Null
$extensions = @('.obj','.png','.json','.txt','.wav','.hlsl','.hlsli')
$assets = foreach ($folder in @('farm','ui','shader','generated','title','transition')) {
    Get-ChildItem -LiteralPath (Join-Path $resources $folder) -Recurse -File | Where-Object Extension -In $extensions
}
$assets += Get-Item -LiteralPath (Join-Path $resources 'text_textures.json'), (Join-Path $resources 'levels/farm_scene.json')
# Serif is baked into title textures; only Sans is loaded at runtime.
foreach ($name in @('NotoSansJP-VF.ttf','OFL.txt','NotoSerifJP-OFL.txt')) {
    $assets += Get-Item -LiteralPath (Join-Path $resources ('fonts/' + $name))
}
foreach ($asset in $assets) {
    $dest = Join-Path $target ('Resources/' + [IO.Path]::GetRelativePath($resources, $asset.FullName))
    New-Item -ItemType Directory -Path (Split-Path $dest) -Force | Out-Null
    Copy-Item -LiteralPath $asset.FullName -Destination $dest
}
Copy-Item -LiteralPath (Join-Path $release 'CG2_00_01.exe') -Destination (Join-Path $target 'SuiroNogyo.exe')
$copiedHash = (Get-FileHash -LiteralPath (Join-Path $target 'Resources/ui/farm_runtime_menu.png') -Algorithm SHA256).Hash.ToLowerInvariant()
$copiedBinary = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Join-Path $target 'SuiroNogyo.exe')))
if ($copiedHash -ne $atlasHash -or !$copiedBinary.Contains($contract)) { throw 'Build/assets changed during packaging. Do not use this incomplete runtime.' }
$copiedBinary = $null
foreach ($name in @('dxcompiler.dll','dxil.dll')) { Copy-Item -LiteralPath (Join-Path $release $name) -Destination $target }
Get-ChildItem -LiteralPath $release -Filter '*.cso' -File | Copy-Item -Destination $target
Copy-Item -LiteralPath (Join-Path $root 'generated/codex_checks/contest_finish_20261027/review/Licenses') -Destination $target -Recurse
$credits = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'exhibition/CREDITS.txt')).Replace('ゲームオーバー演出デモ版','ゲームシステム体験版')
$credits += "`r`n雨音・害虫駆除音: プロジェクト内の音声合成で生成。Resources/farm/weather/audio と pests/audio の manifest.json を参照。`r`n"
[IO.File]::WriteAllText((Join-Path $target 'CREDITS.txt'), $credits, [Text.UTF8Encoding]::new($true))
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'system_experience/README.txt') -Destination $target
New-Item -ItemType Directory -Path (Join-Path $target 'Settings') | Out-Null
Copy-Item -LiteralPath $farm -Destination (Join-Path $target 'Settings/farm') -Recurse
$files = @(Get-ChildItem -LiteralPath $target -File -Recurse)
if ($files.Extension -contains '.pdb') { throw 'Debug symbols in runtime' }
@($files | ForEach-Object { [pscustomobject]@{Path=[IO.Path]::GetRelativePath($target,$_.FullName);SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash} }) |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($target + '_manifest.json') -Encoding utf8
[pscustomobject]@{Runtime=$target;Files=$files.Count;MiB=[Math]::Round(($files | Measure-Object Length -Sum).Sum/1MB,2)} | ConvertTo-Json
