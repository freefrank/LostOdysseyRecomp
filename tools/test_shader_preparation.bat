@echo off
call "%~dp0test.bat" shaders pipeline %*
exit /b %errorlevel%
