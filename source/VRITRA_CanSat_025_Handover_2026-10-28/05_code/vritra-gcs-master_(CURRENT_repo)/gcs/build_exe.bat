@echo off
REM ==========================================================================
REM   VRITRA - CAN-7USAT Ground Control Station  -  one-click builder
REM   Team 2026-INSPACe-CAN-7USAT-025, Lovely Professional University
REM
REM   Double-click this ONCE. It produces:  dist\VRITRA_GCS.exe
REM   Pure-QPainter 3-D view - NO OpenGL / GPU required at runtime.
REM ==========================================================================
title Build VRITRA GCS .exe
cd /d "%~dp0"
setlocal

echo.
echo  ============================================================
echo   VRITRA GCS  -  building your standalone .exe
echo   (one time only, about 2-4 minutes)
echo  ============================================================
echo.

REM ---- 1. Python present? ----
python --version >nul 2>&1
if errorlevel 1 (
  echo  [X] Python was not found.
  echo      Install Python 3.10+ from https://www.python.org/downloads/
  echo      Tick "Add Python to PATH" during install, then run this file again.
  echo.
  pause
  exit /b 1
)
echo  [1/3] Python found:
python --version
echo.

REM ---- 2. Source file present? ----
if not exist "vritra_gcs.py" (
  echo  [X] vritra_gcs.py is not in this folder.
  echo      Keep build_exe.bat, vritra_gcs.py and VRITRA_GCS.spec together.
  echo.
  pause
  exit /b 1
)

REM ---- 3. Dependencies ----
echo  [2/3] Installing build tools and libraries...
python -m pip install --upgrade pip >nul 2>&1
python -m pip install pyinstaller PyQt5 pyqtgraph pyserial numpy
echo.

REM ---- 4. Freeze into ONE .exe ----
echo  [3/3] Freezing application (this is the slow part)...
if exist "VRITRA_GCS.spec" (
  REM Preferred: reproducible build from the spec file
  python -m PyInstaller --noconfirm --clean VRITRA_GCS.spec
) else (
  REM Fallback: inline options, no spec file needed
  set ICON=
  if exist "vritra.ico" set ICON=--icon vritra.ico
  python -m PyInstaller ^
    --noconfirm --clean --onefile --windowed ^
    --name VRITRA_GCS ^
    %ICON% ^
    --hidden-import serial.tools.list_ports ^
    --hidden-import numpy ^
    --hidden-import pyqtgraph.graphicsItems.ViewBox.axisCtrlTemplate_pyqt5 ^
    --hidden-import pyqtgraph.graphicsItems.PlotItem.plotConfigTemplate_pyqt5 ^
    --hidden-import pyqtgraph.imageview.ImageViewTemplate_pyqt5 ^
    --collect-data pyqtgraph ^
    --exclude-module tkinter ^
    --exclude-module matplotlib ^
    --exclude-module PyQt6 ^
    --exclude-module OpenGL ^
    --exclude-module pyqtgraph.opengl ^
    vritra_gcs.py
)
echo.

if exist "dist\VRITRA_GCS.exe" (
  echo  ============================================================
  echo   SUCCESS
  echo   Your application is:   dist\VRITRA_GCS.exe
  echo.
  echo   Double-click it to run. No Python required at runtime.
  echo   No GPU / OpenGL driver required either.
  echo   Copy VRITRA_GCS.exe anywhere or make a desktop shortcut.
  echo  ============================================================
) else (
  echo  ------------------------------------------------------------
  echo   BUILD DID NOT COMPLETE.  Scroll up to read the error.
  echo   Most common fix: run this file once more.
  echo  ------------------------------------------------------------
)
echo.
pause
