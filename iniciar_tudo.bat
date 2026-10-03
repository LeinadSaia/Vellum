@echo off
chcp 65001 > nul
echo ===================================================
echo   Vellum - Iniciar Aplicação Completa (Windows)
echo ===================================================

echo [1/2] Iniciando Backend FastAPI em segundo plano...
if exist "venv\Scripts\python.exe" (
    start "Backend - Vellum" cmd /c "venv\Scripts\python.exe -m uvicorn main:app --host 127.0.0.1 --port 8000"
) else (
    start "Backend - Vellum" cmd /c "python -m uvicorn main:app --host 127.0.0.1 --port 8000"
)

echo Aguardando inicialização do backend...
timeout /t 2 /nobreak > nul

echo [2/2] Procurando executável do Frontend Vellum...
if exist "Vellum.exe" (
    start "" "Vellum.exe" %*
    exit /b 0
)
if exist "build\Vellum.exe" (
    start "" "build\Vellum.exe" %*
    exit /b 0
)
if exist "build_windows\Vellum.exe" (
    start "" "build_windows\Vellum.exe" %*
    exit /b 0
)
if exist "qt_frontend\build\Vellum.exe" (
    start "" "qt_frontend\build\Vellum.exe" %*
    exit /b 0
)

for /r "qt_frontend\build" %%f in (Vellum.exe) do (
    if exist "%%f" (
        start "" "%%f" %*
        exit /b 0
    )
)

echo [AVISO] Executável Vellum.exe não encontrado.
echo Compile o frontend com o CMake ou abra no Qt Creator.
pause
