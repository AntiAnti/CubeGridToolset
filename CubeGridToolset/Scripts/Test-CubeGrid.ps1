param([string]$EngineRoot = 'D:\Epic Games\UE_5.8\Engine')
$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$project = Join-Path $projectRoot 'TestMocap58.uproject'
$report = Join-Path $projectRoot 'Saved\CubeGridToolsetTests'
New-Item -ItemType Directory -Path $report -Force | Out-Null
$editor = Join-Path $EngineRoot 'Binaries\Win64\UnrealEditor.exe'
$arguments = @(
    ('"' + $project + '"'), '/Engine/Maps/Entry', '-unattended', '-nosplash', '-nullrhi',
    '-NoLiveCoding', '-NoLoadStartupPackages',
    '-ExecCmds="Automation RunTests CubeGridToolset"', '-TestExit="Automation Test Queue Empty"',
    ('-ReportExportPath="' + $report + '"'),
    ('-abslog="' + (Join-Path $report 'Editor.log') + '"')
)
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
Write-Output ('Editor exit code: ' + $process.ExitCode)
Write-Output ('Test report: ' + $report)
exit $process.ExitCode
