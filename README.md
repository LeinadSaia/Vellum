# Vellum — Leitor Técnico & Tutor Inteligente de Inglês

<p align="center">
  <img src="LOGO.png" alt="Vellum Logo" width="180"/>
</p>

<p align="center">
  <b>Leitor de PDFs desktop de alta fidelidade voltado para literatura técnica em inglês (Engenharia, Computação e Ciências Exatas), com inteligência artificial integrada, renderização de fórmulas matemáticas, tradução contextual e tutor de pronúncia.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/Qt-6.x-green.svg" alt="Qt6"/>
  <img src="https://img.shields.io/badge/Python-3.10%2B-yellow.svg" alt="Python"/>
  <img src="https://img.shields.io/badge/FastAPI-0.100%2B-teal.svg" alt="FastAPI"/>
  <img src="https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey.svg" alt="Cross-Platform"/>
  <img src="https://img.shields.io/badge/License-MIT-purple.svg" alt="MIT License"/>
</p>

---

## Visão Geral

O **Vellum** foi desenvolvido sob a filosofia de **foco total no documento**. Quando o leitor é aberto, a tela é 100% limpa e dedicada à leitura. Com um único atalho (`F4`) ou seleção na tela, o painel lateral inteligente desliza suavemente oferecendo ferramentas profundas de estudo técnico:

- **Seleção Instantânea com OCR**: Desenhe um retângulo sobre qualquer parágrafo, fórmula ou diagrama. O texto é extraído pelo Tesseract OCR, normalizado sintaticamente (com des-hifenização de final de linha) e traduzido imediatamente.
- **Chat Técnico Multimodal com LaTeX**: Converse com a IA sobre circuitos, diagramas e deduções matemáticas. Suporte nativo a fórmulas em bloco (`$$...$$`), equações em linha (`$...$`), frações verticais, símbolos gregos e operadores lógicos (`\implies`, `\iff`, `==>`).
- **Tutor de Fala & Avaliação Fonética**: Ouça o trecho com vozes neurais de alta fidelidade (Edge TTS) e pratique sua pronúncia gravando pelo microfone. O Whisper transcreve sua fala e a IA avalia a similaridade fonética com feedback construtivo.
- **Bloco de Notas Independente**: Anote resumos e observações ao lado do livro com formatação de texto rápida (Negrito, Itálico, Sublinhado, seleção de cores, fontes, contador de palavras) e salvamento em arquivo HTML/TXT.

---

## Os 4 Níveis de Desempenho (Tiers de Hardware)

O Vellum adapta-se automaticamente a qualquer perfil de máquina, desde notebooks simples sem placa de vídeo até estações de trabalho dedicadas:

| Tier | Perfil | Motor Tradução / Chat | Motor Voz (Whisper) | Consumo RAM | Espaço em Disco (Modelos) |
| :---: | :--- | :--- | :--- | :---: | :---: |
| **0** | **Nuvem (Gemini)** *(Padrão)* | Gemini (`gemini-3.5-flash-lite`) | Edge TTS (Nuvem) | **~180 MB** | **0 GB** *(zero downloads locais)* |
| **1** | **Básico (Leve Local)** | Ollama `phi3:mini` (3.8B) | Whisper `tiny.en` | **~3 a 4 GB** | **~2.3 GB** *(2.2 GB Ollama + 73 MB Whisper)* |
| **2** | **Equilibrado** | Ollama `llama3.2:3b` (3B) | Whisper `base.en` | **~5 a 6 GB** | **~2.1 GB** *(2.0 GB Ollama + 139 MB Whisper)* |
| **3** | **Avançado (Dedicado)** | Ollama `llama3:8b` (8B) | Whisper `small.en` | **~9 a 12 GB** | **~5.2 GB** *(4.7 GB Ollama + 462 MB Whisper)* |

### Consumo Total em Disco Conforme a Sua Escolha

O aplicativo base (Frontend C++ compilado + Backend Python) ocupa apenas **~45 MB** (ou **~1.3 MB** no pacote compactado `.tar.gz` para download). Os modelos de IA e voz são opcionais e consomem conforme sua escolha:

