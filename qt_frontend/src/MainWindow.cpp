#include "MainWindow.h"
#include "NetworkManager.h"

#include <QApplication>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollBar>
#include <QScreen>
#include <QBuffer>
#include <QPixmap>
#include <QPainter>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QClipboard>
#include <QPdfPageNavigator>
#include <QMessageBox>
#include <QDateTime>
#include <QKeySequence>

// ═════════════════════════════════════════════════════════════════════════════
// Construtor
// ═════════════════════════════════════════════════════════════════════════════

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Leitor Técnico de Documentos");
    setMinimumSize(1150, 720);
    showMaximized();

    m_net = new NetworkManager("http://localhost:8000", this);

    setupMenuBar();
    setupUi();
    setupStyleSheet();
    connectSignals();

    m_net->verificarConexao();
}

// ═════════════════════════════════════════════════════════════════════════════
// Barra de Menus Superior (Nativa e Discreta)
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::setupMenuBar()
{
    auto *menuBar = this->menuBar();

    // ── Menu Arquivo ──────────────────────────────────────────────────────
    auto *menuArquivo = menuBar->addMenu("&Arquivo");

    auto *actAbrir = menuArquivo->addAction("Abrir Documento...");
    actAbrir->setShortcut(QKeySequence::Open);
    connect(actAbrir, &QAction::triggered, this, &MainWindow::onAbrirPdf);

    auto *actFechar = menuArquivo->addAction("Fechar Documento");
    connect(actFechar, &QAction::triggered, this, &MainWindow::onFecharPdf);

    menuArquivo->addSeparator();

    auto *actSair = menuArquivo->addAction("Sair");
    actSair->setShortcut(QKeySequence::Quit);
    connect(actSair, &QAction::triggered, this, &QWidget::close);

    // ── Menu Exibir ───────────────────────────────────────────────────────
    auto *menuExibir = menuBar->addMenu("&Exibir");

    auto *actModo1 = menuExibir->addAction("Página Individual");
    actModo1->setShortcut(QKeySequence("Ctrl+1"));
    connect(actModo1, &QAction::triggered, this, &MainWindow::onModo1PagClicado);

    auto *actModo2 = menuExibir->addAction("Duas Páginas (Livro)");
    actModo2->setShortcut(QKeySequence("Ctrl+2"));
    connect(actModo2, &QAction::triggered, this, &MainWindow::onModo2PagClicado);

    auto *actModoCont = menuExibir->addAction("Rolagem Contínua");
    actModoCont->setShortcut(QKeySequence("Ctrl+3"));
    connect(actModoCont, &QAction::triggered, this, &MainWindow::onModoContClicado);

    menuExibir->addSeparator();

    auto *actZoomMais = menuExibir->addAction("Aumentar Zoom");
    actZoomMais->setShortcut(QKeySequence::ZoomIn);
    connect(actZoomMais, &QAction::triggered, this, &MainWindow::onZoomMais);

    auto *actZoomMenos = menuExibir->addAction("Diminuir Zoom");
    actZoomMenos->setShortcut(QKeySequence::ZoomOut);
    connect(actZoomMenos, &QAction::triggered, this, &MainWindow::onZoomMenos);

    auto *actZoomReset = menuExibir->addAction("Tamanho Real (100%)");
    actZoomReset->setShortcut(QKeySequence("Ctrl+0"));
    connect(actZoomReset, &QAction::triggered, this, &MainWindow::onZoomReset);

    auto *actFitWidth = menuExibir->addAction("Ajustar à Largura");
    connect(actFitWidth, &QAction::triggered, this, &MainWindow::onAjustarLargura);

    auto *actFitPage = menuExibir->addAction("Ajustar à Página");
    connect(actFitPage, &QAction::triggered, this, &MainWindow::onAjustarPagina);

    // ── Menu Ferramentas ──────────────────────────────────────────────────
    auto *menuFerramentas = menuBar->addMenu("&Ferramentas");

    m_actModoTreino = menuFerramentas->addAction("Habilitar Módulo de Treino & Pronúncia");
    m_actModoTreino->setCheckable(true);
    m_actModoTreino->setChecked(false); // Oculto por padrão
    connect(m_actModoTreino, &QAction::toggled, this, &MainWindow::onToggleModuloTreino);
}

