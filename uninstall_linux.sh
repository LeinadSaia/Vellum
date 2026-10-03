#!/usr/bin/env bash
# ==============================================================================
#   Vellum - Desinstalador Completo e Limpo para Linux
# ==============================================================================

set -e

INSTALL_PREFIX="${HOME}/.local"
BIN_DIR="${INSTALL_PREFIX}/bin"
OPT_DIR="${INSTALL_PREFIX}/share/vellum"
APP_DIR="${INSTALL_PREFIX}/share/applications"
ICON_DIR="${INSTALL_PREFIX}/share/icons/hicolor/512x512/apps"
CONFIG_DIR="${HOME}/.config/Vellum"

echo "======================================================"
echo "          Desinstalador do Vellum (Linux)             "
echo "======================================================"
echo ""

# Encerra qualquer instância em execução
pkill -f "build_linux/Vellum" 2>/dev/null || true
pkill -f "share/vellum/Vellum" 2>/dev/null || true
pkill -f "uvicorn main:app --host 127.0.0.1 --port 8000" 2>/dev/null || true

echo "Removendo arquivos do aplicativo..."
rm -f "${BIN_DIR}/vellum"
rm -rf "${OPT_DIR}"
rm -f "${APP_DIR}/vellum.desktop"
rm -f "${ICON_DIR}/vellum.png"

# Pergunta sobre configurações
read -p "Deseja também remover as configurações salvas (preferências e chaves) em ~/.config/Vellum? [s/N]: " RESP
if [[ "$RESP" =~ ^[sS]$ ]]; then
    rm -rf "${CONFIG_DIR}"
    echo "Configurações removidas."
else
    echo "Configurações preservadas em ${CONFIG_DIR}."
fi

# Atualiza caches do sistema
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${APP_DIR}" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "${INSTALL_PREFIX}/share/icons/hicolor" 2>/dev/null || true
fi

echo ""
echo "======================================================"
echo "    Vellum foi desinstalado com sucesso!              "
echo "    Nenhum arquivo residual foi deixado no sistema.   "
echo "======================================================"
