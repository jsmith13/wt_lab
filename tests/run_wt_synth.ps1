# SPDX-License-Identifier: GPL-3.0-only
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
python tools\gen_wavetables.py --check
if ($LASTEXITCODE) { throw 'Wavetable data verification failed' }
python tools\gen_wt_dsp.py --check
if ($LASTEXITCODE) { throw 'Voice DSP data verification failed' }
New-Item -ItemType Directory -Force -Path build\host | Out-Null
$vcvars = Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat' |
          Select-Object -First 1 -ExpandProperty FullName
if (-not $vcvars) { throw 'vcvars64.bat not found: install the Visual Studio C++ Build Tools.' }
$core = 'cl /nologo /TC /O2 /W4 /WX /Itests /FImsvc.h tests\wt_synth_host.c ' +
        '/Fo:build\host\wt_synth_host.obj /Fe:build\host\wt_synth_host.exe'
$app = 'cl /nologo /TC /O2 /W3 /Itests /FImsvc.h /Ifirmware\src /Ifirmware\gen tests\wt_app_host.c ' +
       '/Fo:build\host\wt_app_host.obj /Fe:build\host\wt_app_host.exe'
$policy = 'cl /nologo /TC /O2 /W4 /WX /Itests /FImsvc.h tests\flash_test.c ' +
          '/Fo:build\host\flash_test.obj /Fe:build\host\flash_test.exe'
& $env:ComSpec /c "call `"$vcvars`" >nul && $core && $app && $policy"
if ($LASTEXITCODE) { throw 'WT instrument host build failed' }
& build\host\flash_test.exe
if ($LASTEXITCODE) { throw 'Flash chip/timeout policy checks failed' }
& build\host\wt_synth_host.exe build\host\wt-voices.wav
if ($LASTEXITCODE) { throw 'WT voice checks failed' }
& build\host\wt_app_host.exe
if ($LASTEXITCODE) { throw 'WT application ownership/event checks failed' }
& .\tests\run_host.ps1 tests\wt_instrument.txt
& build\host\wtlab.exe build\host\wt_ui tests\wt_ui.txt | Tee-Object -FilePath build\host\wt_ui.log
if ($LASTEXITCODE) { throw 'WT gesture scenario failed' }
$flash = 'build\host\wt_workflow.flash.bin'
if (Test-Path -LiteralPath $flash) { Remove-Item -LiteralPath $flash }
& build\host\wtlab.exe build\host\wt_workflow tests\wt_workflow.txt 0 $flash | Tee-Object -FilePath build\host\wt_workflow.log
if ($LASTEXITCODE) { throw 'WT performance/save scenario failed' }
& build\host\wtlab.exe build\host\wt_reload tests\wt_reload.txt 0 $flash | Tee-Object -FilePath build\host\wt_reload.log
if ($LASTEXITCODE) { throw 'WT process-restart reload scenario failed' }
& build\host\wtlab.exe build\host\wt_stress tests\wt_stress.txt | Tee-Object -FilePath build\host\wt_stress.log
if ($LASTEXITCODE) { throw 'WT combined stress scenario failed' }
python tests\wt_synthcheck.py build\host\wt-voices.wav build\host\wt_instrument build\host\wt_ui build\host\wt_workflow build\host\wt_reload build\host\wt_stress
if ($LASTEXITCODE) { throw 'WT instrument audio/panel checks failed' }
$controllers = 'build\host\wt_controllers'
& build\host\wtlab.exe $controllers tests\wt_controllers.txt | Tee-Object -FilePath "$controllers.log"
if ($LASTEXITCODE) { throw 'WT MIDI controller scenario failed' }
python tests\controllercheck.py $controllers
if ($LASTEXITCODE) { throw 'WT MIDI controller audio/editor checks failed' }