// ═════════════════════════════════════════════════════════════════════════════
// Construção da Interface
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::setupUi()
{
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(3);
    setCentralWidget(splitter);

    // ── Painel Esquerdo: Toolbar + Visualizador(es) de PDF ────────────────
    auto *painelEsq = new QWidget(this);
    painelEsq->setObjectName("painelEsquerdo");
    auto *layoutEsq = new QVBoxLayout(painelEsq);
    layoutEsq->setContentsMargins(0, 0, 0, 0);
    layoutEsq->setSpacing(0);

    layoutEsq->addWidget(criarBarraVisualizacao());

    auto *viewContainer = new QWidget(painelEsq);
    viewContainer->setObjectName("viewContainer");
    auto *viewLayout = new QHBoxLayout(viewContainer);
    viewLayout->setContentsMargins(0, 0, 0, 0);
    viewLayout->setSpacing(2);

    m_pdfDoc = new QPdfDocument(this);

    // Visualizador principal (Esquerda / Único)
    m_pdfView = new QPdfView(viewContainer);
    m_pdfView->setDocument(m_pdfDoc);
    m_pdfView->setPageMode(QPdfView::PageMode::SinglePage);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    m_pdfView->viewport()->installEventFilter(this);

    // Visualizador secundário (Direita em modo 2 páginas)
    m_pdfView2 = new QPdfView(viewContainer);
    m_pdfView2->setDocument(m_pdfDoc);
    m_pdfView2->setPageMode(QPdfView::PageMode::SinglePage);
    m_pdfView2->setZoomMode(QPdfView::ZoomMode::FitInView);
    m_pdfView2->setVisible(false);
    m_pdfView2->viewport()->installEventFilter(this);

    m_activePdfView = m_pdfView;

    viewLayout->addWidget(m_pdfView, 1);
    viewLayout->addWidget(m_pdfView2, 1);

    layoutEsq->addWidget(viewContainer, 1);

    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, m_pdfView->viewport());

    splitter->addWidget(painelEsq);

    // ── Painel Direito: Tradução Direta & Controles ───────────────────────
    auto *painelDir = new QFrame(this);
    painelDir->setObjectName("painelDireito");
    auto *layoutDir = new QVBoxLayout(painelDir);
    layoutDir->setContentsMargins(16, 16, 16, 16);
    layoutDir->setSpacing(10);

    // Cabeçalho sóbrio
    auto *titulo = new QLabel("Tradução & Análise", painelDir);
    titulo->setObjectName("tituloPainel");
    layoutDir->addWidget(titulo);

    auto *subtitulo = new QLabel("Selecione um parágrafo para tradução técnica imediata", painelDir);
    subtitulo->setObjectName("subtituloPainel");
    layoutDir->addWidget(subtitulo);

    layoutDir->addSpacing(4);

    // Controles Principais
    auto *layoutBotoesTop = new QHBoxLayout();
    layoutBotoesTop->setSpacing(8);

    m_btnAbrir = new QPushButton("Abrir Arquivo...", painelDir);
    m_btnAbrir->setObjectName("btnAbrir");
    layoutBotoesTop->addWidget(m_btnAbrir);

    m_btnModoCaptura = new QPushButton("Modo Seleção", painelDir);
    m_btnModoCaptura->setObjectName("btnCaptura");
    m_btnModoCaptura->setCheckable(true);
    layoutBotoesTop->addWidget(m_btnModoCaptura);

    layoutDir->addLayout(layoutBotoesTop);

    m_btnTraduzir = new QPushButton("Traduzir Seleção", painelDir);
    m_btnTraduzir->setObjectName("btnTraduzir");
    m_btnTraduzir->setEnabled(false);
    layoutDir->addWidget(m_btnTraduzir);

    m_chkTraducaoAuto = new QCheckBox("Traduzir automaticamente ao selecionar", painelDir);
    m_chkTraducaoAuto->setObjectName("chkAuto");
    m_chkTraducaoAuto->setChecked(true);
    layoutDir->addWidget(m_chkTraducaoAuto);

    layoutDir->addSpacing(6);

    // Caixa de Tradução Direta
    auto *lblTrad = new QLabel("Texto e Tradução:", painelDir);
    lblTrad->setObjectName("lblSecao");
    layoutDir->addWidget(lblTrad);

    m_txtTraducao = new QTextEdit(painelDir);
    m_txtTraducao->setObjectName("txtTraducao");
    m_txtTraducao->setReadOnly(true);
    m_txtTraducao->setMinimumHeight(220);
    m_txtTraducao->setPlaceholderText("O texto selecionado no documento aparecerá traduzido aqui...");
    layoutDir->addWidget(m_txtTraducao, 1);

    // ── Módulo Opcional de Treino (Oculto por Padrão) ─────────────────────
    m_containerTreino = new QWidget(painelDir);
    m_containerTreino->setObjectName("containerTreino");
    auto *layoutTreino = new QVBoxLayout(m_containerTreino);
    layoutTreino->setContentsMargins(0, 8, 0, 0);
    layoutTreino->setSpacing(6);

    auto *sepTreino = new QFrame(m_containerTreino);
    sepTreino->setFrameShape(QFrame::HLine);
    sepTreino->setObjectName("separador");
    layoutTreino->addWidget(sepTreino);

    auto *lblTreinoHeader = new QLabel("Módulo de Treino & Pronúncia:", m_containerTreino);
    lblTreinoHeader->setObjectName("lblSecao");
    layoutTreino->addWidget(lblTreinoHeader);

    auto *botoesTreino = new QHBoxLayout();
    botoesTreino->setSpacing(6);

    m_btnOuvir = new QPushButton("Ouvir Pronúncia", m_containerTreino);
    m_btnOuvir->setObjectName("btnSecundario");
    m_btnOuvir->setEnabled(false);
    botoesTreino->addWidget(m_btnOuvir);

    m_btnGravar = new QPushButton("Gravar Voz", m_containerTreino);
    m_btnGravar->setObjectName("btnGravar");
    m_btnGravar->setEnabled(false);
    botoesTreino->addWidget(m_btnGravar);

    m_btnLimparOcr = new QPushButton("Extrair Texto", m_containerTreino);
    m_btnLimparOcr->setObjectName("btnSecundario");
    m_btnLimparOcr->setEnabled(false);
    botoesTreino->addWidget(m_btnLimparOcr);

    layoutTreino->addLayout(botoesTreino);

    m_containerTreino->setVisible(false); // Inicia oculto!
    layoutDir->addWidget(m_containerTreino);

    // Histórico / Registro
    auto *lblLog = new QLabel("Registro de Atividades:", painelDir);
    lblLog->setObjectName("lblSecao");
    layoutDir->addWidget(lblLog);

    m_logBox = new QTextEdit(painelDir);
    m_logBox->setObjectName("logBox");
    m_logBox->setReadOnly(true);
    m_logBox->setMaximumHeight(100);
    layoutDir->addWidget(m_logBox);

    // Barra de status inferior
    m_lblStatus = new QLabel("Pronto", painelDir);
    m_lblStatus->setObjectName("lblStatus");
    layoutDir->addWidget(m_lblStatus);

    splitter->addWidget(painelDir);

    splitter->setStretchFactor(0, 70);
    splitter->setStretchFactor(1, 30);
}

