param([switch]$Video,[switch]$Dense,[switch]$Integration,[switch]$FullHD,[switch]$SplashComparison,[switch]$LocalEdges,[switch]$Ablation)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=Join-Path $root 'docs/design/water_routes'
if($Integration -or $SplashComparison) { $out=Join-Path $root 'docs/design/water_integration' }
[void][IO.Directory]::CreateDirectory($out)
$exe=Join-Path $root 'generated/outputs/Release/CG2_00_01.exe'
$env:SUIRO_WATER_TRIAL='1'
$env:SUIRO_WATER_TRIAL_COMPARE='C'
$env:SUIRO_WATER_TRIAL_INSPECT='0'
$env:SUIRO_WATER_TRIAL_LEGACY='0'
$env:SUIRO_WATER_TRIAL_CLASSIC='0'
$env:SUIRO_WATER_TRIAL_NO_HARVEST_COST='0'
$env:SUIRO_WATER_TRIAL_PROFILE='1'
$env:SUIRO_WATER_TRIAL_REPLAY=if($Video){'1'}else{'0'}
$env:SUIRO_WATER_TRIAL_DENSE=if($Dense){'1'}else{'0'}
$env:SUIRO_WATER_TRIAL_1080=if($FullHD){'1'}else{'0'}
$env:SUIRO_WATER_TRIAL_TREE='1' # Freeze the reference hydraulics for shader comparisons.
if($SplashComparison -or $LocalEdges){$env:SUIRO_WATER_TRIAL_TREE='0'}
$env:SUIRO_WATER_TRIAL_DRAIN='0'
$prefix=if($Dense){'dense-'}else{''}
if($Integration){$prefix+=if($FullHD){'1080-'}else{'720-'}}
if($LocalEdges){$prefix='edges-'+$prefix}
if($Ablation){$prefix='control-'+$prefix}
$rounds=if($Video -or $SplashComparison){1}else{3}
for($i=1;$i -le $rounds;$i++) {
    $order=if($i%2){'shaded','routed'}else{'routed','shaded'}
    if($Integration){$order=if($i%2){'routed','active'}else{'active','routed'}}
    if($SplashComparison){$order='splash-before','splash-after'}
    if($Ablation){$order=if($i%2){'routed','nolanes','shaded'}else{'shaded','nolanes','routed'}}
    foreach($name in $order) {
        $env:SUIRO_WATER_TRIAL_PREVIOUS=if($name -eq 'shaded'){'1'}else{'0'}
        $env:SUIRO_WATER_TRIAL_ACTIVE_FACES=if($name -eq 'active'){'1'}else{'0'}
        $env:SUIRO_WATER_TRIAL_NO_LANES=if($name -eq 'nolanes'){'1'}else{'0'}
        $env:SUIRO_WATER_TRIAL_OLD_SPLASH=if($name -eq 'splash-before'){'1'}else{'0'}
        $p=Start-Process $exe -WorkingDirectory (Join-Path $root 'project') -PassThru
        try {
            $variant=if($SplashComparison){'routed'}else{$name}
            $csv=Join-Path "$env:TEMP/SuiroWaterTrial" ("gpu-$variant-"+$p.Id+'.csv')
            $until=[DateTime]::UtcNow.AddSeconds(55)
            while(!(Test-Path $csv) -or @(Import-Csv $csv).Count -ne 64) {
                if($p.HasExited -or [DateTime]::UtcNow -gt $until){throw "No complete GPU sample: $name PID $($p.Id)"}
                Start-Sleep -Milliseconds 250
            }
            if(!$Video) {
                if(@(Import-Csv $csv).Count -ne 64){throw 'Incomplete benchmark'}
                Copy-Item $csv (Join-Path $out "$prefix$name-$i.csv")
            }
            if($Video -or $i -eq 1){
                $p.Refresh()
                while($Video -and $p.MainWindowHandle -eq 0 -and !$p.HasExited -and [DateTime]::UtcNow -lt $until) {
                    Start-Sleep -Milliseconds 50;$p.Refresh()
                }
                $hwnd=$p.MainWindowHandle
                if($hwnd -eq 0){throw 'No game window'}
                $target='hwnd='+$hwnd.ToInt64()
                if(!$Video) {
                    & ffmpeg -nostdin -loglevel error -y -f gdigrab -draw_mouse 0 -i $target -frames:v 1 -update 1 (Join-Path $out "$prefix$name.png")
                    if($LASTEXITCODE){throw 'Window snapshot failed'}
                } else {
                    $file=Join-Path $out "$name.mp4"
                    $filter='gfxcapture=hwnd='+$hwnd.ToInt64()+':max_framerate=30:capture_cursor=0,hwdownload,format=bgra'
                    & ffmpeg -nostdin -loglevel error -y -f lavfi -i $filter -t 16 -r 30 -an -c:v libx264 -preset veryfast -crf 18 -pix_fmt yuv420p -movflags +faststart $file
                    if($LASTEXITCODE){throw 'Window recording failed'}
                }
            }
            Write-Output "OK $name run $i"
        } finally {
            if(!$p.HasExited){[void]$p.CloseMainWindow(); if(!$p.WaitForExit(10000)){throw "Close trial PID $($p.Id) manually"}}
        }
    }
}