| Cenário de Instalação | Modelos Instalados | Espaço em Disco Adicional | Consumo de RAM em Execução | Indicado Para |
| :--- | :--- | :---: | :---: | :--- |
| **Apenas Nuvem (Sem IA Local)** | Nenhum (Gemini + Edge TTS) | **0 GB** | **~180 MB** | Qualquer notebook básico, computadores corporativos ou com pouco espaço livre |
| **Apenas 1 Modelo Local (Básico)** | `phi3:mini` + `tiny.en` | **~2.3 GB** | **~3.5 GB** | PCs com 8 GB de RAM sem GPU dedicada (100% offline) |
| **Apenas 1 Modelo Local (Equilibrado)** | `llama3.2:3b` + `base.en` | **~2.1 GB** | **~5.5 GB** | PCs modernos com 8 GB a 16 GB de RAM |
| **Apenas 1 Modelo Local (Avançado)** | `llama3:8b` + `small.en` | **~5.2 GB** | **~10 GB** | PCs de alto desempenho com 16 GB+ de RAM ou placa de vídeo NVIDIA/AMD dedicada |
| **Todos os Modelos Instalados** | `phi3:mini` + `llama3.2:3b` + `llama3:8b` + todos os Whisper | **~9.6 GB** | **~180 MB a 10 GB** *(varia pelo tier ativo)* | Desenvolvedores ou quem deseja alternar livremente entre todos os modos offline |