// ─────────────────────────────────────────────────────────────────────────────

QWidget* MainWindow::criarBarraVisualizacao()
{
    auto *bar = new QFrame(this);
    bar->setObjectName("barraVisualizacao");
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(8);

    // ── Navegação ─────────────────────────────────────────────────────────
    m_btnPagAnterior = new QPushButton("Anterior", bar);
    m_btnPagAnterior->setObjectName("btnNav");
    m_btnPagAnterior->setEnabled(false);

    auto *lblPagTexto = new QLabel("Página", bar);
    lblPagTexto->setObjectName("lblToolbar");

    m_spinPagina = new QSpinBox(bar);
    m_spinPagina->setObjectName("spinPagina");
    m_spinPagina->setMinimum(1);
    m_spinPagina->setMaximum(1);
    m_spinPagina->setValue(1);
    m_spinPagina->setEnabled(false);

    m_lblTotalPag = new QLabel("/ 0", bar);
    m_lblTotalPag->setObjectName("lblToolbar");

    m_btnPagProxima = new QPushButton("Próxima", bar);
    m_btnPagProxima->setObjectName("btnNav");
    m_btnPagProxima->setEnabled(false);

    layout->addWidget(m_btnPagAnterior);
    layout->addWidget(lblPagTexto);
    layout->addWidget(m_spinPagina);
    layout->addWidget(m_lblTotalPag);
    layout->addWidget(m_btnPagProxima);

    auto *vsep1 = new QFrame(bar);
    vsep1->setFrameShape(QFrame::VLine);
    vsep1->setObjectName("vseparador");
    layout->addWidget(vsep1);

    // ── Modos de Visualização ──────────────────────────────────────────
    m_btnModo1Pag = new QPushButton("Individual", bar);
    m_btnModo1Pag->setObjectName("btnModoVis");
    m_btnModo1Pag->setCheckable(true);
    m_btnModo1Pag->setChecked(true);
    m_btnModo1Pag->setToolTip("Uma página por vez");

    m_btnModo2Pag = new QPushButton("Lado a Lado", bar);
    m_btnModo2Pag->setObjectName("btnModoVis");
    m_btnModo2Pag->setCheckable(true);
    m_btnModo2Pag->setToolTip("Duas páginas lado a lado (modo livro)");

    m_btnModoCont = new QPushButton("Contínuo", bar);
    m_btnModoCont->setObjectName("btnModoVis");
    m_btnModoCont->setCheckable(true);
    m_btnModoCont->setToolTip("Rolagem vertical contínua");

    layout->addWidget(m_btnModo1Pag);
    layout->addWidget(m_btnModo2Pag);
    layout->addWidget(m_btnModoCont);

    auto *vsep2 = new QFrame(bar);
    vsep2->setFrameShape(QFrame::VLine);
    vsep2->setObjectName("vseparador");
    layout->addWidget(vsep2);

    // ── Zoom ──────────────────────────────────────────────────────────────
    m_btnZoomMenos = new QPushButton("-", bar);
    m_btnZoomMenos->setObjectName("btnNav");
    m_btnZoomMenos->setToolTip("Diminuir Zoom");

    m_btnZoomReset = new QPushButton("100%", bar);
    m_btnZoomReset->setObjectName("btnZoomReset");
    m_btnZoomReset->setToolTip("Redefinir Zoom para 100%");

    m_btnZoomMais = new QPushButton("+", bar);
    m_btnZoomMais->setObjectName("btnNav");
    m_btnZoomMais->setToolTip("Aumentar Zoom");

    m_btnAjustarLarg = new QPushButton("Largura", bar);
    m_btnAjustarLarg->setObjectName("btnNav");
    m_btnAjustarLarg->setToolTip("Ajustar à Largura");

    m_btnAjustarPag = new QPushButton("Página", bar);
    m_btnAjustarPag->setObjectName("btnNav");
    m_btnAjustarPag->setToolTip("Ajustar à Página Inteira");

    layout->addWidget(m_btnZoomMenos);
    layout->addWidget(m_btnZoomReset);
    layout->addWidget(m_btnZoomMais);
    layout->addWidget(m_btnAjustarLarg);
    layout->addWidget(m_btnAjustarPag);

    layout->addStretch();
    return bar;
}

