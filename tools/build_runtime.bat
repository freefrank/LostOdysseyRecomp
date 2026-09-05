@echo off
rem Configure + build the runtime with clang-cl (preset windows-clang). Usage: tools\build_runtime.bat [target]
setlocal
call "%~dp0setup_windows.bat" || exit /b 1
cd /d "%~dp0.."
if not exist out\build\windows-clang\build.ninja cmake --preset windows-clang || exit /b 1
set T=%1
if "%T%"=="" set T=LostOdysseyRecomp
cmake --build out\build\windows-clang --target %T% || exit /b 1
echo BUILD OK
