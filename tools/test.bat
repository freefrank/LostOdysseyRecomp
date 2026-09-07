@echo off
setlocal
python -B "%~dp0tests\run.py" %*
exit /b %errorlevel%
