@echo off
chcp 65001 > nul

set PARAM=%1

if "%PARAM%"=="/phi3" goto dl_phi3
if "%PARAM%"=="/llama32" goto dl_llama32
if "%PARAM%"=="/llama3" goto dl_llama3
if "%PARAM%"=="/whisper-tiny" goto dl_wtiny
if "%PARAM%"=="/whisper-base" goto dl_wbase
goto fim

:dl_phi3
echo.
echo ==============================================================================
echo   Vellum - Verificando Suporte a Modelo Offline: phi3:mini
echo ==============================================================================
where ollama >nul 2>nul
if %errorlevel% equ 0 (
    echo [OLLAMA DETECTADO] Baixando phi3:mini via Ollama...
    ollama pull phi3:mini
) else (
    echo [INFO] Ollama nao encontrado instalado neste computador.
    echo        O modelo offline phi3:mini nao sera baixado agora.
    echo        O Vellum continuara funcionando perfeitamente em modo Nuvem (Gemini).
    echo        Para usar IA local no futuro, instale o Ollama em https://ollama.com
)
goto fim

:dl_llama32
echo.
echo ==============================================================================
echo   Vellum - Verificando Suporte a Modelo Offline: llama3.2:3b
echo ==============================================================================
where ollama >nul 2>nul
if %errorlevel% equ 0 (
    echo [OLLAMA DETECTADO] Baixando llama3.2:3b via Ollama...
    ollama pull llama3.2:3b
) else (
    echo [INFO] Ollama nao encontrado instalado neste computador.
    echo        O modelo offline llama3.2:3b nao sera baixado agora.
    echo        O Vellum continuara funcionando perfeitamente em modo Nuvem (Gemini).
    echo        Para usar IA local no futuro, instale o Ollama em https://ollama.com
)
goto fim

:dl_llama3
echo.
echo ==============================================================================
echo   Vellum - Verificando Suporte a Modelo Offline: llama3:8b
echo ==============================================================================
where ollama >nul 2>nul
if %errorlevel% equ 0 (
    echo [OLLAMA DETECTADO] Baixando llama3:8b via Ollama...
    ollama pull llama3:8b
) else (
    echo [INFO] Ollama nao encontrado instalado neste computador.
    echo        O modelo offline llama3:8b nao sera baixado agora.
    echo        O Vellum continuara funcionando perfeitamente em modo Nuvem (Gemini).
    echo        Para usar IA local no futuro, instale o Ollama em https://ollama.com
)
goto fim

:dl_wtiny
echo.
echo [WHISPER] Baixando modelo tiny.en para reconhecimento de voz...
powershell -Command "New-Item -ItemType Directory -Force -Path \"$env:USERPROFILE\.cache\whisper\" | Out-Null; if (-not (Test-Path \"$env:USERPROFILE\.cache\whisper\tiny.en.pt\")) { (New-Object System.Net.WebClient).DownloadFile('https://openaipublic.azureedge.net/main/whisper/models/d3dd57d32accea0b295c96e26691aa14d8822fac7d9d27d5dc00b4ca2826dd03/tiny.en.pt', \"$env:USERPROFILE\.cache\whisper\tiny.en.pt\") }"
goto fim

:dl_wbase
echo.
echo [WHISPER] Baixando modelo base.en para reconhecimento de voz...
powershell -Command "New-Item -ItemType Directory -Force -Path \"$env:USERPROFILE\.cache\whisper\" | Out-Null; if (-not (Test-Path \"$env:USERPROFILE\.cache\whisper\base.en.pt\")) { (New-Object System.Net.WebClient).DownloadFile('https://openaipublic.azureedge.net/main/whisper/models/25a8566e1d0c1e2231d1c762132cd20e0f96a85d16145c3a00adf5d1ac670ead/base.en.pt', \"$env:USERPROFILE\.cache\whisper\base.en.pt\") }"
goto fim

:fim
exit /b 0
