# App de Tradução e Leitura Técnica

Leitor de PDFs desktop voltado para leitura técnica em inglês, com tradução direta via LLM local (Ollama) e módulo opcional de prática de pronúncia com Whisper.

Construído com frontend nativo em C++20/Qt6 e backend assíncrono em Python (FastAPI). Todo o processamento (OCR, tradução e reconhecimento de voz) roda localmente na máquina, sem envio de dados para serviços externos.

---

## Funcionalidades

- **Visualização de PDFs (QtPdf)**
  - Modos de exibição: página individual, duas páginas lado a lado (modo livro) e rolagem contínua.
  - Controles de zoom (ajuste por largura, página inteira, 100% e zoom manual).
- **Tradução por Seleção Direta**
  - Seleção visual retangular sobre o PDF.
  - Extração de texto via Tesseract OCR direto do framebuffer do leitor (compatível com Wayland e X11).
  - Tradução contextual para português via Ollama (`llama3` por padrão).
- **Módulo de Treino de Pronúncia (Opcional)**
  - Fica oculto por padrão para manter a leitura limpa; ativado pelo menu `Ferramentas > Habilitar Módulo de Treino & Pronúncia`.
  - Gravação de áudio pelo microfone.
  - Transcrição local com OpenAI Whisper (`small`).
  - Avaliação de correspondência fonética e correção via LLM.

---

## Arquitetura

O projeto divide-se em duas camadas:

- **Frontend (`qt_frontend/`)**: Aplicação desktop em C++20 com Qt6. Gerencia a renderização do PDF (`QPdfView`), a captura da seleção retangular do usuário e a interface gráfica. Comunica-se com o backend via requisições HTTP REST assíncronas.
- **Backend (`main.py`)**: Servidor local em Python rodando FastAPI e Uvicorn na porta 8000. Expõe endpoints para OCR, tradução com Ollama e pipeline de transcrição/avaliação com Whisper.

---

## Dependências

### Sistema (Linux / Arch / EndeavourOS)
- `cmake` (>= 3.16) e compilador C++20 (`gcc` ou `clang`)
- `qt6-base` e `qt6-pdf`
- `tesseract` e `tesseract-data-eng`
- `ollama` (com suporte a Vulkan/ROCm se usar GPU AMD, ou CUDA para NVIDIA)
- `portaudio` e `ffmpeg`

### Modelos Locais
Certifique-se de que o serviço do Ollama está rodando e com o modelo baixado:
```bash
ollama pull llama3
```

---

## Compilação e Execução

### 1. Configurar o ambiente Python
```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### 2. Compilar o frontend C++
```bash
cmake -B build_linux -S qt_frontend -DCMAKE_BUILD_TYPE=Release
cmake --build build_linux
```

### 3. Iniciar a aplicação
O repositório inclui um script que inicia o backend e o frontend juntos:
```bash
./iniciar_tudo.sh
```

Ou, se preferir rodar os processos separadamente em terminais distintos:
```bash
# Terminal 1: Backend
./iniciar_backend.sh

# Terminal 2: Frontend
./build_linux/EnsinadorDeIngles
```

---

## Configurações Opcionais

O backend aceita as seguintes variáveis de ambiente:

| Variável | Padrão | Descrição |
| :--- | :--- | :--- |
| `OLLAMA_MODEL` | `llama3` | Nome do modelo usado para tradução e avaliação |
| `OLLAMA_HOST` | `http://localhost:11434` | Endereço do serviço local do Ollama |

Exemplo de uso com outro modelo:
```bash
export OLLAMA_MODEL="qwen2.5:7b"
./iniciar_tudo.sh
```

---

## Atalhos de Teclado

| Atalho | Ação |
| :--- | :--- |
| `Ctrl + O` | Abrir documento PDF |
| `Ctrl + 1` | Modo página individual |
| `Ctrl + 2` | Modo duas páginas (livro) |
| `Ctrl + 3` | Modo rolagem contínua |
| `Ctrl + +` / `Ctrl + -` | Aumentar / Diminuir zoom |
| `Ctrl + 0` | Resetar zoom para 100% |
| `Ctrl + Q` | Sair da aplicação |

---

## Licença

MIT.