// ═════════════════════════════════════════════════════════════════════════════
// Folha de Estilos (Design Sóbrio, Minimalista e Profissional)
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::setupStyleSheet()
{
    setStyleSheet(R"(
        /* ── Janela Principal e Menus ─────────────────────────────── */
        QMainWindow {
            background-color: #16171a;
            color: #e5e7eb;
        }

        QMenuBar {
            background-color: #16171a;
            color: #d1d5db;
            border-bottom: 1px solid #27282d;
            font-size: 12px;
            padding: 2px 6px;
        }
        QMenuBar::item {
            padding: 4px 8px;
            background: transparent;
            border-radius: 4px;
        }
        QMenuBar::item:selected {
            background-color: #27282d;
            color: #ffffff;
        }

        QMenu {
            background-color: #1e1f24;
            color: #e5e7eb;
            border: 1px solid #2f3138;
            border-radius: 6px;
            padding: 4px;
            font-size: 12px;
        }
        QMenu::item {
            padding: 6px 20px 6px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #2b2d35;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background-color: #2b2d35;
            margin: 4px 0;
        }

        /* ── Áreas do Documento ───────────────────────────────────── */
        #painelEsquerdo, #viewContainer, QPdfView {
            background-color: #1a1b1f;
            border: none;
        }

        #barraVisualizacao {
            background-color: #16171a;
            border-bottom: 1px solid #27282d;
        }

        #lblToolbar {
            color: #9ca3af;
            font-size: 12px;
        }

        #vseparador {
            color: #27282d;
            max-width: 1px;
            margin: 2px 6px;
        }

        /* ── Botões da Toolbar ────────────────────────────────────── */
        #btnNav {
            background-color: #202227;
            color: #d1d5db;
            border: 1px solid #2d3038;
            border-radius: 4px;
            padding: 4px 10px;
            font-size: 11px;
        }
        #btnNav:hover   { background-color: #292c33; color: #ffffff; border-color: #3f434d; }
        #btnNav:pressed { background-color: #1b1c20; }
        #btnNav:disabled { background-color: #16171a; color: #4b515d; border-color: #202227; }

        #btnZoomReset {
            background-color: #202227;
            color: #e5e7eb;
            border: 1px solid #2d3038;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 11px;
            min-width: 44px;
        }
        #btnZoomReset:hover { background-color: #292c33; border-color: #3f434d; }

        #btnModoVis {
            background-color: #202227;
            color: #9ca3af;
            border: 1px solid #2d3038;
            border-radius: 4px;
            padding: 4px 10px;
            font-size: 11px;
        }
        #btnModoVis:hover   { background-color: #292c33; color: #ffffff; }
        #btnModoVis:checked {
            background-color: #2d3139;
            color: #ffffff;
            border-color: #4b5260;
            font-weight: 600;
        }

        #spinPagina {
            background-color: #202227;
            color: #ffffff;
            border: 1px solid #2d3038;
            border-radius: 4px;
            padding: 2px 6px;
            font-size: 11px;
            min-width: 50px;
        }

        /* ── Painel Direito (Controles) ──────────────────────────── */
        #painelDireito {
            background-color: #18191d;
            border-left: 1px solid #27282d;
        }

        #tituloPainel {
            font-size: 15px;
            font-weight: 600;
            color: #f3f4f6;
        }

        #subtituloPainel {
            font-size: 11px;
            color: #838894;
        }

        #lblSecao {
            color: #9ca3af;
            font-size: 11px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        #separador {
            color: #27282d;
        }

        /* ── Botões de Ação do Painel Direito ─────────────────────── */
        QPushButton {
            background-color: #22242a;
            color: #e5e7eb;
            border: 1px solid #2f333c;
            border-radius: 6px;
            padding: 8px 12px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #2a2c35;
            border-color: #404552;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #1c1d22;
        }
        QPushButton:disabled {
            background-color: #18191d;
            color: #444955;
            border-color: #22242a;
        }

        #btnAbrir {
            background-color: #22242a;
        }

        #btnCaptura:checked {
            background-color: #2e3340;
            color: #93c5fd;
            border-color: #3b82f6;
            font-weight: 600;
        }

        #btnTraduzir {
            background-color: #1d4ed8;
            color: #ffffff;
            border: 1px solid #2563eb;
            font-weight: 600;
        }
        #btnTraduzir:hover {
            background-color: #2563eb;
            border-color: #3b82f6;
        }
        #btnTraduzir:disabled {
            background-color: #1c202a;
            color: #434c5f;
            border-color: #222735;
        }

        #chkAuto {
            color: #9ca3af;
            font-size: 11px;
        }
        #chkAuto::indicator {
            width: 14px; height: 14px;
            border-radius: 3px;
            border: 1px solid #3b404d;
            background-color: #202227;
        }
        #chkAuto::indicator:checked {
            background-color: #2563eb;
            border-color: #2563eb;
        }

        #btnSecundario {
            background-color: #202227;
            font-size: 11px;
            padding: 6px 10px;
        }

        #btnGravar {
            background-color: #831843;
            color: #ffffff;
            border: 1px solid #9d174d;
            font-size: 11px;
            padding: 6px 10px;
        }
        #btnGravar:hover {
            background-color: #9d174d;
        }

        /* ── Caixas de Texto (Tradução e Log) ─────────────────────── */
        #txtTraducao {
            background-color: #141518;
            color: #e5e7eb;
            border: 1px solid #26282f;
            border-radius: 6px;
            padding: 10px;
            font-size: 13px;
            line-height: 1.5;
        }

        #logBox {
            background-color: #141518;
            color: #8b92a0;
            border: 1px solid #26282f;
            border-radius: 6px;
            padding: 6px;
            font-family: monospace;
            font-size: 11px;
        }

        #lblStatus {
            font-size: 11px;
            color: #6b7280;
            padding: 2px;
        }

        /* ── Barra de Rolagem ─────────────────────────────────────── */
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #141518;
            width: 6px;
            height: 6px;
            margin: 0;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #2c2f38;
            border-radius: 3px;
            min-height: 20px;
        }
        QScrollBar::handle:hover {
            background: #3e424e;
        }
    )");
}

