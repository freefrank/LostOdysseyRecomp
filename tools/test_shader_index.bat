@echo off
setlocal
call "%~dp0setup_windows.bat" || exit /b 1
cd /d "%~dp0.."
if not exist out\shader-index-test mkdir out\shader-index-test
clang-cl /nologo /std:c++20 /EHsc /O2 /MT /ILostOdysseyRecomp tools/tests/shader_resource_scan_test.cpp /Foout/shader-index-test/test.obj /Feout/shader-index-test/test.exe || exit /b 1
out\shader-index-test\test.exe out\shader-index-test || exit /b 1
