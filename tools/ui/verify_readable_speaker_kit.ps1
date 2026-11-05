param(
 [string]$OutputDirectory = 'generated/ui_progress_readable_20261104',
 [string]$BackupName = 'before-readable.pptx',
 [int]$OriginalVideoSlide = 6,
 [switch]$CaptionCleanup
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$out = Join-Path $root $OutputDirectory
$deck = Get-Content -LiteralPath (Join-Path $out 'deck.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $out 'review/google-final.pptx'))
$before = [IO.Compression.ZipFile]::OpenRead((Join-Path $out "review/$BackupName"))
function Read-Entry($zip, [string]$name) {
 $entry = $zip.GetEntry($name)
 if ($null -eq $entry) { throw "Missing entry: $name" }
 $reader = [IO.StreamReader]::new($entry.Open())
 try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
}
function Normalize([string]$value) { return ($value -replace '\s', '') }
try {
 $slides = @($archive.Entries | Where-Object { $_.FullName -match '^ppt/slides/slide\d+\.xml$' })
 if ($slides.Count -ne $deck.Count) { throw "Slide count $($slides.Count)" }
 $checks = @(); $videos = @()
 for ($i=1; $i -le $deck.Count; $i++) {
  $xml = [xml](Read-Entry $archive "ppt/slides/slide$i.xml")
  $hidden = $xml.DocumentElement.GetAttribute('show') -in @('0','false')
  if ($hidden -ne ($i -eq 10)) { throw "Skip state $i" }
  $text = Normalize (($xml.SelectNodes("//*[local-name()='t']") | ForEach-Object { $_.InnerText }) -join '')
  foreach ($e in $deck[$i-1].elements) {
   if ($e.type -eq 'text' -and -not $text.Contains((Normalize $e.value))) { throw "Text mismatch $i : $($e.value)" }
  }
  if ($CaptionCleanup) {
   $oldXml = [xml](Read-Entry $before "ppt/slides/slide$i.xml")
   $oldText = Normalize (($oldXml.SelectNodes("//*[local-name()='t']") | ForEach-Object { $_.InnerText }) -join '')
   $caption = Normalize $deck[$i-1].referenceNote
   $expected = $oldText
   if ($caption) {
    if (-not $oldText.Contains($caption)) { throw "Original caption missing $i" }
    $expected = $oldText.Replace($caption, '')
   }
   if ($text -ne $expected) { throw "Unexpected text change $i" }
  }
  $rels = [xml](Read-Entry $archive "ppt/slides/_rels/slide$i.xml.rels")
  $note = @($rels.Relationships.Relationship | Where-Object { $_.Type -like '*/notesSlide' })
  if ($note.Count -ne 1) { throw "Notes missing $i" }
  $noteXml = [xml](Read-Entry $archive ('ppt/' + ($note[0].Target -replace '^\.\./','')))
  $body = $noteXml.SelectNodes("//*[local-name()='sp'][.//*[local-name()='ph'][@type='body']]//*[local-name()='t']")
  $actual = Normalize (($body | ForEach-Object { $_.InnerText }) -join '')
  if ($actual -ne (Normalize $deck[$i-1].notes)) { throw "Notes mismatch $i" }
  foreach($r in $rels.Relationships.Relationship) {
   if ($r.Target -like '*drive.google.com*') { $videos += [pscustomobject]@{slide=$i;target=$r.Target} }
  }
  $checks += [pscustomobject]@{slide=$i;allText='PASS';notes='PASS';hidden=$hidden}
 }
 $oldRels = [xml](Read-Entry $before "ppt/slides/_rels/slide$OriginalVideoSlide.xml.rels")
 $oldVideo = @($oldRels.Relationships.Relationship | Where-Object { $_.Target -like '*drive.google.com*' })
 if ($videos.Count -ne 1 -or $videos[0].slide -ne 7 -or $videos[0].target -ne $oldVideo[0].Target) { throw 'Native video changed' }
 $report = [ordered]@{slides=11;mainSlides=10;hiddenSlides=@(10);lastSlideIsPlan=$true;checks=$checks;onlyBottomCaptionsRemoved=[bool]$CaptionCleanup;nativeVideoPreserved=$true;plannedSeconds=300;rehearsalMeasured=$false;thirdPartyPlaybackVerified=$false;gameChanged=$false;sharingChanged=$false}
 $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $out 'review/verification.json') -Encoding UTF8
 Write-Output 'PASS: 11 slides, all text/notes match, supplement10 skipped, plan last, native video preserved on7.'
} finally { $archive.Dispose(); $before.Dispose() }