// ═════════════════════════════════════════════════════════════════════════════
// Conexão de Sinais
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::connectSignals()
{
    // Ações principais
    connect(m_btnAbrir,        &QPushButton::clicked, this, &MainWindow::onAbrirPdf);
    connect(m_btnModoCaptura,  &QPushButton::toggled, this, &MainWindow::onToggleModoCaptura);
    connect(m_btnTraduzir,     &QPushButton::clicked, this, &MainWindow::onTraduzirDireto);

    // Módulo opcional de treino
    connect(m_btnGravar,       &QPushButton::clicked, this, &MainWindow::onGravarInterpretacao);
    connect(m_btnLimparOcr,    &QPushButton::clicked, this, &MainWindow::onLimparOcr);

    // Navegação
    connect(m_btnPagAnterior,  &QPushButton::clicked, this, &MainWindow::onPaginaAnterior);
    connect(m_btnPagProxima,   &QPushButton::clicked, this, &MainWindow::onPaginaProxima);
    connect(m_spinPagina, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPaginaSpinChanged);

    // Modos de visualização
    connect(m_btnModo1Pag,     &QPushButton::clicked, this, &MainWindow::onModo1PagClicado);
    connect(m_btnModo2Pag,     &QPushButton::clicked, this, &MainWindow::onModo2PagClicado);
    connect(m_btnModoCont,     &QPushButton::clicked, this, &MainWindow::onModoContClicado);

    // Zoom
    connect(m_btnZoomMenos,    &QPushButton::clicked, this, &MainWindow::onZoomMenos);
    connect(m_btnZoomMais,     &QPushButton::clicked, this, &MainWindow::onZoomMais);
    connect(m_btnZoomReset,    &QPushButton::clicked, this, &MainWindow::onZoomReset);
    connect(m_btnAjustarLarg,  &QPushButton::clicked, this, &MainWindow::onAjustarLargura);
    connect(m_btnAjustarPag,   &QPushButton::clicked, this, &MainWindow::onAjustarPagina);

    // Pronúncia
    connect(m_btnOuvir, &QPushButton::clicked, this, [this]() {
        const QString texto = m_textoOriginalEn.isEmpty()
            ? QApplication::clipboard()->text()
            : m_textoOriginalEn;
        if (texto.isEmpty()) {
            appendLog("Nenhum texto selecionado para reproduzir.", "warning");
            return;
        }
        appendLog("Reprodução de áudio: \"" + texto.left(80) + "...\"", "info");
    });

    // NetworkManager
    connect(m_net, &NetworkManager::traducaoDiretaResultado, this, &MainWindow::onTraducaoDiretaResultado);
    connect(m_net, &NetworkManager::limparOcrResultado,      this, &MainWindow::onLimparOcrResultado);
    connect(m_net, &NetworkManager::transcricaoResultado,    this, &MainWindow::onTranscricaoResultado);
    connect(m_net, &NetworkManager::avaliacaoResultado,      this, &MainWindow::onAvaliacaoResultado);
    connect(m_net, &NetworkManager::servidorOnline,          this, &MainWindow::onServidorOnline);
    connect(m_net, &NetworkManager::requisicaoIniciada,      this, &MainWindow::onRequisicaoIniciada);
    connect(m_net, &NetworkManager::requisicaoConcluida,     this, &MainWindow::onRequisicaoConcluida);
    connect(m_net, &NetworkManager::erroRequisicao,          this, &MainWindow::onErroRequisicao);
    connect(m_net, &NetworkManager::gravacaoIniciada,        this, &MainWindow::onGravacaoIniciada);

    connect(m_pdfView->pageNavigator(), &QPdfPageNavigator::currentPageChanged,
            this, [this](int pag) {
        if (m_modoVis == ModoVisualizacao::Continuo) {
            m_paginaAtual = pag;
            atualizarInfoNavegacao();
        }
    });
}

// ═════════════════════════════════════════════════════════════════════════════
// Event Filter (Seleção de Área e Pan)
// ═════════════════════════════════════════════════════════════════════════════

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    auto *targetView = (watched == m_pdfView2->viewport()) ? m_pdfView2 : m_pdfView;
    if (watched != m_pdfView->viewport() && watched != m_pdfView2->viewport())
        return QMainWindow::eventFilter(watched, event);

    switch (event->type()) {

    case QEvent::Wheel: {
        auto *we = static_cast<QWheelEvent*>(event);
        if ((we->modifiers() & Qt::ControlModifier) || (we->modifiers() & Qt::AltModifier)) {
            const qreal delta = we->angleDelta().y() > 0 ? 0.15 : -0.15;
            zoomDelta(delta);
            return true;
        }
        break;
    }

    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent*>(event);
        m_activePdfView = targetView;

        if (me->button() == Qt::LeftButton) {
            if (m_modoCaptura) {
                m_rbOrigin = me->pos();
                m_rubberBand->setParent(m_activePdfView->viewport());
                m_rubberBand->setGeometry(QRect(m_rbOrigin, QSize()));
                m_rubberBand->show();
                return true;
            } else {
                m_panAtivo = true;
                m_panOrigin = me->pos();
                targetView->viewport()->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        } else if (me->button() == Qt::MiddleButton) {
            m_panAtivo = true;
            m_panOrigin = me->pos();
            targetView->viewport()->setCursor(Qt::ClosedHandCursor);
            return true;
        }
        break;
    }

    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent*>(event);
        if (m_modoCaptura && (me->buttons() & Qt::LeftButton)) {
            m_rubberBand->setGeometry(
                QRect(m_rbOrigin, me->pos()).normalized()
            );
            return true;
        } else if (m_panAtivo) {
            const QPoint delta = me->pos() - m_panOrigin;
            m_panOrigin = me->pos();
            targetView->horizontalScrollBar()->setValue(
                targetView->horizontalScrollBar()->value() - delta.x());
            targetView->verticalScrollBar()->setValue(
                targetView->verticalScrollBar()->value() - delta.y());
            return true;
        }
        break;
    }

    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent*>(event);
        if (m_modoCaptura && me->button() == Qt::LeftButton) {
            m_rubberBand->hide();
            const QRect selecao = QRect(m_rbOrigin, me->pos()).normalized();
            if (selecao.width() > 10 && selecao.height() > 10) {
                capturarRegiaoRubberBand();
            }
            return true;
        } else if (m_panAtivo) {
            m_panAtivo = false;
            targetView->viewport()->setCursor(m_modoCaptura ? Qt::CrossCursor : Qt::OpenHandCursor);
            return true;
        }
        break;
    }

    default:
        break;
    }

    return QMainWindow::eventFilter(watched, event);
}

