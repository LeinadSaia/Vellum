@echo off
chcp 65001 > nul
echo ==============================================================================
echo        Vellum - Script de Compilacao e Empacotamento para Windows
echo ==============================================================================

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

set RELEASE_DIR=release_windows
set DIST_DIR=dist_installer

echo.
echo [1/5] Limpando pastas de release anteriores...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%RELEASE_DIR%"
mkdir "%DIST_DIR%"

echo.
echo [2/5] Compilando Frontend C++ (Qt6)...
cmake -B build_windows -S qt_frontend -DCMAKE_BUILD_TYPE=Release
cmake --build build_windows --config Release -j%NUMBER_OF_PROCESSORS%

if not exist "build_windows\Release\Vellum.exe" (
    if not exist "build_windows\Vellum.exe" (
        echo [ERRO] Executavel Vellum.exe nao foi gerado!
        pause
        exit /b 1
    ) else (
        copy /y "build_windows\Vellum.exe" "%RELEASE_DIR%\Vellum.exe"
    )
) else (
    copy /y "build_windows\Release\Vellum.exe" "%RELEASE_DIR%\Vellum.exe"
)

echo.
echo [3/5] Coletando dependencias Qt com windeployqt...
windeployqt --no-translations --compiler-runtime "%RELEASE_DIR%\Vellum.exe"
copy /y "qt_frontend\resources\app_icon.ico" "%RELEASE_DIR%\app_icon.ico"

echo [4/5] Empacotando Backend Python (FastAPI) com PyInstaller...
pip install pyinstaller -r requirements.txt
pyinstaller --clean --noconsole --name vellum_backend --onedir --distpath "temp_dist" main.py
if exist "temp_dist\vellum_backend" (
    move "temp_dist\vellum_backend" "%RELEASE_DIR%\backend"
    rmdir /s /q "temp_dist"
)

echo.
echo [5/5] Gerando Instalador com Inno Setup...
set ISCC="C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if exist %ISCC% (
    %ISCC% installer_windows.iss
    echo [SUCESSO] Instalador gerado em %DIST_DIR%\Vellum-Setup-Windows-v1.0.1.exe!
) else (
    where iscc >nul 2>nul
    if %errorlevel% equ 0 (
        iscc installer_windows.iss
        echo [SUCESSO] Instalador gerado em %DIST_DIR%\Vellum-Setup-Windows-v1.0.1.exe!
    ) else (
        echo [INFO] Inno Setup (ISCC.exe) nao encontrado.
        echo        A pasta release_windows\ contem a versao portatil completa pronta para uso!
        echo        Para gerar o instalador executavel (.exe), instale o Inno Setup 6 e execute: iscc installer_windows.iss
    )
)

echo.
echo [Extra] Gerando Pacote Portatil (.zip)...
copy /y "download_models.bat" "%RELEASE_DIR%\"
copy /y "install_tesseract.ps1" "%RELEASE_DIR%\"
powershell -Command "Compress-Archive -Path '%RELEASE_DIR%\*' -DestinationPath '%DIST_DIR%\Vellum-v1.0.1-Windows-x86_64.zip' -Force"
echo [SUCESSO] Pacote portatil gerado em %DIST_DIR%\Vellum-v1.0.1-Windows-x86_64.zip!

echo.
echo ==============================================================================
echo                      Processo finalizado!
echo ==============================================================================
pause
