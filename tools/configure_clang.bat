@echo off
setlocal
rem Configure the clang-cl preset with the MSVC SDK environment. Build afterwards with: cmake --build outbuildwindows-clang
call "%~dp0setup_windows.bat" || exit /b 1
cd /d "%~dp0.."
cmake --preset windows-clang
