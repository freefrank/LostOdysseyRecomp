@echo off
rem Shared tool discovery. Intentionally no setlocal: export the SDK environment.
if defined VSCMD_VER goto tools
if defined LO_VCVARS64 goto vcvars
set "LO_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%LO_VSWHERE%" (
  echo Visual Studio not found. Run from an x64 Developer Command Prompt or set LO_VCVARS64.
  exit /b 1
)
for /f "usebackq delims=" %%I in (`"%LO_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "LO_VCVARS64=%%I\VC\Auxiliary\Build\vcvars64.bat"
:vcvars
if not exist "%LO_VCVARS64%" exit /b 1
call "%LO_VCVARS64%" >nul || exit /b 1
:tools
if defined VSINSTALLDIR set "PATH=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
if defined LLVM_ROOT set "PATH=%LLVM_ROOT%\bin;%PATH%"
where clang-cl >nul 2>&1
if errorlevel 1 if exist "%ProgramFiles%\LLVM\bin\clang-cl.exe" set "PATH=%ProgramFiles%\LLVM\bin;%PATH%"
where cmake >nul 2>&1 || exit /b 1
where ninja >nul 2>&1 || exit /b 1
exit /b 0
