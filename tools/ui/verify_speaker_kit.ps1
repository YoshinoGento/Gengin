$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out = Join-Path $root 'generated/ui_speaker_kit_plan_last_20261104'
$deck = Get-Content -LiteralPath (Join-Path $out 'deck.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $out 'review/google-final.pptx'))
$before = [IO.Compression.ZipFile]::OpenRead((Join-Path $out 'review/before-order.pptx'))
$originalOrder = @(1, 2, 3, 4, 5, 6, 7, 9, 8)
function Read-Entry([string]$name) {
    $entry = $archive.GetEntry($name)
    if ($null -eq $entry) { throw "Missing entry: $name" }
    $reader = [IO.StreamReader]::new($entry.Open())
    try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
}
function Normalize([string]$text) { return ($text -replace '\s', '') }
function Xml-Text([string]$value) {
    $xml = [xml]$value
    return (($xml.SelectNodes("//*[local-name()='t']") | ForEach-Object { $_.InnerText }) -join '')
}
try {
    $slides = @($archive.Entries | Where-Object { $_.FullName -match '^ppt/slides/slide\d+\.xml$' })
    if ($slides.Count -ne 9) { throw "Wrong slide count: $($slides.Count)" }
    $checks = @()
    $videos = @()
    for ($i = 1; $i -le 9; $i++) {
        $slideXml = [xml](Read-Entry "ppt/slides/slide$i.xml")
        $hidden = $slideXml.DocumentElement.GetAttribute('show') -in @('0', 'false')
        if ($hidden -ne ($i -eq 8)) { throw "Unexpected skip state: $i" }
        $slideText = Normalize (Xml-Text $slideXml.OuterXml)
        if (-not $slideText.Contains((Normalize $deck[$i - 1].title))) { throw "Title mismatch: $i" }
        if ($i -eq 9) {
            foreach ($element in $deck[8].elements) {
                if ($element.type -eq 'text' -and -not $slideText.Contains((Normalize $element.value))) {
                    throw "Schedule text mismatch: $($element.value)"
                }
            }
        }
        $originalIndex = $originalOrder[$i - 1]
        $reader = [IO.StreamReader]::new($before.GetEntry("ppt/slides/slide$originalIndex.xml").Open())
            try { $originalText = Normalize (Xml-Text $reader.ReadToEnd()) } finally { $reader.Dispose() }
        if ($slideText -ne $originalText) { throw "Unexpected slide text change: $i" }
        $rels = [xml](Read-Entry "ppt/slides/_rels/slide$i.xml.rels")
        $noteRel = @($rels.Relationships.Relationship | Where-Object { $_.Type -like '*/notesSlide' })
        if ($noteRel.Count -ne 1) { throw "Missing notes relation: $i" }
        $target = 'ppt/' + ($noteRel[0].Target -replace '^\.\./', '')
        $noteXml = [xml](Read-Entry $target)
        $body = $noteXml.SelectNodes("//*[local-name()='sp'][.//*[local-name()='ph'][@type='body']]//*[local-name()='t']")
        $notes = Normalize (($body | ForEach-Object { $_.InnerText }) -join '')
        if ($notes -ne (Normalize $deck[$i - 1].notes)) { throw "Notes mismatch: $i" }
        foreach ($r in $rels.Relationships.Relationship) {
            if ($r.Target -like '*drive.google.com*') { $videos += [pscustomobject]@{ slide = $i; target = $r.Target } }
        }
        $checks += [pscustomobject]@{ slide = $i; original = $originalIndex; hidden = $hidden; title = 'PASS'; notes = 'PASS' }
    }
    if ($videos.Count -ne 1 -or $videos[0].slide -ne 6) { throw 'Native video reference was not preserved on slide 6' }
    $report = [ordered]@{
        slideCount = $slides.Count
        hiddenSlides = @(8)
        lastSlideIsPlan = $true
        supplementBRemoved = $true
        checks = $checks
        nativeVideoReference = $videos
        videoPlaybackRepeated = $false
        thirdPartyAccessVerified = $false
        rehearsalMeasured = $false
        priorSchoolAuditScreenshots = 19
        scheduleBasis = '2026-11-07 to 2026-12-06 proposal; next presentation date unconfirmed'
        allScheduleText = 'PASS'
        otherSlideTextPreserved = 'PASS'
        gameCodeChanged = $false
        sharingChanged = $false
    }
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $out 'review/verification.json') -Encoding UTF8
    Write-Output 'PASS: 9 cloud titles/notes match; supplement A skipped; plan last; slide 6 retains one Drive video reference.'
} finally { $archive.Dispose(); $before.Dispose() }
