#!/usr/bin/env bash
# ==============================================================================
#   Vellum - Instalador Automatizado para Linux
#   Instala o binário, backend, atalho no menu do sistema, ícone e modelos.
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

INSTALL_PREFIX="${HOME}/.local"
BIN_DIR="${INSTALL_PREFIX}/bin"
OPT_DIR="${INSTALL_PREFIX}/share/vellum"
APP_DIR="${INSTALL_PREFIX}/share/applications"
ICON_DIR="${INSTALL_PREFIX}/share/icons/hicolor/512x512/apps"

AUTO_YES=false
if [[ "$1" == "-y" || "$1" == "--yes" ]]; then
    AUTO_YES=true
fi

echo "======================================================"
echo "          Instalador do Vellum (Linux)                "
echo "======================================================"
echo "Diretório de instalação: ${OPT_DIR}"
echo "Executável: ${BIN_DIR}/vellum"
echo ""

# 1. Compilação do Frontend se necessário
if [ ! -f "build_linux/Vellum" ]; then
    echo "[1/5] Compilando Frontend C++ (Qt6)..."
    if ! command -v cmake >/dev/null 2>&1; then
        echo -e "\n[ERRO] O comando 'cmake' não foi encontrado."
        echo "Para instalar o Vellum a partir do código fonte, instale as dependências executando o comando para o seu sistema:"
        echo ""
        echo "👉 Ubuntu / Debian / Pop!_OS / Mint:"
        echo "   sudo apt update && sudo apt install -y build-essential cmake qt6-base-dev qt6-multimedia-dev libqt6svg6-dev python3-venv tesseract-ocr"
        echo ""
        echo "👉 Fedora:"
        echo "   sudo dnf install -y gcc-c++ cmake qt6-qtbase-devel qt6-qtmultimedia-devel qt6-qtsvg-devel python3 tesseract"
        echo ""
        echo "👉 Arch Linux / Manjaro:"
        echo "   sudo pacman -S --needed base-devel cmake qt6-base qt6-multimedia qt6-svg python tesseract"
        echo ""
        echo "Após a instalação, tente rodar este script novamente."
        exit 1
    fi

    cmake -B build_linux -S qt_frontend -DCMAKE_BUILD_TYPE=Release
    cmake --build build_linux -j"$(nproc)"
else
    echo "[1/5] Frontend compilado detectado em build_linux/Vellum."
fi

# 2. Configuração do ambiente Python
echo "[2/5] Verificando dependências do backend..."
if [ ! -d "venv" ]; then
    echo "      Criando ambiente virtual Python..."
    python3 -m venv venv
    ./venv/bin/pip install --upgrade pip
    ./venv/bin/pip install -r requirements.txt
else
    echo "      Ambiente virtual Python detectado."
fi

# 3. Criação das pastas de destino
echo "[3/5] Copiando arquivos do aplicativo..."
mkdir -p "${BIN_DIR}" "${OPT_DIR}" "${APP_DIR}" "${ICON_DIR}"

# Copia arquivos essenciais para ~/.local/share/vellum
cp "build_linux/Vellum" "${OPT_DIR}/Vellum" 2>/dev/null || cp "Vellum" "${OPT_DIR}/Vellum"
if [ -f "vellum_backend" ]; then
    cp "vellum_backend" "${OPT_DIR}/vellum_backend"
elif [ -f "dist/vellum_backend" ]; then
    cp "dist/vellum_backend" "${OPT_DIR}/vellum_backend"
else
    cp "main.py" "${OPT_DIR}/main.py"
    cp "requirements.txt" "${OPT_DIR}/requirements.txt"
    cp -r "venv" "${OPT_DIR}/" 2>/dev/null || true
fi

# Copia ícone oficial
if [ -f "qt_frontend/resources/app_icon.png" ]; then
    cp "qt_frontend/resources/app_icon.png" "${ICON_DIR}/vellum.png"
elif [ -f "LOGO.png" ]; then
    cp "LOGO.png" "${ICON_DIR}/vellum.png"
fi

# 4. Criação do script launcher no ~/.local/bin/vellum
cat << 'EOF' > "${BIN_DIR}/vellum"
#!/usr/bin/env bash
BASE_DIR="${HOME}/.local/share/vellum"
cd "${BASE_DIR}"

export QT_QPA_PLATFORM="wayland;xcb"
exec "${BASE_DIR}/Vellum" "$@"
EOF
chmod +x "${BIN_DIR}/vellum"

# 5. Instalação do arquivo .desktop
cat << EOF > "${APP_DIR}/vellum.desktop"
[Desktop Entry]
Version=1.0
Type=Application
Name=Vellum
GenericName=Leitor Técnico e Tutor de Inglês
Comment=Leitor Técnico de PDF e Tutor Inteligente com IA
Exec=${BIN_DIR}/vellum %f
Icon=vellum
Terminal=false
MimeType=application/pdf;
Categories=Office;Education;Science;Viewer;
StartupWMClass=Vellum
Keywords=PDF;Reader;Technical;English;Translator;Ollama;Gemini;
EOF

# Atualiza bancos de dados do sistema se disponíveis
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${APP_DIR}" 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "${INSTALL_PREFIX}/share/icons/hicolor" 2>/dev/null || true
fi

# 6. Modelos e Dependências: Gerenciados agora pela própria UI do Vellum (C++)!
echo "[4/5] Modelos opcionais serão baixados via interface na primeira abertura."

echo ""
echo "======================================================"
echo "    Instalação concluída com sucesso! [5/5]           "
echo "======================================================"
echo "Você já pode:"
echo " 1. Abrir o Vellum pelo menu do seu sistema operacional."
echo " 2. Executar no terminal: vellum [arquivo.pdf]"
echo " 3. Abrir arquivos PDF com botão direito > Abrir com > Vellum."
echo " 4. Para desinstalar a qualquer momento: ./uninstall_linux.sh"
echo ""
