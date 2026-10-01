@echo off
chcp 65001 > nul
echo ===================================================
echo   Ensinador de Inglês - Iniciar Aplicação Completa
echo ===================================================

echo [1/2] Iniciando Backend FastAPI em segundo plano...
start "Backend - Ensinador de Inglês" cmd /k "python -m uvicorn main:app --host 0.0.0.0 --port 8000"

echo Aguardando 3 segundos para inicializacao do backend...
timeout /t 3 /nobreak > nul

echo [2/2] Procurando executavel do Frontend Qt...
set EXE_PATH=qt_frontend\build\Desktop_Qt_6_11_2_MSVC2022_64bit_Debug\EnsinadorDeIngles.exe
if exist "%EXE_PATH%" (
    echo Iniciando Frontend Qt...
    start "" "%EXE_PATH%"
) else (
    echo Executavel nao encontrado em %EXE_PATH%.
    echo Abra o projeto no Qt Creator e compile em modo Debug/Release.
)
