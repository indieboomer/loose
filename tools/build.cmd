@echo off
setlocal
set "LOOSE_CONFIG=Release"
if /i "%~1"=="debug" set "LOOSE_CONFIG=Debug"
cd /d "%~dp0.."
if not defined VSCMD_VER (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat"
)
set "PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
call cmake --preset windows -DCMAKE_MAKE_PROGRAM="%VSINSTALLDIR%Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
if errorlevel 1 exit /b 1
call cmake --build build --config %LOOSE_CONFIG% --parallel 8
if errorlevel 1 exit /b 1
call ctest --test-dir build -C %LOOSE_CONFIG% --output-on-failure
exit /b %errorlevel%
