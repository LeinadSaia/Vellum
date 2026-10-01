# 📖 Leitor Técnico & Tradutor Inteligente

<div align="center">

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/)
[![Qt6](https://img.shields.io/badge/Qt-6.x-41CD52?style=flat-square&logo=qt&logoColor=white)](https://www.qt.io/)
[![Python](https://img.shields.io/badge/Python-3.10%2B-3776AB?style=flat-square&logo=python&logoColor=white)](https://www.python.org/)
[![FastAPI](https://img.shields.io/badge/FastAPI-0.110%2B-009688?style=flat-square&logo=fastapi&logoColor=white)](https://fastapi.tiangolo.com/)
[![Ollama](https://img.shields.io/badge/Ollama-Local%20LLM-black?style=flat-square&logo=ollama&logoColor=white)](https://ollama.ai/)
[![Whisper](https://img.shields.io/badge/Whisper-Speech--to--Text-FF6F00?style=flat-square)](https://github.com/openai/whisper)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-555555?style=flat-square)](https://github.com/thnsm/App-de-tradu-o-e-leitura)

**Uma aplicação desktop de alta performance para leitura imersiva de documentos em inglês, com tradução técnica instantânea via IA local e módulo opcional de treino de pronúncia.**

</div>

---

## 💡 Sobre o Projeto

O **Leitor Técnico & Tradutor Inteligente** foi concebido para estudantes, pesquisadores e profissionais que precisam ler livros e papers em inglês com agilidade e fluidez.

Diferente de leitores de PDF convencionais ou extensões de navegador com tradução genérica, o aplicativo une a performance e o render nativo do **C++20 com Qt6** à inteligência contextual de **LLMs locais (Ollama)** e reconhecimento de voz (**OpenAI Whisper**), garantindo:
- **Privacidade total**: Seus documentos e áudios nunca saem do seu computador.
- **Traduções contextualizadas**: O modelo local traduz termos técnicos mantendo o sentido correto da frase.
- **Interface focada e sem distrações**: Tema escuro minimalista (*Dark Slate/Charcoal*) sem elementos distrativos ou clichês visuais.

---

## ✨ Principais Funcionalidades

### 📄 Visualizador de Documentos Nativo (Qt6)
- **Modos de Exibição Flexíveis**:
  - **Página Individual**: Foco máximo em um único conteúdo.
  - **Lado a Lado (Modo Livro)**: Exibe duas páginas simultaneamente com sincronia de navegação.
  - **Rolagem Contínua**: Navegação vertical fluida pelo documento inteiro.
- **Controles de Zoom**: Aumentar/diminuir, reset para 100%, ajuste automático à largura da janela ou à página inteira.
- **Navegação Rápida**: Controle numérico de páginas e atalhos de teclado.

### ⚡ Tradução Técnica Direta por Seleção
- **Seleção por Retângulo (Rubber Band)**: Selecione qualquer parágrafo ou frase diretamente na tela com o mouse.
- **OCR Integrado (Tesseract)**: Extração precisa de texto diretamente da renderização da página, compatível nativamente com **Wayland** e **X11/Windows**.
- **Tradução Automática Instantânea**: Ao soltar a seleção, o texto em inglês é submetido ao Ollama e a tradução técnica em português surge no painel lateral em poucos segundos.

### 🎙️ Módulo Opcional de Treino & Pronúncia (Oculto por Padrão)
- Ativável pelo menu superior esquerdo: `Ferramentas > Habilitar Módulo de Treino & Pronúncia`.
- **Reprodução**: Pratique a escuta do parágrafo selecionado.
- **Gravação de Voz**: Grave sua pronúncia com microfone.
- **Transcrição e Avaliação**: O modelo **OpenAI Whisper** transcreve seu áudio e a IA avalia sua acurácia fonética comparando com o texto original do documento.

---

## 🏗️ Arquitetura do Sistema

O software opera em uma arquitetura desacoplada e modular:

```text
┌────────────────────────────────────────────────────────┐
│               Frontend Desktop (C++ / Qt6)             │
│  - QPdfDocument & QPdfView (Renderizador nativo)       │
│  - Painel de Leitura & Seleção de Área (Rubber Band)   │
│  - Barra de Menus Nativa & Painel de Tradução          │
│  - NetworkManager (Qt Network HTTP assíncrono)         │
└───────────────────────────┬────────────────────────────┘
                            │ HTTP JSON / Port 8000
┌───────────────────────────▼────────────────────────────┐
│              Backend Local (Python / FastAPI)          │
│  - /traduzir         -> Tesseract OCR + LLM Local      │
│  - /limpar_ocr       -> Correção de ruído textual      │
│  - /iniciar_gravacao -> Gravação de áudio do mic       │
│  - /parar_gravacao   -> Whisper STT + LLM Avaliação    │
└─────────────┬───────────────────────────┬──────────────┘
              │                           │
┌─────────────▼─────────────┐ ┌───────────▼──────────────┐
│       Ollama (Local)      │ │   OpenAI Whisper (Local) │
│  llama3 (Vulkan / ROCm /  │ │   Modelo 'small' / PyTorch│
│  CUDA / CPU)              │ │                          │
└───────────────────────────┘ └──────────────────────────┘
```

---

## 🚀 Como Começar

### Pré-requisitos

1. **Compilador C++ e Qt6**:
   - `cmake` (>= 3.16), compilador C++20 (`g++`, `clang` ou `MSVC`).
   - Pacotes Qt6: `qt6-base`, `qt6-webengine` / `qt6-pdf`.
2. **Python**: Versão 3.10 ou superior.
3. **Tesseract OCR**: Com dados de idioma em inglês (`tessdata/eng`).
4. **Ollama**: Instalado e com o modelo desejado baixado (padrão: `llama3`):
   ```bash
   ollama pull llama3
   ```

---

### Instalação no Linux (EndeavourOS / Arch Linux)

1. **Instale as dependências do sistema**:
   ```bash
   sudo pacman -S base-devel cmake qt6-base qt6-pdf tesseract tesseract-data-eng ollama
   ```
   *(Opcional para aceleração por GPU AMD Radeon):*
   ```bash
   sudo pacman -S vulkan-radeon ollama-vulkan
   ```

2. **Clone o repositório**:
   ```bash
   git clone https://github.com/thnsm/App-de-tradu-o-e-leitura.git
   cd App-de-tradu-o-e-leitura
   ```

3. **Inicie o ambiente virtual Python**:
   ```bash
   python3 -m venv venv
   source venv/bin/activate
   pip install -r requirements.txt
   ```

4. **Compile o Frontend C++**:
   ```bash
   cmake -B build_linux -S qt_frontend -DCMAKE_BUILD_TYPE=Release
   cmake --build build_linux
   ```

5. **Execute tudo com um único comando**:
   ```bash
   ./iniciar_tudo.sh
   ```

---

### Instalação no Windows

1. Certifique-se de ter o **Python 3.10+**, o **Tesseract-OCR** instalado (em `C:\Program Files\Tesseract-OCR`) e o **Qt6** configurado via Qt Online Installer ou vcpkg.
2. Instale as dependências no terminal do Windows:
   ```cmd
   python -m venv venv
   venv\Scripts\activate
   pip install -r requirements.txt
   ```
3. Compile o executável através do CMake / Visual Studio.
4. Execute via script automatizado:
   ```cmd
   iniciar_tudo.bat
   ```

---

## ⌨️ Atalhos Úteis

| Atalho | Ação |
| :--- | :--- |
| `Ctrl + O` | Abrir novo documento PDF |
| `Ctrl + 1` | Alternar para visualização de Página Única |
| `Ctrl + 2` | Alternar para visualização Lado a Lado (Livro) |
| `Ctrl + 3` | Alternar para visualização de Rolagem Contínua |
| `Ctrl + +` / `Ctrl + -` | Aumentar / Diminuir Zoom |
| `Ctrl + 0` | Redefinir Zoom para 100% |
| `Menu Ferramentas` | Exibir / Ocultar Módulo de Treino & Pronúncia |

---

## ⚙️ Variáveis de Ambiente Opcionais

Você pode customizar o comportamento do backend definindo variáveis de ambiente antes da inicialização:

- `OLLAMA_MODEL`: Nome do modelo utilizado para tradução e avaliação (Padrão: `llama3`).
  *Exemplo:* `export OLLAMA_MODEL="mistral"` ou `export OLLAMA_MODEL="qwen2.5:7b"`
- `OLLAMA_HOST`: Endereço do servidor Ollama (Padrão: `http://localhost:11434`).

---

## 🛠️ Tecnologias Utilizadas

- **Frontend**: C++20, Qt6 (Widgets, Network, Pdf, PdfWidgets), CMake.
- **Backend API**: Python 3.10+, FastAPI, Uvicorn, Pydantic, asyncio.
- **Inteligência Artificial**: Ollama (LLaMA 3), OpenAI Whisper (`small`), PyTorch.
- **Processamento de Imagem & OCR**: Tesseract OCR, PyTesseract, Pillow.
- **Áudio**: PyAudio, SoundFile, NumPy.

---

## 📄 Licença

Distribuído sob a licença **MIT**. Consulte o arquivo `LICENSE` para mais informações.
