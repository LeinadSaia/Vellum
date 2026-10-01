#!/usr/bin/env bash
# ===================================================
#   Iniciando Backend - Ensinador de Inglês (Linux)
# ===================================================

cd "$(dirname "$0")"

# Ativa o ambiente virtual se existir
if [ -d "venv" ]; then
    echo "Ativando ambiente virtual (venv)..."
    source venv/bin/activate
fi

# Libera a porta 8000 se houver processo antigo preso
fuser -k 8000/tcp >/dev/null 2>&1
sleep 0.5

# Verifica se o Ollama está ativo
if ! curl -s http://localhost:11434/api/tags >/dev/null 2>&1; then
    echo "[AVISO] O Ollama não parece estar respondendo em http://localhost:11434."
    echo "        Se ainda não iniciou, execute em outro terminal: ollama serve"
    echo "        E certifique-se de ter o modelo baixado: ollama run llama3"
    echo ""
fi

echo "Iniciando servidor FastAPI em http://localhost:8000..."
python3 -m uvicorn main:app --host 0.0.0.0 --port 8000 --reload
