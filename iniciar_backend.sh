#!/usr/bin/env bash
# ===================================================
#   Iniciando Backend - Ensinador de Ingles (Linux)
# ===================================================

cd "$(dirname "$0")"

# Ativa o ambiente virtual se existir
if [ -d "venv" ]; then
    source venv/bin/activate
fi

# Libera a porta 8000 se houver processo antigo preso
fuser -k 8000/tcp >/dev/null 2>&1
sleep 0.5

# Le o tier salvo pelo frontend (QSettings → INI via Python)
# Formato: org=EnsinadorDeIngles, app=LeitorTecnico
OLLAMA_TIER=$(python3 - <<'EOF'
try:
    import configparser, os, pathlib
    # Localiza o arquivo QSettings no XDG_CONFIG_HOME
    cfg_base = pathlib.Path(os.environ.get("XDG_CONFIG_HOME", pathlib.Path.home() / ".config"))
    ini = cfg_base / "EnsinadorDeIngles" / "LeitorTecnico.conf"
    if ini.exists():
        cfg = configparser.ConfigParser()
        cfg.read(str(ini))
        sec = "General"
        model = cfg.get(sec, "ollamamodelTier", fallback="llama3")
        whisper = cfg.get(sec, "whispermodelTier", fallback="base.en")
        print(f"{model}|{whisper}")
    else:
        print("llama3|base.en")
except Exception:
    print("llama3|base.en")
EOF
)

export OLLAMA_MODEL=$(echo "$OLLAMA_TIER" | cut -d'|' -f1)
export WHISPER_MODEL=$(echo "$OLLAMA_TIER" | cut -d'|' -f2)

echo "Tier selecionado: Ollama=${OLLAMA_MODEL}  Whisper=${WHISPER_MODEL}"

# Verifica se o Ollama esta ativo (apenas avisa, nao bloqueia)
if ! curl -s http://localhost:11434/api/tags >/dev/null 2>&1; then
    echo "[AVISO] Ollama nao responde em localhost:11434."
    echo "        Execute em outro terminal: ollama serve"
    echo "        E baixe o modelo escolhido: ollama pull ${OLLAMA_MODEL}"
    echo ""
fi

echo "Iniciando servidor FastAPI em http://localhost:8000..."
python3 -m uvicorn main:app --host 0.0.0.0 --port 8000 --reload
