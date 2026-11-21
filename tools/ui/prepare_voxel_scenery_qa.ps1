param([switch]$VerifyOnly, [ValidateSet('current','baseline')][string]$Variant = 'current',
    [ValidatePattern('^[a-z0-9_]+$')][string]$RunName = 'voxel_scenery_20261021')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output = Join-Path $root "generated/codex_checks/$RunName"
$runtime = Join-Path $output $Variant
$baseline = Join-Path $output 'protected_before.json'
$ready = Join-Path $root 'generated/start_contest_20261020/ready'
$packages = @(Get-ChildItem -LiteralPath $ready -Directory)
if ($packages.Count -ne 1) { throw 'Expected one reviewed runtime package' }
function Get-ProtectedFiles {
    $result = @{}
    foreach ($folder in @('project/Settings','generated/start_contest_20261020/ready',
        'generated/title_contest_20261006/ready','generated/transition_contest_20261013/ready')) {
        $path = Join-Path $root $folder
        if (!(Test-Path -LiteralPath $path)) { continue }
        foreach ($file in Get-ChildItem -LiteralPath $path -File -Recurse) {
            $result[$file.FullName.Substring($root.Length+1)] = (Get-FileHash -LiteralPath $file.FullName).Hash
        }
    }
    return $result
}
$current = Get-ProtectedFiles
New-Item -ItemType Directory -Path $output -Force | Out-Null
if (!(Test-Path -LiteralPath $baseline)) {
    if ($VerifyOnly) { throw 'Missing protection baseline' }
    $current | ConvertTo-Json | Set-Content -LiteralPath $baseline -Encoding utf8
}
$before = Get-Content -LiteralPath $baseline -Raw | ConvertFrom-Json -AsHashtable
if ($current.Count -ne $before.Count) { throw 'Protected file set changed' }
foreach ($key in $before.Keys) {
    if ($current[$key] -ne $before[$key]) { throw "Protected file changed: $key" }
}
if (!$VerifyOnly) {
    $exe = Join-Path $runtime 'VoxelSceneryQA.exe'
    if (Get-Process -Name VoxelSceneryQA -ErrorAction SilentlyContinue | Where-Object Path -eq $exe) {
        throw 'Close the owned QA process before refresh'
    }
    if ($Variant -eq 'baseline' -and (Test-Path -LiteralPath $exe)) { throw 'Baseline is immutable; use VerifyOnly' }
    if (!(Test-Path -LiteralPath $runtime)) {
        New-Item -ItemType Directory -Path $runtime | Out-Null
        Get-ChildItem -LiteralPath $packages[0].FullName |
            Where-Object { $_.Extension -ne '.exe' -and $_.Name -notin @('MANIFEST.json','StartGame.cmd') } |
            Copy-Item -Destination $runtime -Recurse
    }
    Copy-Item -LiteralPath (Join-Path $root 'generated/outputs/Release/CG2_00_01.exe') -Destination $exe -Force
    Copy-Item -LiteralPath (Join-Path $root 'project/Resources/shader') -Destination (Join-Path $runtime 'Resources') -Recurse -Force
    $paths = @('title/countryside.obj','title/reflection_scenery.obj','title/pond_shore.obj',
        'title/countryside_mesh.json','farm/arrival/scenery.obj','farm/arrival/manifest.json',
        'title/grass_patch.obj','title/grass_far_patch.obj','farm/arrival/grass_layout.json',
        'farm/arrival/crown_0.obj','farm/arrival/crown_1.obj','farm/arrival/crown_2.obj',
        'farm/arrival/crown_3.obj','farm/arrival/crown_4.obj','farm/soil_ridge.obj','farm/bank_stone.obj',
        'farm/arrival/tools_wood.obj','farm/arrival/tools_metal.obj','ui/farm_hud_icons.png',
        'farm/contest/ceremony/manifest.json')
    foreach ($part in @('carrot','carrot_leaves','tomato','tomato_stems','pumpkin','pumpkin_vines')) {
        $paths += "farm/crop_$part.obj","farm/contest/ceremony/crop_$part.obj"
    }
    foreach ($path in $paths) {
        $parent = Split-Path -Parent (Join-Path $runtime "Resources/$path")
        if (!(Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
        Copy-Item -LiteralPath (Join-Path $root "project/Resources/$path") -Destination (Join-Path $runtime "Resources/$path") -Force
    }
}
[pscustomobject]@{Runtime=$runtime; ProtectedFiles=$current.Count; Variant=$Variant} | ConvertTo-Json
