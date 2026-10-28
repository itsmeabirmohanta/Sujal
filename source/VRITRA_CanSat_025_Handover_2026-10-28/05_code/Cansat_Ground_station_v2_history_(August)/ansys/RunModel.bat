@echo off
rem Build the VRITRA CanSat RF model in Ansys HFSS.
rem Leave Ansys Electronics Desktop OPEN before running this - the script
rem attaches to the session that is already running and builds in that window.
rem Just double-click this file.  No PowerShell, no quoting rules.

cd /d "%~dp0"

if not exist "%~dp0pyaedtenv\Scripts\python.exe" (
  echo.
  echo Cannot find the Python environment at:
  echo   %~dp0pyaedtenv
  echo.
  echo Recreate it with:
  echo   python -m venv "%~dp0pyaedtenv"
  echo   "%~dp0pyaedtenv\Scripts\python.exe" -m pip install pyaedt==1.3.0
  echo.
  pause
  exit /b 1
)

echo Building the CanSat RF model...
echo.
"%~dp0pyaedtenv\Scripts\python.exe" "%~dp0CansatRfModel.py"

echo.
echo ---------------------------------------------------------------
echo Finished. Switch to the Ansys window and press Ctrl+D to fit.
echo ---------------------------------------------------------------
pause
