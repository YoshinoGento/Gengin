param([string]$OutputDirectory = 'generated/exhibitions/contest_20261027')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output = [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
if (!$output.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output must stay within this workspace'
}
if (Test-Path -LiteralPath $output) { throw 'Refusing to replace an existing exhibition' }
$template = Join-Path $root 'generated/codex_checks/contest_finish_20261027/review'
$exe = Join-Path $root 'generated/outputs/Release/CG2_00_01.exe'
foreach ($path in @($template, $exe, (Join-Path $root 'project/Resources/exhibition/failed.json'))) {
    if (!(Test-Path -LiteralPath $path)) { throw "Missing prerequisite: $path" }
}
$before = @(Get-ChildItem -LiteralPath $template -Recurse -File | ForEach-Object {
    [pscustomobject]@{Path=$_.FullName; Hash=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
New-Item -ItemType Directory -Path $output | Out-Null
foreach ($name in @('Resources','Licenses','dxcompiler.dll','dxil.dll')) {
    Copy-Item -LiteralPath (Join-Path $template $name) -Destination $output -Recurse
}
# Preserve the established runtime allowlist, but refresh every included authored resource.
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $output 'Resources') -File -Recurse) {
    $source = Join-Path $root ('project/' + [IO.Path]::GetRelativePath($output,$file.FullName))
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        Copy-Item -LiteralPath $source -Destination $file.FullName
    }
}
Copy-Item -LiteralPath (Join-Path $root 'project/Resources/exhibition') -Destination (Join-Path $output 'Resources') -Recurse
Copy-Item -LiteralPath $exe -Destination (Join-Path $output 'SuiroNogyoExhibition.exe')
foreach ($name in @('README.txt','StartExhibition.cmd','CREDITS.txt')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "exhibition/$name") -Destination $output
}
$settings = Join-Path $output 'Settings/runtime'
New-Item -ItemType Directory -Path $settings -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'exhibition/contest_exhibition.json') -Destination $settings
foreach ($file in $before) {
    if ((Get-FileHash -LiteralPath $file.Path).Hash -ne $file.Hash) { throw 'Previous review changed' }
}
$manifest = Join-Path (Split-Path $output) ((Split-Path $output -Leaf) + '_source_review_hashes.json')
if (Test-Path -LiteralPath $manifest) { throw 'Refusing to replace an existing source manifest' }
$before | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $manifest -Encoding utf8
Get-ChildItem -LiteralPath (Join-Path $output 'Resources/exhibition') -File | ForEach-Object {
    if ((Get-FileHash -LiteralPath $_.FullName).Hash -ne
        (Get-FileHash -LiteralPath (Join-Path $root "project/Resources/exhibition/$($_.Name)")).Hash) { throw 'Exhibition fixture mismatch' }
}
[pscustomobject]@{Runtime=$output; MiB=[math]::Round(((Get-ChildItem -LiteralPath $output -Recurse -File |
    Measure-Object Length -Sum).Sum/1MB),2); ExeSHA256=(Get-FileHash -LiteralPath $exe).Hash;
    ProtectedPreviousFiles=$before.Count; PersonalSavesCopied=0} | ConvertTo-Json
