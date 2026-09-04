@echo off
rem Configure + build the runtime with clang-cl (preset windows-clang). Usage: tools\build_runtime.bat [target]
setlocal
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%ProgramFiles%\LLVM\bin;%PATH%"
cd /d "%~dp0.."
if not exist out\build\windows-clang\build.ninja cmake --preset windows-clang || exit /b 1
set T=%1
if "%T%"=="" set T=LostOdysseyRecomp
cmake --build out\build\windows-clang --target %1 || exit /b 1
echo BUILD OK
