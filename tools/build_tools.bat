@echo off
rem Build XenonRecomp, XenonAnalyse and xexdump with MSVC into out\build\tools
setlocal
call "%~dp0setup_windows.bat" || exit /b 1
cd /d "%~dp0.."
rem apply local XenonRecomp patch (idempotent: skip if already applied)
git -C tools/XenonRecomp apply --check --reverse ../patches/XenonRecomp-lostodyssey.patch >nul 2>&1 || git -C tools/XenonRecomp apply ../patches/XenonRecomp-lostodyssey.patch || exit /b 1
cmake -S tools\xexdump -B out\build\tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl || exit /b 1
cmake --build out\build\tools --target XenonRecomp XenonAnalyse xexdump || exit /b 1
python tools\ppc_codegen.py stamp-tool || exit /b 1
echo BUILD OK
