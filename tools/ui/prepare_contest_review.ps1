param([Parameter(Mandatory)][string]$FixtureRoot)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$fixtures = (Resolve-Path -LiteralPath $FixtureRoot).Path
$checks = Join-Path $root 'generated/codex_checks'
if ([IO.Path]::GetDirectoryName($fixtures) -ne $checks -or
    [IO.Path]::GetFileName($fixtures) -notmatch '^contest_cultivated_[0-9]+$') {
    throw 'Expected a generated, cultivated review fixture directory'
}
$template = Join-Path $checks 'contest_sequence_20261026/current'
$release = Join-Path $root 'generated/outputs/Release/CG2_00_01.exe'
if ((Get-FileHash -LiteralPath $release).Hash -ne
    (Get-FileHash -LiteralPath (Join-Path $template 'VoxelSceneryQA.exe')).Hash) {
    throw 'Review template and Release differ; verify the runtime before packaging'
}
$output = Join-Path $fixtures 'review'
if (Test-Path -LiteralPath $output) { throw 'Review output already exists; never replace an existing review' }
$caseNames = @('01_passed','02_failed','03_missed')
$records = @()
$caseCatalogs = @{}
$active = $null
foreach ($case in $caseNames) {
    $farm = Join-Path $fixtures "$case/Settings/farm"
    $catalog = Get-Content -LiteralPath (Join-Path $farm 'farm_documents.json') -Raw | ConvertFrom-Json
    $caseCatalogs[$case] = $catalog
    $files = @(Get-ChildItem -LiteralPath (Join-Path $farm 'saves') -File -Filter '*.json')
    if ($files.Count -ne $(if ($case -eq '03_missed') { 1 } else { 2 })) { throw "Unexpected saves in $case" }
    foreach ($file in $files) {
        $save = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if ($save.schemaVersion -ne 18) { throw 'Unexpected review save schema' }
        $records += [pscustomobject]@{Case=$case; File=$file.FullName; Id=$file.BaseName; Hash=(Get-FileHash -LiteralPath $file.FullName).Hash}
    }
    if ($case -eq '02_failed') { $active = $catalog.activeDocumentId }
}
if (@($records.Id | Select-Object -Unique).Count -ne $records.Count -or $active -notin $records.Id) {
    throw 'Invalid or duplicate review identities'
}
New-Item -ItemType Directory -Path $output | Out-Null
foreach ($name in @('Resources','Licenses','CREDITS.txt','dxcompiler.dll','dxil.dll')) {
    Copy-Item -LiteralPath (Join-Path $template $name) -Destination $output -Recurse
}
Copy-Item -LiteralPath $release -Destination (Join-Path $output 'SuiroNogyoReview.exe')
# Keep the reviewed resource allowlist, but reject any stale source-backed asset.
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $output 'Resources') -File -Recurse) {
    $relative = [IO.Path]::GetRelativePath($output,$file.FullName)
    $source = Join-Path $root "project/$relative"
    if ((Test-Path -LiteralPath $source -PathType Leaf) -and
        (Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $file.FullName).Hash) {
        throw "Stale template resource: $relative. Incomplete review retained for inspection."
    }
}
$saveDir = Join-Path $output 'Settings/farm/saves'
New-Item -ItemType Directory -Path $saveDir -Force | Out-Null
foreach ($record in $records) {
    Copy-Item -LiteralPath $record.File -Destination $saveDir
    $copied = Join-Path $saveDir ([IO.Path]::GetFileName($record.File))
    if ((Get-FileHash -LiteralPath $copied).Hash -ne $record.Hash) { throw 'Fixture copy mismatch' }
}
@{schemaVersion=1;activeDocumentId=$active} | ConvertTo-Json |
    Set-Content -LiteralPath (Join-Path $output 'Settings/farm/farm_documents.json') -Encoding utf8
# No personal audio/editor settings: the runtime uses its authored 40% defaults.
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'contest_review/StartGame.cmd') -Destination $output
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'contest_review/README.txt') -Destination $output
New-Item -ItemType Directory -Path (Join-Path $output 'ReviewCases') | Out-Null
$launcherTemplate = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'contest_review/StartCase.cmd.template') -Raw
foreach ($case in $caseNames) {
    $caseCatalogs[$case] | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output "ReviewCases/$case.json") -Encoding utf8
    $launcherTemplate.Replace('@CASE@',$case) | Set-Content -LiteralPath (Join-Path $output "$case.cmd") -Encoding ascii
}
$manifest = [ordered]@{
    Purpose='Local review only; not a submission package'
    Evidence='System-cultivated crops, not a human playthrough; grams are authored game units'
    ExeSHA256=(Get-FileHash -LiteralPath $release).Hash
    InitialCase='02_failed / Judging Ready'
    Fixtures=@($records | Select-Object Case,Id,Hash)
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'REVIEW_MANIFEST.json') -Encoding utf8
[pscustomobject]@{Runtime=$output; Saves=$records.Count; InitialCase=$manifest.InitialCase;
    MiB=[math]::Round(((Get-ChildItem -LiteralPath $output -File -Recurse | Measure-Object Length -Sum).Sum/1MB),2)} | ConvertTo-Json
