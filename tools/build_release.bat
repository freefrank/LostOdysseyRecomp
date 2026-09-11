@echo off
setlocal
call "%~dp0setup_windows.bat" || exit /b 1
cd /d "%~dp0.."
if not defined LO_BUILD_JOBS set "LO_BUILD_JOBS=4"
git -C tools/XenonRecomp apply --reverse --check ../patches/XenonRecomp-lostodyssey.patch >nul 2>&1 || git -C tools/XenonRecomp apply ../patches/XenonRecomp-lostodyssey.patch || exit /b 1
git -C thirdparty/plume apply --reverse --check ../../tools/patches/plume-lostodyssey.patch >nul 2>&1 || git -C thirdparty/plume apply ../../tools/patches/plume-lostodyssey.patch || exit /b 1
cmake -S . -B out/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl "-DLO_PREBUILT_PPC_DIR=%LO_PREBUILT_PPC_DIR%" || exit /b 1
cmake --build out/build/release --target LostOdysseyRecomp --parallel %LO_BUILD_JOBS% || exit /b 1
