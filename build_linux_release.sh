#!/usr/bin/env bash
# ==============================================================================
#   Vellum - Gerador de Pacote de Release para Linux
#   Gera Vellum-v1.0.0-Linux-x86_64.tar.gz pronto para o GitHub Releases
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RELEASE_DIR="release_linux/vellum"
DIST_DIR="dist_installer"
TAR_NAME="Vellum-v1.0.0-Linux-x86_64.tar.gz"

echo "=============================================================================="
echo "          Vellum - Gerador de Pacote de Release (Linux)                       "
echo "=============================================================================="

# 1. Limpeza
rm -rf release_linux "${DIST_DIR}/${TAR_NAME}"
mkdir -p "${RELEASE_DIR}" "${DIST_DIR}"

# 2. Compilação do Frontend C++
echo "[1/4] Compilando Frontend C++ (Qt6)..."
cmake -B build_linux -S qt_frontend -DCMAKE_BUILD_TYPE=Release
cmake --build build_linux -j"$(nproc)"

# 3. Cópia dos arquivos para o pacote
echo "[2/4] Estruturando pacote release..."
cp "build_linux/Vellum" "${RELEASE_DIR}/Vellum"
cp "main.py" "${RELEASE_DIR}/main.py"
cp "requirements.txt" "${RELEASE_DIR}/requirements.txt"
cp "iniciar_tudo.sh" "${RELEASE_DIR}/iniciar_tudo.sh"
cp "iniciar_backend.sh" "${RELEASE_DIR}/iniciar_backend.sh"
cp "install_linux.sh" "${RELEASE_DIR}/install_linux.sh"
cp "uninstall_linux.sh" "${RELEASE_DIR}/uninstall_linux.sh"
cp "vellum.desktop" "${RELEASE_DIR}/vellum.desktop"
cp "README.md" "${RELEASE_DIR}/README.md"

if [ -f "qt_frontend/resources/app_icon.png" ]; then
    cp "qt_frontend/resources/app_icon.png" "${RELEASE_DIR}/vellum.png"
elif [ -f "LOGO.png" ]; then
    cp "LOGO.png" "${RELEASE_DIR}/vellum.png"
fi

chmod +x "${RELEASE_DIR}/Vellum"
chmod +x "${RELEASE_DIR}/iniciar_tudo.sh"
chmod +x "${RELEASE_DIR}/iniciar_backend.sh"
chmod +x "${RELEASE_DIR}/install_linux.sh"
chmod +x "${RELEASE_DIR}/uninstall_linux.sh"

# 4. Geração do tar.gz
echo "[3/4] Compactando pacote ${TAR_NAME}..."
cd release_linux
tar -czf "../${DIST_DIR}/${TAR_NAME}" vellum
cd ..

echo "[4/4] Pacote gerado com sucesso!"
ls -lh "${DIST_DIR}/${TAR_NAME}"

echo ""
echo "=============================================================================="
echo "  Pacote pronto para upload no GitHub Releases:                               "
echo "  ${DIST_DIR}/${TAR_NAME}                                                     "
echo "=============================================================================="
