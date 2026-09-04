@echo off
rem Configure the clang-cl preset with the MSVC SDK environment. Build afterwards with: cmake --build outbuildwindows-clang
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%ProgramFiles%\LLVM\bin;%PATH%"
cd /d "%~dp0.."
cmake --preset windows-clang
