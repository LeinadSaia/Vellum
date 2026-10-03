@echo off
chcp 65001 > nul

set PARAM=%1

if "%PARAM%"=="/whisper-tiny" goto dl_wtiny
if "%PARAM%"=="/whisper-base" goto dl_wbase
if "%PARAM%"=="/phi3" goto dl_phi3
if "%PARAM%"=="/llama32" goto dl_llama32
if "%PARAM%"=="/llama3" goto dl_llama3
goto fim

:dl_wtiny
echo.
echo ===================================================
echo   Baixando Modelo Whisper tiny.en (73 MB)...
echo ===================================================
powershell -Command "New-Item -ItemType Directory -Force -Path \"$env:USERPROFILE\.cache\whisper\" | Out-Null; $wc = New-Object System.Net.WebClient; $wc.DownloadFile('https://openaipublic.azureedge.net/main/whisper/models/d3dd57d32accea0b295c96e26691aa14d8822fac7d9d27d5dc00ba0adc22e8dd/tiny.en.pt', \"$env:USERPROFILE\.cache\whisper\tiny.en.pt\")"
goto fim

:dl_wbase
echo.
echo ===================================================
echo   Baixando Modelo Whisper base.en (139 MB)...
echo ===================================================
powershell -Command "New-Item -ItemType Directory -Force -Path \"$env:USERPROFILE\.cache\whisper\" | Out-Null; $wc = New-Object System.Net.WebClient; $wc.DownloadFile('https://openaipublic.azureedge.net/main/whisper/models/256150255c601dbb170ef89248a8458ad4cf37081fa97e5b2eb5de50d1da676e/base.en.pt', \"$env:USERPROFILE\.cache\whisper\base.en.pt\")"
goto fim

:dl_phi3
echo.
echo ===================================================
echo   Baixando Modelo phi3:mini via Ollama (~2.2 GB)...
echo ===================================================
where ollama >nul 2>nul
if %errorlevel% equ 0 (
    ollama pull phi3:mini
) else (
    echo [INFO] Ollama nao encontrado no PATH do Windows.
    echo        Instale o Ollama em https://ollama.com e execute: ollama pull phi3:mini
    timeout /t 5 > nul
)
goto fim

:dl_llama32
echo.
echo ===================================================
echo   Baixando Modelo llama3.2:3b via Ollama (~2.0 GB)...
echo ===================================================
where ollama >nul 2>nul
if %errorlevel% equ 0 (
    ollama pull llama3.2:3b
) else (
    echo [INFO] Ollama nao encontrado no PATH do Windows.
    echo        Instale o Ollama em https://ollama.com e execute: ollama pull llama3.2:3b
    timeout /t 5 > nul
)
goto fim

:dl_llama3
echo.
echo ===================================================
echo   Baixando Modelo llama3:8b via Ollama (~4.7 GB)...
echo ===================================================
where ollama >nul 2>nul
if %errorlevel% equ 0 (
    ollama pull llama3:8b
) else (
    echo [INFO] Ollama nao encontrado no PATH do Windows.
    echo        Instale o Ollama em https://ollama.com e execute: ollama pull llama3:8b
    timeout /t 5 > nul
)
goto fim

:fim
exit /b 0
