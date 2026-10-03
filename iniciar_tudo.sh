#!/usr/bin/env bash
# ===================================================
#   Vellum - Iniciar Aplicação Completa (Linux)
# ===================================================

cd "$(dirname "$0")"

# 1. Compilação do Frontend se ainda não existir
if [ ! -f "build_linux/Vellum" ]; then
    echo "[1/3] Compilando Frontend C++ (Qt6)..."
    cmake -B build_linux -S qt_frontend -DCMAKE_BUILD_TYPE=Release
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

# Encerra o backend e libera a porta 8000 ao fechar a interface
cleanup() {
    kill $BACKEND_PID 2>/dev/null
    fuser -k 8000/tcp >/dev/null 2>&1
}
trap cleanup EXIT INT TERM

# Aguarda o backend responder
for i in {1..25}; do
    if curl -s http://localhost:8000/docs >/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done

# 3. Inicia o Frontend Qt
echo "[3/3] Iniciando Frontend Vellum..."
export QT_QPA_PLATFORM="wayland;xcb"
./build_linux/Vellum "$@"
