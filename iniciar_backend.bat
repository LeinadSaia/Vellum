@echo off
chcp 65001 > nul
echo ===================================================
echo   Iniciando Backend - Ensinador de Inglês (FastAPI)
echo ===================================================

where python >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERRO] Python não foi encontrado no PATH do Windows!
    pause
    exit /b 1
)

echo Verificando Ollama...
curl -s http://localhost:11434/api/tags >nul 2>&1
if %errorlevel% neq 0 (
    echo [AVISO] O serviço do Ollama parece estar fechado. Lembre-se de abrir o Ollama e baixar o modelo: ollama run llama3
)

echo.
echo Iniciando servidor FastAPI em http://localhost:8000...
echo Pressione Ctrl+C para encerrar.
echo.

python -m uvicorn main:app --host 0.0.0.0 --port 8000 --reload
pause
