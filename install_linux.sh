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

# 6. Assistente de Modelos Opcionais (Modularidade de Instalação)
echo "[4/5] Configuração de Modelos Opcionais..."
if [ "$AUTO_YES" = false ] && [ -t 0 ]; then
    # Modelos Whisper (Voz)
    echo ""
    read -p "Deseja baixar os modelos Whisper para prática de fala offline (~210 MB)? [S/n]: " RESP_VOZ
    RESP_VOZ=${RESP_VOZ:-S}
    if [[ "$RESP_VOZ" =~ ^[sS]$ ]]; then
        echo "Baixando modelos Whisper para ~/.cache/whisper..."
        mkdir -p "${HOME}/.cache/whisper"
        if [ ! -f "${HOME}/.cache/whisper/base.en.pt" ]; then
            curl -L -o "${HOME}/.cache/whisper/base.en.pt" "https://openaipublic.azureedge.net/main/whisper/models/256150255c601dbb170ef89248a8458ad4cf37081fa97e5b2eb5de50d1da676e/base.en.pt"
        fi
        if [ ! -f "${HOME}/.cache/whisper/tiny.en.pt" ]; then
            curl -L -o "${HOME}/.cache/whisper/tiny.en.pt" "https://openaipublic.azureedge.net/main/whisper/models/d3dd57d32accea0b295c96e26691aa14d8822fac7d9d27d5dc00ba0adc22e8dd/tiny.en.pt"
        fi
        echo "Modelos Whisper prontos."
    fi

    # Modelos Ollama (IA Local)
    echo ""
    echo "Deseja baixar algum modelo de IA local offline via Ollama?"
    echo "  1) Básico:     phi3:mini   (~2.2 GB) — Ideal para CPUs comuns e 4-8 GB RAM"
    echo "  2) Equilibrado: llama3.2:3b (~2.0 GB) — Ideal para CPUs modernas e 8 GB RAM"
    echo "  3) Avançado:   llama3:8b   (~4.7 GB) — Para PCs potentes / GPU dedicada"
    echo "  4) Pular       (Usar apenas Nuvem / Gemini por padrão)"
    read -p "Escolha uma opção [1-4, padrão=4]: " OPT_OLLAMA
    OPT_OLLAMA=${OPT_OLLAMA:-4}

    case "$OPT_OLLAMA" in
        1)
            if command -v ollama >/dev/null 2>&1; then
                echo "Baixando phi3:mini via Ollama..."
                ollama pull phi3:mini || true
            else
                echo "[INFO] Ollama não detectado no sistema. Baixe em https://ollama.com e execute: ollama pull phi3:mini"
            fi
            ;;
        2)
            if command -v ollama >/dev/null 2>&1; then
                echo "Baixando llama3.2:3b via Ollama..."
                ollama pull llama3.2:3b || true
            else
                echo "[INFO] Ollama não detectado no sistema. Baixe em https://ollama.com e execute: ollama pull llama3.2:3b"
            fi
            ;;
        3)
            if command -v ollama >/dev/null 2>&1; then
                echo "Baixando llama3:8b via Ollama..."
                ollama pull llama3:8b || true
            else
                echo "[INFO] Ollama não detectado no sistema. Baixe em https://ollama.com e execute: ollama pull llama3:8b"
            fi
            ;;
        *)
            echo "Nenhum modelo Ollama selecionado. Modo Nuvem (Gemini) configurado como padrão."
            ;;
    esac
else
    echo "Modo automático / padrão ativado. Modelos pesados pulados."
fi

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
