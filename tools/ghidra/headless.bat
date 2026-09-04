@echo off
rem Run Ghidra headless against the LostOdyssey project in LostOdysseyRecompLib\private\ghidra
rem Usage: tools\ghidra\headless.bat <extra analyzeHeadless args>
rem   import:   tools\ghidra\headless.bat -import LostOdysseyRecompLib\private\disc1\default.xex -loader XEXLoaderWVLoader
rem   script:   tools\ghidra\headless.bat -process default.xex -noanalysis -postScript MyScript.py
setlocal
set "JAVA_HOME=%ProgramFiles%\Eclipse Adoptium\jdk-21.0.12.101-hotspot"
set "PATH=%JAVA_HOME%\bin;%PATH%"
cd /d "%~dp0..\.."
call tools\ghidra_12.1.3\support\analyzeHeadless.bat "%CD%\LostOdysseyRecompLib\private\ghidra" LostOdyssey -scriptPath "%CD%\tools\ghidra" %*
