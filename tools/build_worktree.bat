@echo off
rem Configure + build a runtime target in a git worktree, taking SDKs, FSR shaders and the
rem dependency checkouts from the main checkout. Usage: tools\build_worktree.bat [target]
rem   LO_MAIN_CHECKOUT  main checkout (default: first entry of `git worktree list`)
rem   LO_DEPS_DIR       dependency checkouts (default: <main checkout>/.cache/deps)
rem   LO_BUILD_JOBS     parallel jobs (default 8; use fewer when several builds run)
rem The worktree needs the prepared inputs of docs\BUILDING.md (submodules, tools\patches),
rem plus copies of the main checkout's generated, gitignored files: LostOdysseyRecompLib\ppc,
rem LostOdysseyRecompLib\config and LostOdysseyRecompLib\private (image_disc*.bin and .sym).
rem LostOdysseyRecomp\kernel\imports_stubs.cpp has to be a copy, not a hard link: it is
rem regenerated and would write into the main checkout.
setlocal
if not defined LO_MAIN_CHECKOUT for /f "tokens=1,* delims= " %%A in ('git -C "%~dp0." worktree list --porcelain') do if "%%A"=="worktree" if not defined LO_MAIN_CHECKOUT set "LO_MAIN_CHECKOUT=%%B"
if not defined LO_MAIN_CHECKOUT (
  echo Main checkout not found. Set LO_MAIN_CHECKOUT.
  exit /b 1
)
if not defined LO_DEPS_DIR set "LO_DEPS_DIR=%LO_MAIN_CHECKOUT%/.cache/deps"
if not exist "%LO_DEPS_DIR%" echo Dependency checkouts not found in %LO_DEPS_DIR%. Set LO_DEPS_DIR or LO_MAIN_CHECKOUT.& exit /b 1
if not defined LO_BUILD_JOBS set "LO_BUILD_JOBS=8"
set "MAIN=%LO_MAIN_CHECKOUT%"
set "D=%LO_DEPS_DIR%"
set "T=%~1"
if not defined T set "T=LostOdysseyRecomp"
call "%~dp0setup_windows.bat" || exit /b 1
cd /d "%~dp0.."
if not exist out\build\windows-clang\build.ninja (
  cmake --preset windows-clang ^
    -DLO_DLSS_SDK_ROOT="%D%/nvidia-dlss-37495948" ^
    -DLO_STREAMLINE_SDK_ROOT="%D%/streamline-v2.14.1/sdk" ^
    -DLO_FSR_SDK_ROOT="%D%/fidelityfx-sdk-v1.1.4" ^
    -DLO_FSR_FG_RUNTIME="%D%/fidelityfx-sdk-v1.1.4/PrebuiltSignedDLL/amd_fidelityfx_dx12.dll" ^
    -DLO_FSR_VULKAN_FG_RUNTIME="%D%/fidelityfx-sdk-v1.1.4/PrebuiltSignedDLL/amd_fidelityfx_vk.dll" ^
    -DLO_FSR_SHADER_DIR="%MAIN%/out/fsr-shaders-vk" ^
    -DLO_FSR_DX12_SHADER_DIR="%MAIN%/out/fsr-dx12-shaders" ^
    -DLO_FSR_DX12_ADAPTER_DIR="%MAIN%/out/fsr-dx12-adapter" ^
    -DLO_ENABLE_DLSS=ON -DLO_ENABLE_FSR=ON -DLO_ENABLE_STREAMLINE_FG=ON ^
    -DLO_ENABLE_D3D12_DLSS_FG=ON -DLO_ENABLE_FSR_FG=ON -DLO_ENABLE_VULKAN_FSR_FG=ON || exit /b 1
)
cmake --build out\build\windows-clang --target %T% -j %LO_BUILD_JOBS% || exit /b 1
echo BUILD OK