> **Pré-requisito para Modelos Locais (Ollama)**: Os modelos de reconhecimento de fala Whisper (tiny.en, base.en, small.en) são baixados diretamente e funcionam de forma autônoma. Já os modelos de Inteligência Artificial local (phi3:mini, llama3.2:3b e llama3:8b) rodam através do servidor Ollama. Portanto, o download e a execução desses modelos de IA só ocorrerão se você tiver o Ollama previamente instalado no seu computador (disponível gratuitamente em https://ollama.com). Se o Ollama não estiver instalado, o instalador ignorará esses downloads e o Vellum funcionará normalmente no modo Nuvem (Gemini).

> **Degradação Graciosa (Graceful Degradation)**: Se você não tiver o Ollama ou o Whisper instalados, o Vellum opera perfeitamente no modo **Nuvem (Gemini)** sem travar ou emitir erros de inicialização.

---

## Arquitetura do Sistema

O projeto adota uma arquitetura desacoplada em duas camadas:

```
┌─────────────────────────────────────────────────────────────┐
│                    FRONTEND (C++17 / Qt6)                   │
│  - Renderização de PDF via QPdf / QPdfPageNavigator         │
│  - Captura RubberBand para OCR em Wayland / X11 / Windows   │
│  - Renderizador rico de Markdown, LaTeX e Tabelas           │
│  - Bloco de notas formatado & gerenciador de atalhos        │
└──────────────────────────────┬──────────────────────────────┘
                               │ Requisições REST Assíncronas
                               ▼ (porta local 8000)
┌─────────────────────────────────────────────────────────────┐
│                 BACKEND (Python / FastAPI)                  │
│  - Lazy Loading do Whisper (boot ultra rápido em ~1.5s)     │
│  - Extração OCR via Tesseract (com detecção local/sistema)  │
│  - Motor Gemini API com pool de conexões persistentes HTTP  │
│  - Integração com Ollama Local                              │
│  - Síntese Neural Edge-TTS + Áudio via SoundDevice          │
└─────────────────────────────────────────────────────────────┘
```

---

## Instalação e Execução

### No Linux (Ubuntu, Debian, Fedora, Arch, etc.)

#### Opção A: Instalador Automatizado (Recomendado)
Para integrar o Vellum diretamente no menu do seu sistema operacional (`.desktop`), registrar o ícone e associar arquivos PDF:
```bash
git clone https://github.com/thnsm/App-de-tradu-o-e-leitura.git Vellum
cd Vellum
./install_linux.sh
```
Após isso, basta abrir o **Vellum** pelo lançador de aplicativos do sistema ou digitar no terminal:
```bash
vellum livro.pdf
```

#### Opção B: Execução Portátil Rápida (Sem Instalar)
```bash
./iniciar_tudo.sh
```

#### Como Desinstalar no Linux
Diferente de outros aplicativos, o Vellum preza por deixar o seu sistema limpo. Para desinstalar completamente:
```bash
./uninstall_linux.sh
```
O script remove o executável, atalhos, ícones e oferece a opção de remover as configurações salvas, sem deixar nenhum arquivo residual.

---

### No Windows 10 / 11

#### Opção A: Versão Portátil ZIP (Recomendada — Baixe, extraia e use)
Baixe o arquivo compactado `Vellum-v1.0.1-Windows-x86_64.zip` disponível na aba [Releases](https://github.com/thnsm/App-de-tradu-o-e-leitura/releases):
1. Extraia o arquivo `.zip` para qualquer pasta de sua preferência (ex: em `Documentos` ou `Área de Trabalho`).
2. Abra a pasta extraída e dê dois cliques em `Vellum.exe`.
3. Essa versão não requer direitos de administrador, não instala nada no sistema operacional e não sofre bloqueios de políticas restritivas do Smart App Control do Windows 11.

#### Opção B: Instalador Clássico (`Vellum-Setup-Windows-v1.0.1.exe`)
Baixe a versão executável disponível na aba [Releases](https://github.com/thnsm/App-de-tradu-o-e-leitura/releases) do repositório e execute o instalador. Ele configurará:
- Atalhos na Área de Trabalho e no Menu Iniciar.
- Opção de associar o Vellum para abrir arquivos `.pdf` com dois cliques.
- Backend em segundo plano 100% invisível (sem janelas pretas de console).
- Suporte a seleção modular de modelos durante a instalação (lembre-se: para selecionar modelos de IA Ollama, é necessário ter o Ollama instalado no computador).

> **Aviso sobre o Windows SmartScreen**: Como o Vellum é um projeto de código aberto recém-compilado e distribuído de forma independente (sem certificado comercial de assinatura de código pago), o Windows pode exibir a tela azul "O Windows protegeu o seu computador". Trata-se de um comportamento padrão do sistema para novos binários sem reputação acumulada. Para prosseguir normalmente, clique em **"Mais informações"** e depois no botão **"Executar assim mesmo"**.

#### Opção C: Executar via Script
Se clonou o código-fonte no Windows:
```cmd
iniciar_tudo.bat
```

#### Como Desinstalar no Windows
O Vellum cria um desinstalador dedicado e transparente:
- Acesse o Menu Iniciar > pasta **Vellum** > clique em **Desinstalar Vellum**.
- Ou abra `Configurações do Windows > Aplicativos > Aplicativos Instalados > Vellum > Desinstalar`.
- O desinstalador encerra processos em execução, remove toda a pasta da aplicação e pergunta se deseja limpar dados residuais do registro para uma limpeza de 100%.

---

## Como Gerar as Releases para o GitHub

Se você é o mantenedor e deseja gerar os pacotes para publicar no GitHub Releases:

### Gerar Pacote Linux:
Execute no terminal:
```bash
./build_linux_release.sh
```
O pacote **`dist_installer/Vellum-v1.0.0-Linux-x86_64.tar.gz`** será criado pronto para ser anexado à Release.

### Gerar Instalador Windows:
Em uma máquina Windows (ou no GitHub Actions):
```cmd
build_windows_release.bat
```
Ele compila o frontend, executa o `windeployqt`, empacota o backend com PyInstaller e gera o instalador **`dist_installer/Vellum-Setup-Windows-v1.0.0.exe`** via Inno Setup.

---

## Atalhos de Teclado Essenciais

| Atalho | Ação |
| :--- | :--- |
| `F4` | Abrir / Recolher painel lateral de ferramentas |
| `←` / `→` | Página anterior / Próxima página |
| `PageUp` / `PageDown` | Navegar páginas no modo contínuo ou livro |
| `Ctrl + O` | Abrir arquivo PDF |
| `Ctrl + 1` | Visualização em Página Individual |
| `Ctrl + 2` | Visualização em Duas Páginas (Modo Livro) |
| `Ctrl + 3` | Visualização em Rolagem Contínua |
| `Ctrl + +` / `Ctrl + -` | Aumentar / Diminuir Zoom |
| `Ctrl + 0` | Resetar Zoom para 100% |
| `Ctrl + N` | Alternar diretamente para o Bloco de Notas |
| `Esc` | Cancelar seleção retangular na tela |
| `Ctrl + Q` | Fechar o Vellum |

---

## Variáveis de Ambiente e Configurações Opcionais

O Vellum salva suas preferências e chaves de API automaticamente nas configurações do sistema. Caso prefira configurar via terminal:

| Variável | Padrão | Descrição |
| :--- | :--- | :--- |
| `GEMINI_API_KEY` | *(Vazia)* | Chave de API do Google Gemini (para Tier 0 Nuvem) |
| `GEMINI_TRANSLATE_MODEL`| `gemini-3.5-flash-lite` | Modelo padrão de altíssima velocidade para traduções |
| `OLLAMA_MODEL` | `llama3` | Modelo padrão para inferência local com Ollama |
| `WHISPER_MODEL` | `base.en` | Modelo padrão para transcrição e fonética (`tiny.en`, `base.en`, `small.en`) |

---

## Licença

Distribuído sob a licença **MIT**. Consulte `LICENSE` para mais detalhes.
