@echo off
setlocal
title I-BT-Radio 8048S070C MENU BETA telepito
cd /d "%~dp0"
echo I-BT-Radio 8048S070C MENU BETA - teszt firmware telepito
echo A program eszkozadatokat ellenoriz, COM-portot keres es valasztast ker, majd valodi feltoltesi allapotot mutat.
echo.
if exist "C:\esp32\SYSTEM\TOOLS\platformio\core\penv\Scripts\python.exe" (
  set "PYTHON_CMD=C:\esp32\SYSTEM\TOOLS\platformio\core\penv\Scripts\python.exe"
  goto python_found
)
where py >nul 2>&1
if not errorlevel 1 (
  set "PYTHON_CMD=py -3"
  goto python_found
)
where python >nul 2>&1
if not errorlevel 1 (
  set "PYTHON_CMD=python"
  goto python_found
)
echo HIBA: Python 3 nem talalhato. Telepitsd a python.org oldalrol, es jelold be az Add Python to PATH opciot.
pause
exit /b 2
:python_found
call %PYTHON_CMD% -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 9) else 1)" >nul 2>&1
if errorlevel 1 (
  echo HIBA: Python 3.9 vagy ujabb szukseges.
  pause
  exit /b 2
)
call %PYTHON_CMD% -c "import serial, esptool" >nul 2>&1
if errorlevel 1 (
  echo Python-fuggosegek telepitese a csomag requirements.txt fajlja alapjan...
  call %PYTHON_CMD% -m pip install -r requirements.txt
  if errorlevel 1 (
    echo HIBA: a fuggosegek telepitese nem sikerult. Ellenorizd az internetkapcsolatot es a Python telepitest.
    pause
    exit /b 2
  )
)
call %PYTHON_CMD% -B -X utf8 -u update.py %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" echo A telepites hibaval allt meg. Olvasd el a fenti hibat es a RETURN_TO_EVA naplot.
pause
exit /b %RC%
