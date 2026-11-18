param(
    [string]$OutputDirectory = 'generated/exhibitions/contest_check_20261116',
    [switch]$Preview,
    [switch]$CheerAudition,
    [switch]$ClearGallery
)
$ErrorActionPreference = 'Stop'
if ($CheerAudition -and !$Preview) { throw 'CheerAudition requires Preview' }
if ($ClearGallery -and (!$Preview -or $CheerAudition)) { throw 'ClearGallery requires Preview and excludes CheerAudition' }
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$target = [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
$initial = $target + '_initial'
foreach ($path in @($target, $initial)) {
    if (!$path.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Workspace paths only' }
    $parent = $path
    while ($parent) {
        if ((Test-Path -LiteralPath $parent) -and ((Get-Item -LiteralPath $parent).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Linked path' }
        $parent = Split-Path -Parent $parent
    }
    if (Test-Path -LiteralPath $path) { throw 'Refusing to replace an existing check package' }
}
$source = Join-Path $root 'project/Resources/farm/experience/final_submission.json'
$data = Get-Content -LiteralPath $source -Raw -Encoding utf8 | ConvertFrom-Json
$id = [string]$data.document.id
if ($id -notmatch '^farm_[0-9]+_[0-9]+$' -or $data.date.day -ne 30 -or
    $data.economy.contestReservationId -le 0 -or $data.economy.contestResults.Count -ne 2 -or
    $data.economy.contestResults[0].contestDay -ne 10 -or $data.economy.contestResults[1].contestDay -ne 20) {
    throw 'Expected an unsubmitted final-day exhibition fixture'
}
# Keep the production save intact; only its independent catalog selects the initial farm.
New-Item -ItemType Directory -Path (Join-Path $initial 'saves') | Out-Null
Copy-Item -LiteralPath $source -Destination (Join-Path $initial ('saves/' + $id + '.json'))
@{schemaVersion=1; activeDocumentId=$id} | ConvertTo-Json |
    Set-Content -LiteralPath (Join-Path $initial 'farm_documents.json') -Encoding utf8
& (Join-Path $PSScriptRoot 'prepare_system_experience.ps1') -InitialFarm $initial -OutputDirectory $OutputDirectory
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'contest_check/Start_Check.cmd') -Destination $target
$readme = 'contest_check/README.txt'
if ($Preview) {
    # Replay immutable contest fixtures without submitting or changing the initial farm.
    Copy-Item -LiteralPath (Join-Path $root 'project/Resources/exhibition') -Destination (Join-Path $target 'Resources') -Recurse
    New-Item -ItemType Directory -Path (Join-Path $target 'Settings/runtime') -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'exhibition/contest_exhibition.json') -Destination (Join-Path $target 'Settings/runtime/contest_exhibition.json')
    $readme = 'contest_check/README_Preview.txt'
    if ($CheerAudition) {
        @{enabled=$true; repeatFirstCheer=$true} | ConvertTo-Json |
            Set-Content -LiteralPath (Join-Path $target 'Settings/runtime/contest_exhibition.json') -Encoding utf8
        $readme = 'contest_check/README_Audition.txt'
    }
    if ($ClearGallery) {
        @{enabled=$true; clearGallery=$true} | ConvertTo-Json |
            Set-Content -LiteralPath (Join-Path $target 'Settings/runtime/contest_exhibition.json') -Encoding utf8
        $readme = 'contest_check/README_Gallery.txt'
    }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot $readme) -Destination (Join-Path $target 'README.txt')
$save = Join-Path $target ('Settings/farm/saves/' + $id + '.json')
if ((Get-FileHash -LiteralPath $save).Hash -ne (Get-FileHash -LiteralPath $source).Hash) { throw 'Initial fixture changed' }
$files = @(Get-ChildItem -LiteralPath $target -File -Recurse)
@($files | ForEach-Object { [pscustomobject]@{
    Path=[IO.Path]::GetRelativePath($target,$_.FullName)
    SHA256=(Get-FileHash -LiteralPath $_.FullName).Hash
} }) | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($target + '_manifest.json') -Encoding utf8
[pscustomobject]@{Runtime=$target; Preview=[bool]$Preview; InitialDay=30; ReservedHarvest=$data.economy.contestReservationId; Files=$files.Count} | ConvertTo-Json
