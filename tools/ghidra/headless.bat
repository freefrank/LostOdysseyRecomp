@echo off
rem Run Ghidra headless against the LostOdyssey project in LostOdysseyRecompLib\private\ghidra
rem Usage: tools\ghidra\headless.bat <extra analyzeHeadless args>
rem   import:   tools\ghidra\headless.bat -import LostOdysseyRecompLib\private\disc1\default.xex -loader XEXLoaderWVLoader
rem   script:   tools\ghidra\headless.bat -process default.xex -noanalysis -postScript MyScript.py
setlocal
if not defined GHIDRA_HOME (
  echo Set GHIDRA_HOME to your Ghidra installation directory and JAVA_HOME to a compatible JDK.
  exit /b 1
)
if not exist "%GHIDRA_HOME%\support\analyzeHeadless.bat" exit /b 1
if defined JAVA_HOME set "PATH=%JAVA_HOME%\bin;%PATH%"
cd /d "%~dp0..\.."
call "%GHIDRA_HOME%\support\analyzeHeadless.bat" "%CD%\LostOdysseyRecompLib\private\ghidra" LostOdyssey -scriptPath "%CD%\tools\ghidra" %*
