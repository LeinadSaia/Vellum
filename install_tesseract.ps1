# Vellum - instala o Tesseract OCR (UB-Mannheim) em <app>\Tesseract-OCR
# e adiciona o pacote de idioma Portugues (o instalador ja inclui Ingles).
# Uso: powershell -ExecutionPolicy Bypass -File install_tesseract.ps1 -InstallDir "C:\...\Vellum\Tesseract-OCR"
param(
    [Parameter(Mandatory = $true)][string]$InstallDir
)

$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$ProgressPreference = 'SilentlyContinue'

$tesseractExe = Join-Path $InstallDir 'tesseract.exe'
$tessdata = Join-Path $InstallDir 'tessdata'
$ua = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) Vellum-Installer'
$setupUrls = @(
    'https://digi.bib.uni-mannheim.de/tesseract/tesseract-ocr-w64-setup-5.5.0.20241111.exe',
    'https://github.com/UB-Mannheim/tesseract/releases/download/v5.5.0.20241111/tesseract-ocr-w64-setup-5.5.0.20241111.exe'
)
$setupFile = Join-Path $env:TEMP 'vellum-tesseract-setup.exe'

function Test-Tesseract { Test-Path $tesseractExe }

if (-not (Test-Tesseract)) {
    foreach ($url in $setupUrls) {
        try {
            Write-Host "Baixando Tesseract: $url"
            Invoke-WebRequest -Uri $url -OutFile $setupFile -UserAgent $ua -UseBasicParsing
            if ((Get-Item $setupFile).Length -gt 10MB) { break }
        } catch {
            Write-Host "Falha: $($_.Exception.Message)"
        }
    }

    if (Test-Path $setupFile) {
        try {
            # O instalador do Tesseract pede elevacao (UAC); /D deve ser o ultimo argumento, sem aspas.
            Start-Process -FilePath $setupFile -ArgumentList '/S', "/D=$InstallDir" -Verb RunAs -Wait
        } catch {
            Write-Host "Instalacao do Tesseract cancelada ou falhou: $($_.Exception.Message)"
        }
        Remove-Item $setupFile -Force -ErrorAction SilentlyContinue
    }
}

# Fallback: winget (se o download direto falhou)
if (-not (Test-Tesseract) -and (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Host 'Tentando via winget...'
    try {
        winget install -e --id UB-Mannheim.TesseractOCR --silent --accept-package-agreements --accept-source-agreements --location "$InstallDir"
    } catch {
        Write-Host "winget falhou: $($_.Exception.Message)"
    }
}

# Pacote de idioma Portugues
$tessdataOk = Test-Path $tessdata
if (-not $tessdataOk) {
    # Se o winget instalou em outro lugar padrao, tenta detectar
    foreach ($p in @("$env:ProgramFiles\Tesseract-OCR\tessdata", "${env:ProgramFiles(x86)}\Tesseract-OCR\tessdata")) {
        if (Test-Path $p) { $tessdata = $p; $tessdataOk = $true; break }
    }
}
if ($tessdataOk) {
    $por = Join-Path $tessdata 'por.traineddata'
    if (-not (Test-Path $por)) {
        try {
            Invoke-WebRequest -Uri 'https://github.com/tesseract-ocr/tessdata_fast/raw/main/por.traineddata' -OutFile $por -UserAgent $ua -UseBasicParsing
        } catch {
            Write-Host "Nao foi possivel baixar o idioma Portugues: $($_.Exception.Message)"
        }
    }
} else {
    Write-Host 'Tesseract nao foi instalado. O OCR por retangulo ficara indisponivel ate instalar manualmente.'
}
exit 0
