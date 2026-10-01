#!/usr/bin/env bash
# ===================================================
#   Ensinador de Inglês - Iniciar Tudo (Linux / Wayland)
# ===================================================

cd "$(dirname "$0")"

# 1. Compilação do Frontend se ainda não existir
if [ ! -f "build_linux/EnsinadorDeIngles" ]; then
    echo "[1/3] Compilando Frontend C++ (Qt6)..."
    cmake -B build_linux -S qt_frontend
    cmake --build build_linux -j$(nproc)
    if [ $? -ne 0 ]; then
        echo "[ERRO] Falha ao compilar o frontend Qt6."
        exit 1
    fi
fi

# 2. Inicia o Backend em segundo plano
echo "[2/3] Iniciando Backend FastAPI..."
./iniciar_backend.sh &
BACKEND_PID=$!

# Função para encerrar o backend ao fechar o script
trap "kill $BACKEND_PID 2>/dev/null" EXIT

# Aguarda 3 segundos para o backend subir
sleep 3

# 3. Inicia o Frontend Qt
echo "[3/3] Iniciando Frontend Qt (Wayland/XWayland)..."
export QT_QPA_PLATFORM="wayland;xcb"
./build_linux/EnsinadorDeIngles

# Ao fechar a janela, o backend encerra junto graças ao trap
