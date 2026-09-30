@echo off
rem Build the Linux ELF target inside WSL with Ninja and clang, with optional deployment.
rem Usage: tools\build_wsl.bat [target] [--deploy]
setlocal
set "ROOT=%~dp0.."
pushd "%ROOT%"
set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=LostOdysseyRecomp"
set "EXTRA=%~2"

echo ==^> Invoking WSL Linux build for target: %TARGET% %EXTRA%
wsl -d Manjaro bash -c "./tools/build_linux.sh %TARGET% %EXTRA%" || exit /b 1
popd