// ═════════════════════════════════════════════════════════════════════════════
// Captura de Imagem da Região
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::capturarRegiaoRubberBand()
{
    if (!m_activePdfView) m_activePdfView = m_pdfView;

    const QRect rect = m_rubberBand->geometry();
    if (rect.isEmpty()) return;

    const QPixmap captura = m_activePdfView->viewport()->grab(rect);
    if (captura.isNull()) return;

    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    captura.save(&buffer, "PNG");
    const QByteArray base64 = ba.toBase64();

    m_textoOcrAtual = "BASE64:" + QString::fromLatin1(base64);

    appendLog(QString("Região capturada (%1x%2 px)").arg(rect.width()).arg(rect.height()));

    m_btnTraduzir->setEnabled(true);
    m_btnLimparOcr->setEnabled(true);

    if (m_chkTraducaoAuto && m_chkTraducaoAuto->isChecked()) {
        onTraduzirDireto();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Ações de Tradução
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::onTraduzirDireto()
{
    if (m_textoOcrAtual.startsWith("BASE64:")) {
        const QByteArray base64 = m_textoOcrAtual.mid(7).toLatin1();
        appendLog("Processando tradução da seleção...");
        m_net->traduzirDireto(QString(), base64);
    } else if (!m_textoOcrAtual.isEmpty()) {
        appendLog("Processando tradução...");
        m_net->traduzirDireto(m_textoOcrAtual);
    } else {
        const QString clipboardTxt = QApplication::clipboard()->text().trimmed();
        if (!clipboardTxt.isEmpty()) {
            appendLog("Traduzindo texto da área de transferência...");
            m_net->traduzirDireto(clipboardTxt);
        } else {
            appendLog("Nenhuma seleção disponível.", "warning");
        }
    }
}

void MainWindow::onTraducaoDiretaResultado(const QString &textoIngles, const QString &traducaoPortugues)
{
    m_textoOriginalEn = textoIngles;
    m_btnOuvir->setEnabled(!textoIngles.isEmpty());
    m_btnGravar->setEnabled(!textoIngles.isEmpty());

    if (m_txtTraducao) {
        m_txtTraducao->setHtml(
            "<div style='margin-bottom: 8px;'>"
            "<span style='color: #6b7280; font-size: 10px; font-weight: 600; text-transform: uppercase;'>Texto Original (EN)</span><br>"
            "<span style='color: #9ca3af; font-size: 12px; line-height: 1.4;'>" + textoIngles.toHtmlEscaped() + "</span>"
            "</div><hr style='border: 0; border-top: 1px solid #27282d; margin: 8px 0;'>"
            "<div>"
            "<span style='color: #60a5fa; font-size: 10px; font-weight: 600; text-transform: uppercase;'>Tradução (PT-BR)</span><br>"
            "<span style='color: #f3f4f6; font-size: 13px; font-weight: 500; line-height: 1.5;'>" + traducaoPortugues.toHtmlEscaped() + "</span>"
            "</div>"
        );
    }

    appendLog("Tradução concluída com sucesso.", "success");
}

void MainWindow::onLimparOcr()
{
    if (m_textoOcrAtual.startsWith("BASE64:")) {
        const QByteArray base64 = m_textoOcrAtual.mid(7).toLatin1();
        m_net->processarImagemOcr(base64);
    } else if (!m_textoOcrAtual.isEmpty()) {
        m_net->limparOcr(m_textoOcrAtual);
    }
}

void MainWindow::onLimparOcrResultado(const QString &texto)
{
    m_textoOriginalEn = texto;
    m_btnOuvir->setEnabled(true);
    m_btnGravar->setEnabled(true);
    appendLog("Texto extraído da imagem com sucesso.");
}

// ═════════════════════════════════════════════════════════════════════════════
// Módulo de Treino (Oculto / Exibido via Menu)
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::onToggleModuloTreino(bool visivel)
{
    if (m_containerTreino) {
        m_containerTreino->setVisible(visivel);
    }
    if (visivel) {
        appendLog("Módulo de treino e pronúncia ativado.", "info");
    } else {
        appendLog("Módulo de treino ocultado.", "info");
    }
}

void MainWindow::onGravarInterpretacao()
{
    if (!m_gravando) {
        if (m_textoOriginalEn.isEmpty()) {
            m_textoOriginalEn = QApplication::clipboard()->text().trimmed();
        }

        if (m_textoOriginalEn.isEmpty()) {
            appendLog("Selecione um texto antes de iniciar a gravação.", "warning");
            return;
        }

        appendLog("Gravando áudio do microfone...");
        m_net->iniciarGravacao();
    } else {
        m_gravando = false;
        m_btnGravar->setText("Gravar Voz");
        m_btnGravar->setStyleSheet("");
        appendLog("Finalizando gravação e transcrevendo...");
        m_net->pararGravacao();
    }
}

void MainWindow::onGravacaoIniciada()
{
    m_gravando = true;
    m_btnGravar->setText("Parar Gravação");
    m_btnGravar->setStyleSheet("background-color: #dc2626; color: #ffffff; font-weight: 600;");
}

void MainWindow::onTranscricaoResultado(const QString &texto)
{
    m_textoTranscrito = texto;
    appendLog("Transcrição de fala: \"" + texto + "\"");

    if (!m_textoOriginalEn.isEmpty()) {
        appendLog("Avaliando similaridade conceitual...");
        m_net->avaliarTraducao(m_textoOriginalEn, texto);
    }
}

void MainWindow::onAvaliacaoResultado(int nota)
{
    appendLog(QString("Avaliação de precisão: %1 / 100").arg(nota),
              nota >= 70 ? "success" : "warning");
}

// ═════════════════════════════════════════════════════════════════════════════
// Navegação e Modos de Visualização
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::irParaPagina(int pagina)
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) return;

    pagina = qBound(0, pagina, m_pdfDoc->pageCount() - 1);

    if (m_modoVis == ModoVisualizacao::DuasPaginas) {
        if (pagina % 2 != 0 && pagina > 0)
            pagina -= 1;

        m_paginaAtual = pagina;
        m_pdfView->pageNavigator()->jump(pagina, {});

        if (pagina + 1 < m_pdfDoc->pageCount()) {
            m_pdfView2->pageNavigator()->jump(pagina + 1, {});
            m_pdfView2->setVisible(true);
        } else {
            m_pdfView2->setVisible(false);
        }
    } else {
        m_paginaAtual = pagina;
        m_pdfView->pageNavigator()->jump(pagina, {});
    }

    atualizarInfoNavegacao();
}

