@echo off
setlocal
call "%~dp0setup_windows.bat" || exit /b 1
cd /d "%~dp0.."
if not defined LO_BUILD_JOBS set "LO_BUILD_JOBS=4"
set "LO_EXTRA_CMAKE_ARGS="
if defined LO_ENABLE_DLSS set "LO_EXTRA_CMAKE_ARGS=%LO_EXTRA_CMAKE_ARGS% -DLO_ENABLE_DLSS=%LO_ENABLE_DLSS%"
if defined LO_REQUIRE_DLSS set "LO_EXTRA_CMAKE_ARGS=%LO_EXTRA_CMAKE_ARGS% -DLO_REQUIRE_DLSS=%LO_REQUIRE_DLSS%"
if defined LO_DLSS_SDK_ROOT set "LO_EXTRA_CMAKE_ARGS=%LO_EXTRA_CMAKE_ARGS% -DLO_DLSS_SDK_ROOT=%LO_DLSS_SDK_ROOT%"
if defined LO_ENABLE_FSR set "LO_EXTRA_CMAKE_ARGS=%LO_EXTRA_CMAKE_ARGS% -DLO_ENABLE_FSR=%LO_ENABLE_FSR%"
if defined LO_REQUIRE_FSR set "LO_EXTRA_CMAKE_ARGS=%LO_EXTRA_CMAKE_ARGS% -DLO_REQUIRE_FSR=%LO_REQUIRE_FSR%"
if defined LO_FSR_SDK_ROOT set LO_EXTRA_CMAKE_ARGS=%LO_EXTRA_CMAKE_ARGS% "-DLO_FSR_SDK_ROOT=%LO_FSR_SDK_ROOT%"
if defined LO_FSR_SHADER_DIR set LO_EXTRA_CMAKE_ARGS=%LO_EXTRA_CMAKE_ARGS% "-DLO_FSR_SHADER_DIR=%LO_FSR_SHADER_DIR%"
git -C tools/XenonRecomp apply --reverse --check ../patches/XenonRecomp-lostodyssey.patch >nul 2>&1 || git -C tools/XenonRecomp apply ../patches/XenonRecomp-lostodyssey.patch || exit /b 1
git -C thirdparty/plume apply --reverse --check ../../tools/patches/plume-lostodyssey.patch >nul 2>&1 || git -C thirdparty/plume apply ../../tools/patches/plume-lostodyssey.patch || exit /b 1
cmake -S . -B out/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl %LO_EXTRA_CMAKE_ARGS% || exit /b 1
cmake --build out/build/release --target LostOdysseyRecomp LoShaderPackTool --parallel %LO_BUILD_JOBS% || exit /b 1
