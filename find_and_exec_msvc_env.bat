@echo off
@rem This file finds and adds the msvc values to env

for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set InstallDir=%%i
)

if exist "%InstallDir%\VC\Auxiliary\Build\vcvarsall.bat" (
  call "%InstallDir%\VC\Auxiliary\Build\vcvarsall.bat" amd64
  @echo on
  echo "Exporting PATH to ADD_PATH_ENV"
  echo "ADD_PATH_ENV=%PATH%" > $env:GITHUB_ENV
  echo "Exporting finished"
)