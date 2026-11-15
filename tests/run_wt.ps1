# SPDX-License-Identifier: GPL-3.0-only
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
python tools\gen_wavetables.py --check
if ($LASTEXITCODE) { throw 'Wavetable data verification failed' }
New-Item -ItemType Directory -Force -Path build\host | Out-Null
$vcvars = Get-ChildItem 'C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat' |
          Select-Object -First 1 -ExpandProperty FullName
if (-not $vcvars) { throw 'vcvars64.bat not found: install the Visual Studio C++ Build Tools.' }
$cl = 'cl /nologo /TC /O2 /W4 /WX /Itests /FImsvc.h tests\wt_host.c ' +
      '/Fo:build\host\wt_host.obj /Fe:build\host\wt_host.exe'
& $env:ComSpec /c "call `"$vcvars`" >nul && $cl"
if ($LASTEXITCODE) { throw 'Wavetable host build failed' }
& build\host\wt_host.exe build\host\wavetable
if ($LASTEXITCODE) { throw 'Wavetable C checks failed' }
python tests\wtcheck.py build\host\wavetable
if ($LASTEXITCODE) { throw 'Wavetable audio/data checks failed' }