void MainWindow::setModoVisualizacao(ModoVisualizacao modo)
{
    m_modoVis = modo;

    m_btnModo1Pag->setChecked(modo == ModoVisualizacao::UmaPagina);
    m_btnModo2Pag->setChecked(modo == ModoVisualizacao::DuasPaginas);
    m_btnModoCont->setChecked(modo == ModoVisualizacao::Continuo);

    if (modo == ModoVisualizacao::DuasPaginas) {
        m_pdfView->setPageMode(QPdfView::PageMode::SinglePage);
        m_pdfView2->setPageMode(QPdfView::PageMode::SinglePage);
        m_pdfView2->setVisible(true);
    } else if (modo == ModoVisualizacao::Continuo) {
        m_pdfView2->setVisible(false);
        m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
    } else {
        m_pdfView2->setVisible(false);
        m_pdfView->setPageMode(QPdfView::PageMode::SinglePage);
    }

    irParaPagina(m_paginaAtual);
    atualizarLabelZoom();
}

void MainWindow::onModo1PagClicado()
{
    setModoVisualizacao(ModoVisualizacao::UmaPagina);
}

void MainWindow::onModo2PagClicado()
{
    setModoVisualizacao(ModoVisualizacao::DuasPaginas);
}

void MainWindow::onModoContClicado()
{
    setModoVisualizacao(ModoVisualizacao::Continuo);
}

void MainWindow::onPaginaAnterior()
{
    const int passo = (m_modoVis == ModoVisualizacao::DuasPaginas) ? 2 : 1;
    irParaPagina(m_paginaAtual - passo);
}

void MainWindow::onPaginaProxima()
{
    const int passo = (m_modoVis == ModoVisualizacao::DuasPaginas) ? 2 : 1;
    irParaPagina(m_paginaAtual + passo);
}

void MainWindow::onPaginaSpinChanged(int pag)
{
    if (m_pdfDoc && m_pdfDoc->pageCount() > 0 && (pag - 1) != m_paginaAtual) {
        irParaPagina(pag - 1);
    }
}

void MainWindow::atualizarInfoNavegacao()
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) {
        m_spinPagina->setEnabled(false);
        m_btnPagAnterior->setEnabled(false);
        m_btnPagProxima->setEnabled(false);
        m_lblTotalPag->setText("/ 0");
        return;
    }

    const int total = m_pdfDoc->pageCount();
    m_spinPagina->blockSignals(true);
    m_spinPagina->setMaximum(total);
    m_spinPagina->setValue(m_paginaAtual + 1);
    m_spinPagina->setEnabled(true);
    m_spinPagina->blockSignals(false);

    if (m_modoVis == ModoVisualizacao::DuasPaginas) {
        const int segunda = qMin(m_paginaAtual + 2, total);
        m_lblTotalPag->setText(QString("e %1 de %2").arg(segunda).arg(total));
    } else {
        m_lblTotalPag->setText(QString("/ %1").arg(total));
    }

    m_btnPagAnterior->setEnabled(m_paginaAtual > 0);
    m_btnPagProxima->setEnabled(m_paginaAtual < total - 1);
}

// ═════════════════════════════════════════════════════════════════════════════
// Controles de Zoom
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::aplicarZoom(qreal fator)
{
    fator = qBound(0.2, fator, 4.0);

    m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
    m_pdfView->setZoomFactor(fator);

    if (m_pdfView2->isVisible()) {
        m_pdfView2->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView2->setZoomFactor(fator);
    }

    atualizarLabelZoom();
}

