<p align="center">
  <img src="LOGO.png" alt="Vellum Logo" width="180"/>
</p>

<p align="center">
  <b>Leitor de PDFs desktop focado em literatura técnica (Engenharia, Computação e Ciências Exatas), com inteligência artificial integrada, tradução contextual e tutor de pronúncia.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/Qt-6.x-green.svg" alt="Qt6"/>
  <img src="https://img.shields.io/badge/Python-3.10%2B-yellow.svg" alt="Python"/>
  <img src="https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey.svg" alt="Cross-Platform"/>
</p>

---

## 🎯 Principais Funcionalidades

O **Vellum** tem a filosofia de foco total no documento. A interface principal é limpa para leitura e, através do atalho `F4`, um painel lateral de ferramentas inteligentes é ativado.

*   **🔍 Seleção Instantânea com OCR:** Desenhe um retângulo sobre qualquer parágrafo no PDF. O Vellum extrai o texto, normaliza quebras de linha e hífens, e traduz instantaneamente.
*   **💬 Chat Técnico com Suporte a LaTeX:** Converse com a IA sobre o texto do PDF, deduções matemáticas ou diagramas. Fórmulas e equações são renderizadas nativamente na interface (Markdown + LaTeX).
*   **🎙️ Tutor de Pronúncia:** Escute a pronúncia nativa dos trechos técnicos e grave a sua própria voz pelo microfone. O Vellum analisa o seu áudio e fornece um feedback construtivo de pronúncia.
*   **📝 Bloco de Notas Integrado:** Aba dedicada para escrever resumos e notas de estudo formatadas, com exportação fácil, sem precisar abrir outros aplicativos (`Ctrl + N`).
*   **🧠 Processamento Híbrido (Nuvem ou Local):** Funciona out-of-the-box conectado ao Gemini, mas também possui detecção automática do **Ollama** para rodar modelos de Inteligência Artificial 100% offline no seu próprio computador.

---

## 🚀 Instalação e Configuração

O aplicativo é divido em um Frontend de alta performance (C++/Qt6) e um Backend em Python responsável pelas integrações de IA e áudio.

### 🐧 Linux (Ubuntu, Debian, Fedora, Arch)

Para compilar e instalar o Vellum de forma automática e criar um ícone no seu menu de aplicativos, siga estes passos:

**1. Instale as dependências de compilação:**

*   **Ubuntu / Debian / Mint:**
    ```bash
    sudo apt update
    sudo apt install -y build-essential cmake qt6-base-dev qt6-multimedia-dev libqt6svg6-dev qt6-pdf-dev python3-venv tesseract-ocr
    ```
*   **Fedora:**
    ```bash
    sudo dnf install -y gcc-c++ cmake qt6-qtbase-devel qt6-qtmultimedia-devel qt6-qtsvg-devel qt6-qtwebengine-devel python3 tesseract
    ```
*   **Arch Linux:**
    ```bash
    sudo pacman -S --needed base-devel cmake qt6-base qt6-multimedia qt6-svg qt6-webengine python tesseract
    ```

**2. Clone o projeto e instale:**
```bash
git clone https://github.com/LeinadSaia/Vellum.git Vellum
cd Vellum
./install_linux.sh
```
Após o script finalizar, o atalho do Vellum já estará no menu do seu sistema. Caso precise desinstalar, basta executar o `./uninstall_linux.sh`.

---

### 🪟 Windows

Você tem duas opções para rodar no Windows:

*   **Download da Versão Pronta:** Baixe a versão mais recente na aba [Releases](https://github.com/LeinadSaia/Vellum/releases) do repositório. Existe uma versão portátil (em `.zip`, basta extrair e rodar o `Vellum.exe`) e um instalador executável padrão (`.exe`).
*   **Compilar Manualmente:** Caso queira gerar o build no seu computador, abra um terminal e execute o script fornecido na raiz do projeto:
    ```cmd
    build_windows_release.bat
    ```
    Isso compilará o C++ e empacotará o Python nativamente.

---

## ⚙️ Nuvem vs. Execução Local Offline

O Vellum se adapta ao poder de processamento da sua máquina. Ele pode rodar de duas formas principais:

1.  **Modo Nuvem (Leve e Rápido):** É o modo padrão. Basta inserir sua chave de API do Gemini nas configurações do Vellum. Ele consome mínima memória RAM (cerca de ~180MB) e não exige placa de vídeo dedicada.
2.  **Modo Local (100% Offline):** Para ativar, instale o **[Ollama](https://ollama.com/)** no seu computador. Quando o Vellum for aberto, ele detectará o Ollama em segundo plano. Ao escolher um modelo local (como `llama3.2` ou `phi3`), os downloads serão gerenciados automaticamente. O reconhecimento de fala offline e as avaliações de pronúncia usam a rede neural **Whisper**. Este modo é indicado para PCs com pelo menos 8GB de RAM.

---

## ⌨️ Atalhos Essenciais

| Tecla | Ação |
| :--- | :--- |
| `F4` | Abrir ou Recolher o painel lateral da IA |
| `Ctrl + O` | Abrir um arquivo PDF |
| `Ctrl + 1` | Visualização Padrão (Página Individual) |
| `Ctrl + 2` | Visualização em Modo Livro (Duas Páginas) |
| `Ctrl + 3` | Visualização em Rolagem Contínua |
| `Ctrl + +` / `-`| Aumentar ou Diminuir o Zoom |
| `Ctrl + N` | Ir diretamente para a aba do Bloco de Notas |
| `Esc` | Cancelar a ferramenta de seleção na tela |

---

## 📄 Licença

O Vellum é um projeto de código aberto sob a licença **MIT**. Sinta-se livre para estudar, modificar e distribuir.
