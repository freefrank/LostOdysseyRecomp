@echo off
setlocal
call "%~dp0test_shader_index.bat" || exit /b 1
call "%~dp0setup_windows.bat" || exit /b 1
cd /d "%~dp0.."
if not exist out\shader-preparation-test mkdir out\shader-preparation-test
clang-cl /nologo /std:c++20 /EHsc /O2 /MT /W4 /WX /ILostOdysseyRecomp tools/tests/cpx_decode_test.cpp /Foout/shader-preparation-test/cpx.obj /Feout/shader-preparation-test/cpx.exe || exit /b 1
out\shader-preparation-test\cpx.exe || exit /b 1
clang-cl /nologo /std:c++20 /EHsc /O2 /MT /W4 /WX /ILostOdysseyRecomp tools/tests/shader_resource_variants_test.cpp /Foout/shader-preparation-test/variants.obj /Feout/shader-preparation-test/variants.exe || exit /b 1
out\shader-preparation-test\variants.exe out\shader-preparation-test || exit /b 1
clang-cl /nologo /std:c++20 /EHsc /O2 /MT /W4 /WX /ILostOdysseyRecomp tools/tests/pipeline_cache_test.cpp /Foout/shader-preparation-test/pipelines.obj /Feout/shader-preparation-test/pipelines.exe || exit /b 1
out\shader-preparation-test\pipelines.exe out\shader-preparation-test || exit /b 1