void MainWindow::zoomDelta(qreal delta)
{
    aplicarZoom(m_pdfView->zoomFactor() + delta);
}

void MainWindow::onZoomMenos()
{
    zoomDelta(-0.15);
}

void MainWindow::onZoomMais()
{
    zoomDelta(0.15);
}

void MainWindow::onZoomReset()
{
    aplicarZoom(1.0);
}

void MainWindow::onAjustarLargura()
{
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    if (m_pdfView2->isVisible())
        m_pdfView2->setZoomMode(QPdfView::ZoomMode::FitToWidth);

    atualizarLabelZoom();
}

void MainWindow::onAjustarPagina()
{
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    if (m_pdfView2->isVisible())
        m_pdfView2->setZoomMode(QPdfView::ZoomMode::FitInView);

    atualizarLabelZoom();
}

void MainWindow::atualizarLabelZoom()
{
    const int perc = qRound(m_pdfView->zoomFactor() * 100.0);
    m_btnZoomReset->setText(QString::number(perc) + "%");
}

// ═════════════════════════════════════════════════════════════════════════════
// Gerenciamento de Arquivo
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::onAbrirPdf()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Abrir Documento PDF",
        {},
        "Documentos PDF (*.pdf)"
    );
    if (path.isEmpty()) return;

    const auto error = m_pdfDoc->load(path);
    if (error != QPdfDocument::Error::None) {
        appendLog("Falha ao carregar o arquivo PDF.", "error");
        return;
    }

    const QString nome = QFileInfo(path).fileName();
    setWindowTitle(nome + " — Leitor Técnico");
    appendLog(QString("Documento carregado: %1 (%2 páginas)").arg(nome).arg(m_pdfDoc->pageCount()));

    irParaPagina(0);
    atualizarLabelZoom();

    m_btnModoCaptura->setEnabled(true);
    m_btnModoCaptura->setChecked(true); // Seleção pronta para leitura
    m_lblStatus->setText(nome);
}

void MainWindow::onFecharPdf()
{
    if (m_pdfDoc) {
        m_pdfDoc->close();
        setWindowTitle("Leitor Técnico de Documentos");
        atualizarInfoNavegacao();
        m_btnModoCaptura->setEnabled(false);
        m_btnModoCaptura->setChecked(false);
        m_btnTraduzir->setEnabled(false);
        m_lblStatus->setText("Pronto");
        appendLog("Documento fechado.");
    }
}

void MainWindow::onToggleModoCaptura(bool ativo)
{
    m_modoCaptura = ativo;

    if (ativo) {
        m_btnModoCaptura->setText("Seleção Ativa");
        m_pdfView->viewport()->setCursor(Qt::CrossCursor);
        m_pdfView2->viewport()->setCursor(Qt::CrossCursor);
    } else {
        m_btnModoCaptura->setText("Modo Seleção");
        m_rubberBand->hide();
        m_pdfView->viewport()->setCursor(Qt::ArrowCursor);
        m_pdfView2->viewport()->setCursor(Qt::ArrowCursor);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Notificações de Rede e Status
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::onServidorOnline(bool online)
{
    if (online) {
        m_lblStatus->setText("Serviço de tradução ativo");
    } else {
        m_lblStatus->setText("Serviço offline (execute ./iniciar_backend.sh)");
    }
}

void MainWindow::onRequisicaoIniciada(const QString &endpoint)
{
    if (endpoint == "/traduzir") {
        setButtonBusy(m_btnTraduzir, true);
        m_lblStatus->setText("Traduzindo...");
    } else if (endpoint == "/limpar_ocr") {
        setButtonBusy(m_btnLimparOcr, true);
        m_lblStatus->setText("Processando texto...");
    } else if (endpoint == "/parar_gravacao") {
        setButtonBusy(m_btnGravar, true);
        m_lblStatus->setText("Transcrevendo áudio...");
    }
}

void MainWindow::onRequisicaoConcluida(const QString &endpoint)
{
    if (endpoint == "/traduzir") {
        setButtonBusy(m_btnTraduzir, false);
    } else if (endpoint == "/limpar_ocr") {
        setButtonBusy(m_btnLimparOcr, false);
    } else if (endpoint == "/parar_gravacao") {
        setButtonBusy(m_btnGravar, false);
    }
    m_lblStatus->setText("Pronto");
}

void MainWindow::onErroRequisicao(const QString &endpoint, const QString &mensagem)
{
    m_lblStatus->setText("Falha na comunicação");
    appendLog("Erro em " + endpoint + ": " + mensagem, "error");
}

// ═════════════════════════════════════════════════════════════════════════════
// Utilitários de Registro
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::appendLog(const QString &mensagem, const QString &tipo)
{
    const QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString cor = "#9ca3af";
    if (tipo == "error")   cor = "#f87171";
    if (tipo == "warning") cor = "#fbbf24";
    if (tipo == "success") cor = "#34d399";

    m_logBox->append(QString("<span style='color: #4b5563;'>[%1]</span> <span style='color: %2;'>%3</span>")
                     .arg(timestamp, cor, mensagem.toHtmlEscaped()));
    m_logBox->verticalScrollBar()->setValue(m_logBox->verticalScrollBar()->maximum());
}

void MainWindow::setButtonBusy(QPushButton *btn, bool busy)
{
    if (!btn) return;
    btn->setEnabled(!busy);
}
