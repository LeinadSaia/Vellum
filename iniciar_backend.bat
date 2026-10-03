@echo off
chcp 65001 > nul
echo ===================================================
echo   Iniciando Backend - Vellum (FastAPI)
echo ===================================================

set PYTHON_BIN=python
if exist "venv\Scripts\python.exe" (
    set PYTHON_BIN=venv\Scripts\python.exe
) else (
    where python >nul 2>nul
    if %errorlevel% neq 0 (
        echo [ERRO] Python não foi encontrado no sistema nem na pasta venv!
        pause
        exit /b 1
    )
)

echo Verificando Ollama...
curl -s http://localhost:11434/api/tags >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Serviço do Ollama não detectado.
    echo        O modo Nuvem (Gemini) funcionará normalmente.
    echo        Para usar IA local, inicie o Ollama (ollama serve).
)

echo.
echo Iniciando servidor FastAPI em http://localhost:8000...
echo Pressione Ctrl+C para encerrar.
echo.

%PYTHON_BIN% -m uvicorn main:app --host 0.0.0.0 --port 8000 --reload
pause
