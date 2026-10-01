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
#include <QSettings>
#include <QCloseEvent>

// ═════════════════════════════════════════════════════════════════════════════
// Construtor e Inicialização
// ═════════════════════════════════════════════════════════════════════════════

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Leitor Técnico de Documentos");
    setMinimumSize(960, 640);

    m_net = new NetworkManager("http://localhost:8000", this);

    setupMenuBar();
    setupUi();
    setupStyleSheet();
    connectSignals();

    carregarConfiguracoes();

    m_net->verificarConexao();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    salvarConfiguracoes();
    event->accept();
}

// ═════════════════════════════════════════════════════════════════════════════
// Barra de Menus Superior (Controle Central sem Poluição na Tela)
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

    menuExibir->addSeparator();

    m_actBarraLeitor = menuExibir->addAction("Barra de Navegação do Leitor");
    m_actBarraLeitor->setCheckable(true);
    m_actBarraLeitor->setChecked(true);
    connect(m_actBarraLeitor, &QAction::toggled, this, [this](bool visivel) {
        if (m_barraVis) m_barraVis->setVisible(visivel);
    });

    // ── Menu Painel ───────────────────────────────────────────────────────
    auto *menuPainel = menuBar->addMenu("&Painel");

    m_actTogglePainel = menuPainel->addAction("Alternar Painel Lateral");
    m_actTogglePainel->setCheckable(true);
    m_actTogglePainel->setChecked(false); // Inicia fechado!
    m_actTogglePainel->setShortcut(QKeySequence("F4"));
    connect(m_actTogglePainel, &QAction::toggled, this, &MainWindow::onTogglePainelLateral);

    menuPainel->addSeparator();

    auto *actAbaTrad = menuPainel->addAction("Ir para Tradução");
    actAbaTrad->setShortcut(QKeySequence("Ctrl+T"));
    connect(actAbaTrad, &QAction::triggered, this, [this]() { onMudarAbaPainel(0); });

    auto *actAbaTutor = menuPainel->addAction("Ir para Tutor & Pronúncia");
    actAbaTutor->setShortcut(QKeySequence("Ctrl+U"));
    connect(actAbaTutor, &QAction::triggered, this, [this]() { onMudarAbaPainel(1); });

    auto *actAbaConfig = menuPainel->addAction("Ir para Desempenho & Layout");
    actAbaConfig->setShortcut(QKeySequence("Ctrl+D"));
    connect(actAbaConfig, &QAction::triggered, this, [this]() { onMudarAbaPainel(2); });

    // ── Menu Layout ───────────────────────────────────────────────────────
    auto *menuLayout = menuBar->addMenu("&Layout");

    auto *actSalvarLayout = menuLayout->addAction("Salvar Layout Atual");
    actSalvarLayout->setShortcut(QKeySequence("Ctrl+S"));
    connect(actSalvarLayout, &QAction::triggered, this, &MainWindow::onSalvarLayout);

    auto *actRestaurarLayout = menuLayout->addAction("Restaurar Layout Padrão");
    connect(actRestaurarLayout, &QAction::triggered, this, &MainWindow::onRestaurarLayoutPadrao);

    menuLayout->addSeparator();

    auto *actProp100 = menuLayout->addAction("Foco no Leitor (100%)");
    connect(actProp100, &QAction::triggered, this, [this]() { onAplicarProporcao(100); });

    auto *actProp70 = menuLayout->addAction("Leitura & Painel (70% / 30%)");
    connect(actProp70, &QAction::triggered, this, [this]() { onAplicarProporcao(70); });

    auto *actProp50 = menuLayout->addAction("Divisão Equilibrada (50% / 50%)");
    connect(actProp50, &QAction::triggered, this, [this]() { onAplicarProporcao(50); });
}

// ═════════════════════════════════════════════════════════════════════════════
// Construção da Interface
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::setupUi()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(3);
    setCentralWidget(m_splitter);

    // ── Painel Esquerdo: Visualizador de Documentos PDF ───────────────────
    m_painelEsq = new QWidget(this);
    m_painelEsq->setObjectName("painelEsquerdo");
    auto *layoutEsq = new QVBoxLayout(m_painelEsq);
    layoutEsq->setContentsMargins(0, 0, 0, 0);
    layoutEsq->setSpacing(0);

    m_barraVis = criarBarraVisualizacao();
    layoutEsq->addWidget(m_barraVis);

    auto *viewContainer = new QWidget(m_painelEsq);
    viewContainer->setObjectName("viewContainer");
    auto *viewLayout = new QHBoxLayout(viewContainer);
    viewLayout->setContentsMargins(0, 0, 0, 0);
    viewLayout->setSpacing(2);

    m_pdfDoc = new QPdfDocument(this);

    m_pdfView = new QPdfView(viewContainer);
    m_pdfView->setDocument(m_pdfDoc);
    m_pdfView->setPageMode(QPdfView::PageMode::SinglePage);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    m_pdfView->viewport()->installEventFilter(this);

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

    m_splitter->addWidget(m_painelEsq);

    // ── Painel Direito: Abas de Tradução, Tutor e Configurações ───────────
    m_painelDir = new QFrame(this);
    m_painelDir->setObjectName("painelDireito");
    auto *layoutDir = new QVBoxLayout(m_painelDir);
    layoutDir->setContentsMargins(12, 12, 12, 12);
    layoutDir->setSpacing(8);

    m_tabWidget = new QTabWidget(m_painelDir);
    m_tabWidget->setObjectName("tabWidgetPrincipal");

    m_tabWidget->addTab(criarAbaTraducao(), "Tradução");
    m_tabWidget->addTab(criarAbaTutor(), "Tutor de Fala");
    m_tabWidget->addTab(criarAbaConfiguracoes(), "Desempenho & Layout");

    layoutDir->addWidget(m_tabWidget);

    m_splitter->addWidget(m_painelDir);

    // Inicia fechado por padrão para tela 100% limpa
    m_painelDir->setVisible(false);

    m_splitter->setStretchFactor(0, 75);
    m_splitter->setStretchFactor(1, 25);
}

// ─────────────────────────────────────────────────────────────────────────────
// Aba 1: Tradução Direta
// ─────────────────────────────────────────────────────────────────────────────

QWidget* MainWindow::criarAbaTraducao()
{
    auto *aba = new QWidget();
    auto *layout = new QVBoxLayout(aba);
    layout->setContentsMargins(4, 8, 4, 4);
    layout->setSpacing(8);

    auto *layoutTop = new QHBoxLayout();
    layoutTop->setSpacing(6);

    m_btnModoCaptura = new QPushButton("Modo Seleção", aba);
    m_btnModoCaptura->setObjectName("btnCaptura");
    m_btnModoCaptura->setCheckable(true);
    layoutTop->addWidget(m_btnModoCaptura);

    m_btnTraduzir = new QPushButton("Traduzir Seleção", aba);
    m_btnTraduzir->setObjectName("btnTraduzir");
    m_btnTraduzir->setEnabled(false);
    layoutTop->addWidget(m_btnTraduzir);

    layout->addLayout(layoutTop);

    m_chkTraducaoAuto = new QCheckBox("Traduzir automaticamente ao selecionar na tela", aba);
    m_chkTraducaoAuto->setObjectName("chkAuto");
    m_chkTraducaoAuto->setChecked(true);
    layout->addWidget(m_chkTraducaoAuto);

    m_txtTraducao = new QTextEdit(aba);
    m_txtTraducao->setObjectName("txtTraducao");
    m_txtTraducao->setReadOnly(true);
    m_txtTraducao->setPlaceholderText("Selecione um parágrafo no documento para visualizar o texto original e a tradução técnica.");
    layout->addWidget(m_txtTraducao, 1);

    auto *layoutBottom = new QHBoxLayout();
    m_btnCopiarTraducao = new QPushButton("Copiar Tradução", aba);
    m_btnCopiarTraducao->setObjectName("btnSecundario");
    layoutBottom->addWidget(m_btnCopiarTraducao);
    layoutBottom->addStretch();

    layout->addLayout(layoutBottom);
    return aba;
}

// ─────────────────────────────────────────────────────────────────────────────
// Aba 2: Tutor & Pronúncia
// ─────────────────────────────────────────────────────────────────────────────

QWidget* MainWindow::criarAbaTutor()
{
    auto *aba = new QWidget();
    auto *layout = new QVBoxLayout(aba);
    layout->setContentsMargins(4, 8, 4, 4);
    layout->setSpacing(8);

    auto *lblVoz = new QLabel("Voz e Sotaque do Tutor:", aba);
    lblVoz->setObjectName("lblSecao");
    layout->addWidget(lblVoz);

    m_comboVoz = new QComboBox(aba);
    m_comboVoz->setObjectName("comboVoz");
    m_comboVoz->addItem("Inglês (EUA) - Jenny (Feminina)", "en-US-JennyNeural");
    m_comboVoz->addItem("Inglês (EUA) - Guy (Masculina)", "en-US-GuyNeural");
    m_comboVoz->addItem("Inglês (UK) - Sonia (Feminina)", "en-GB-SoniaNeural");
    m_comboVoz->addItem("Inglês (UK) - Ryan (Masculina)", "en-GB-RyanNeural");
    m_comboVoz->addItem("Inglês (AU) - Natasha (Feminina)", "en-AU-NatashaNeural");
    layout->addWidget(m_comboVoz);

    auto *layoutBotoes = new QHBoxLayout();
    layoutBotoes->setSpacing(6);

    m_btnOuvir = new QPushButton("Ouvir Pronúncia", aba);
    m_btnOuvir->setObjectName("btnSecundario");
    m_btnOuvir->setEnabled(false);
    layoutBotoes->addWidget(m_btnOuvir);

    m_btnGravar = new QPushButton("Gravar Minha Voz", aba);
    m_btnGravar->setObjectName("btnGravar");
    m_btnGravar->setEnabled(false);
    layoutBotoes->addWidget(m_btnGravar);

    layout->addLayout(layoutBotoes);

    auto *lblAcuraciaTitulo = new QLabel("Acurácia da Pronúncia:", aba);
    lblAcuraciaTitulo->setObjectName("lblSecao");
    layout->addWidget(lblAcuraciaTitulo);

    m_barAcuracia = new QProgressBar(aba);
    m_barAcuracia->setObjectName("barAcuracia");
    m_barAcuracia->setRange(0, 100);
    m_barAcuracia->setValue(0);
    m_barAcuracia->setTextVisible(true);
    m_barAcuracia->setFormat("%p%");
    layout->addWidget(m_barAcuracia);

    m_txtFeedbackTutor = new QTextEdit(aba);
    m_txtFeedbackTutor->setObjectName("txtFeedback");
    m_txtFeedbackTutor->setReadOnly(true);
    m_txtFeedbackTutor->setPlaceholderText("Grave sua leitura do texto selecionado para receber análise de pronúncia e dicas fonéticas do tutor.");
    layout->addWidget(m_txtFeedbackTutor, 1);

    return aba;
}

// ─────────────────────────────────────────────────────────────────────────────
// Aba 3: Desempenho & Layout
// ─────────────────────────────────────────────────────────────────────────────

QWidget* MainWindow::criarAbaConfiguracoes()
{
    auto *aba = new QWidget();
    auto *layout = new QVBoxLayout(aba);
    layout->setContentsMargins(4, 8, 4, 4);
    layout->setSpacing(8);

    auto *lblPerfil = new QLabel("Perfil de Hardware:", aba);
    lblPerfil->setObjectName("lblSecao");
    layout->addWidget(lblPerfil);

    m_comboPerfil = new QComboBox(aba);
    m_comboPerfil->setObjectName("comboPerfil");
    m_comboPerfil->addItem("Econômico / Rápido (Recomendado para PCs modestos)");
    m_comboPerfil->addItem("Alta Precisão (Recomendado com GPU dedicada)");
    layout->addWidget(m_comboPerfil);

    auto *lblLayout = new QLabel("Gerenciamento de Layout:", aba);
    lblLayout->setObjectName("lblSecao");
    layout->addWidget(lblLayout);

    auto *layoutBtnsLayout = new QHBoxLayout();
    m_btnSalvarLayout = new QPushButton("Salvar Layout", aba);
    m_btnSalvarLayout->setObjectName("btnSecundario");
    layoutBtnsLayout->addWidget(m_btnSalvarLayout);

    m_btnRestaurarLayout = new QPushButton("Restaurar Padrão", aba);
    m_btnRestaurarLayout->setObjectName("btnSecundario");
    layoutBtnsLayout->addWidget(m_btnRestaurarLayout);
    layout->addLayout(layoutBtnsLayout);

    auto *layoutProps = new QHBoxLayout();
    m_btnProporcao100 = new QPushButton("100% Leitor", aba);
    m_btnProporcao100->setObjectName("btnSecundario");
    layoutProps->addWidget(m_btnProporcao100);

    m_btnProporcao70 = new QPushButton("70% / 30%", aba);
    m_btnProporcao70->setObjectName("btnSecundario");
    layoutProps->addWidget(m_btnProporcao70);

    m_btnProporcao50 = new QPushButton("50% / 50%", aba);
    m_btnProporcao50->setObjectName("btnSecundario");
    layoutProps->addWidget(m_btnProporcao50);
    layout->addLayout(layoutProps);

    auto *lblLog = new QLabel("Registro de Atividades:", aba);
    lblLog->setObjectName("lblSecao");
    layout->addWidget(lblLog);

    m_logBox = new QTextEdit(aba);
    m_logBox->setObjectName("logBox");
    m_logBox->setReadOnly(true);
    m_logBox->setMaximumHeight(110);
    layout->addWidget(m_logBox);

    m_lblStatus = new QLabel("Pronto", aba);
    m_lblStatus->setObjectName("lblStatus");
    layout->addWidget(m_lblStatus);

    return aba;
}

// ─────────────────────────────────────────────────────────────────────────────
// Barra Superior de Visualização do Documento
// ─────────────────────────────────────────────────────────────────────────────

QWidget* MainWindow::criarBarraVisualizacao()
{
    auto *bar = new QFrame(this);
    bar->setObjectName("barraVisualizacao");
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(8);

    // Navegação
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

    // Modos de Exibição
    m_btnModo1Pag = new QPushButton("Individual", bar);
    m_btnModo1Pag->setObjectName("btnModoVis");
    m_btnModo1Pag->setCheckable(true);
    m_btnModo1Pag->setChecked(true);

    m_btnModo2Pag = new QPushButton("Lado a Lado", bar);
    m_btnModo2Pag->setObjectName("btnModoVis");
    m_btnModo2Pag->setCheckable(true);

    m_btnModoCont = new QPushButton("Contínuo", bar);
    m_btnModoCont->setObjectName("btnModoVis");
    m_btnModoCont->setCheckable(true);

    layout->addWidget(m_btnModo1Pag);
    layout->addWidget(m_btnModo2Pag);
    layout->addWidget(m_btnModoCont);

    auto *vsep2 = new QFrame(bar);
    vsep2->setFrameShape(QFrame::VLine);
    vsep2->setObjectName("vseparador");
    layout->addWidget(vsep2);

    // Zoom
    m_btnZoomMenos = new QPushButton("-", bar);
    m_btnZoomMenos->setObjectName("btnNav");

    m_btnZoomReset = new QPushButton("100%", bar);
    m_btnZoomReset->setObjectName("btnZoomReset");

    m_btnZoomMais = new QPushButton("+", bar);
    m_btnZoomMais->setObjectName("btnNav");

    m_btnAjustarLarg = new QPushButton("Largura", bar);
    m_btnAjustarLarg->setObjectName("btnNav");

    m_btnAjustarPag = new QPushButton("Página", bar);
    m_btnAjustarPag->setObjectName("btnNav");

    layout->addWidget(m_btnZoomMenos);
    layout->addWidget(m_btnZoomReset);
    layout->addWidget(m_btnZoomMais);
    layout->addWidget(m_btnAjustarLarg);
    layout->addWidget(m_btnAjustarPag);

    layout->addStretch();
    return bar;
}

// ═════════════════════════════════════════════════════════════════════════════
// Folha de Estilos (Design Sóbrio, Minimalista, Dark Slate)
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::setupStyleSheet()
{
    setStyleSheet(R"(
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

        /* ── Painel Direito e Abas ───────────────────────────────── */
        #painelDireito {
            background-color: #18191d;
            border-left: 1px solid #27282d;
        }

        QTabWidget::pane {
            border: 1px solid #27282d;
            border-radius: 6px;
            background-color: #18191d;
            top: -1px;
        }
        QTabBar::tab {
            background-color: #16171a;
            color: #8b92a0;
            padding: 7px 14px;
            margin-right: 2px;
            border-top-left-radius: 5px;
            border-top-right-radius: 5px;
            font-size: 11px;
            font-weight: 600;
        }
        QTabBar::tab:selected {
            background-color: #202227;
            color: #ffffff;
            border: 1px solid #27282d;
            border-bottom: 1px solid #202227;
        }
        QTabBar::tab:hover:!selected {
            color: #d1d5db;
        }

        #lblSecao {
            color: #9ca3af;
            font-size: 11px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        /* Botões */
        QPushButton {
            background-color: #22242a;
            color: #e5e7eb;
            border: 1px solid #2f333c;
            border-radius: 5px;
            padding: 7px 12px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #2a2c35;
            border-color: #404552;
            color: #ffffff;
        }
        QPushButton:pressed { background-color: #1c1d22; }
        QPushButton:disabled {
            background-color: #18191d;
            color: #444955;
            border-color: #22242a;
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
        #btnTraduzir:hover { background-color: #2563eb; border-color: #3b82f6; }
        #btnTraduzir:disabled { background-color: #1c202a; color: #434c5f; border-color: #222735; }

        #btnGravar {
            background-color: #831843;
            color: #ffffff;
            border: 1px solid #9d174d;
        }
        #btnGravar:hover { background-color: #9d174d; }

        #chkAuto {
            color: #9ca3af;
            font-size: 11px;
        }

        QComboBox {
            background-color: #202227;
            color: #e5e7eb;
            border: 1px solid #2d3038;
            border-radius: 5px;
            padding: 5px 8px;
            font-size: 11px;
        }
        QComboBox::drop-down { border: none; }
        QComboBox QAbstractItemView {
            background-color: #1e1f24;
            color: #e5e7eb;
            selection-background-color: #2b2d35;
            border: 1px solid #2f3138;
        }

        QProgressBar {
            background-color: #141518;
            border: 1px solid #26282f;
            border-radius: 4px;
            text-align: center;
            color: #ffffff;
            font-size: 10px;
            font-weight: 600;
            height: 14px;
        }
        QProgressBar::chunk {
            background-color: #2563eb;
            border-radius: 3px;
        }

        #txtTraducao, #txtFeedback {
            background-color: #141518;
            color: #e5e7eb;
            border: 1px solid #26282f;
            border-radius: 6px;
            padding: 8px;
            font-size: 12px;
            line-height: 1.4;
        }

        #logBox {
            background-color: #141518;
            color: #8b92a0;
            border: 1px solid #26282f;
            border-radius: 6px;
            padding: 4px;
            font-family: monospace;
            font-size: 10px;
        }

        #lblStatus {
            font-size: 11px;
            color: #6b7280;
            padding: 2px;
        }

        QScrollBar:vertical, QScrollBar:horizontal {
            background: #141518;
            width: 6px;
            height: 6px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #2c2f38;
            border-radius: 3px;
            min-height: 20px;
        }
        QScrollBar::handle:hover { background: #3e424e; }
    )");
}

// ═════════════════════════════════════════════════════════════════════════════
// Conexão de Sinais
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::connectSignals()
{
    // Ações de Tradução
    connect(m_btnModoCaptura,    &QPushButton::toggled, this, &MainWindow::onToggleModoCaptura);
    connect(m_btnTraduzir,       &QPushButton::clicked, this, &MainWindow::onTraduzirDireto);
    connect(m_btnCopiarTraducao, &QPushButton::clicked, this, &MainWindow::onCopiarTraducao);

    // Tutor e Áudio
    connect(m_btnOuvir,  &QPushButton::clicked, this, &MainWindow::onOuvirPronuncia);
    connect(m_btnGravar, &QPushButton::clicked, this, &MainWindow::onGravarVozTutor);

    // Layout
    connect(m_btnSalvarLayout,    &QPushButton::clicked, this, &MainWindow::onSalvarLayout);
    connect(m_btnRestaurarLayout, &QPushButton::clicked, this, &MainWindow::onRestaurarLayoutPadrao);
    connect(m_btnProporcao100,    &QPushButton::clicked, this, [this]() { onAplicarProporcao(100); });
    connect(m_btnProporcao70,     &QPushButton::clicked, this, [this]() { onAplicarProporcao(70); });
    connect(m_btnProporcao50,     &QPushButton::clicked, this, [this]() { onAplicarProporcao(50); });

    // Configurações
    connect(m_comboPerfil, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPerfilAlterado);

    // Navegação
    connect(m_btnPagAnterior,  &QPushButton::clicked, this, &MainWindow::onPaginaAnterior);
    connect(m_btnPagProxima,   &QPushButton::clicked, this, &MainWindow::onPaginaProxima);
    connect(m_spinPagina, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPaginaSpinChanged);

    // Modos
    connect(m_btnModo1Pag,     &QPushButton::clicked, this, &MainWindow::onModo1PagClicado);
    connect(m_btnModo2Pag,     &QPushButton::clicked, this, &MainWindow::onModo2PagClicado);
    connect(m_btnModoCont,     &QPushButton::clicked, this, &MainWindow::onModoContClicado);

    // Zoom
    connect(m_btnZoomMenos,    &QPushButton::clicked, this, &MainWindow::onZoomMenos);
    connect(m_btnZoomMais,     &QPushButton::clicked, this, &MainWindow::onZoomMais);
    connect(m_btnZoomReset,    &QPushButton::clicked, this, &MainWindow::onZoomReset);
    connect(m_btnAjustarLarg,  &QPushButton::clicked, this, &MainWindow::onAjustarLargura);
    connect(m_btnAjustarPag,   &QPushButton::clicked, this, &MainWindow::onAjustarPagina);

    // NetworkManager
    connect(m_net, &NetworkManager::traducaoDiretaResultado,      this, &MainWindow::onTraducaoDiretaResultado);
    connect(m_net, &NetworkManager::limparOcrResultado,           this, &MainWindow::onLimparOcrResultado);
    connect(m_net, &NetworkManager::transcricaoResultado,         this, &MainWindow::onTranscricaoResultado);
    connect(m_net, &NetworkManager::servidorOnline,               this, &MainWindow::onServidorOnline);
    connect(m_net, &NetworkManager::requisicaoIniciada,           this, &MainWindow::onRequisicaoIniciada);
    connect(m_net, &NetworkManager::requisicaoConcluida,          this, &MainWindow::onRequisicaoConcluida);
    connect(m_net, &NetworkManager::erroRequisicao,               this, &MainWindow::onErroRequisicao);
    connect(m_net, &NetworkManager::gravacaoIniciada,             this, &MainWindow::onGravacaoIniciada);
    connect(m_net, &NetworkManager::falaIniciada,                 this, &MainWindow::onFalaIniciada);
    connect(m_net, &NetworkManager::avaliacaoPronunciaResultado,  this, &MainWindow::onAvaliacaoPronunciaResultado);

    connect(m_pdfView->pageNavigator(), &QPdfPageNavigator::currentPageChanged,
            this, [this](int pag) {
        if (m_modoVis == ModoVisualizacao::Continuo) {
            m_paginaAtual = pag;
            atualizarInfoNavegacao();
        }
    });
}

// ═════════════════════════════════════════════════════════════════════════════
// Persistência de Layout (QSettings)
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::carregarConfiguracoes()
{
    QSettings settings("EnsinadorDeIngles", "LeitorTecnico");
    restoreGeometry(settings.value("geometry").toByteArray());
    
    if (settings.contains("splitter")) {
        m_splitter->restoreState(settings.value("splitter").toByteArray());
    }

    const bool painelVisivel = settings.value("painelVisivel", false).toBool();
    m_painelDir->setVisible(painelVisivel);
    m_actTogglePainel->setChecked(painelVisivel);

    const int abaIndex = settings.value("abaAtual", 0).toInt();
    m_tabWidget->setCurrentIndex(abaIndex);

    const int vozIndex = settings.value("vozIndex", 0).toInt();
    if (vozIndex >= 0 && vozIndex < m_comboVoz->count()) {
        m_comboVoz->setCurrentIndex(vozIndex);
    }
}

void MainWindow::salvarConfiguracoes()
{
    QSettings settings("EnsinadorDeIngles", "LeitorTecnico");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("splitter", m_splitter->saveState());
    settings.setValue("painelVisivel", m_painelDir->isVisible());
    settings.setValue("abaAtual", m_tabWidget->currentIndex());
    settings.setValue("vozIndex", m_comboVoz->currentIndex());
}

void MainWindow::onSalvarLayout()
{
    salvarConfiguracoes();
    appendLog("Layout atual salvo com sucesso nas preferências.", "success");
}

void MainWindow::onRestaurarLayoutPadrao()
{
    m_painelDir->setVisible(true);
    m_actTogglePainel->setChecked(true);
    m_tabWidget->setCurrentIndex(0);

    const int totalWidth = width();
    m_splitter->setSizes({int(totalWidth * 0.72), int(totalWidth * 0.28)});
    salvarConfiguracoes();
    appendLog("Layout restaurado para o padrão ideal.", "info");
}

void MainWindow::onAplicarProporcao(int leitorPercent)
{
    if (leitorPercent >= 100) {
        m_painelDir->setVisible(false);
        m_actTogglePainel->setChecked(false);
        appendLog("Modo foco total: leitor em 100% da janela.", "info");
        return;
    }

    m_painelDir->setVisible(true);
    m_actTogglePainel->setChecked(true);
    const int totalWidth = width();
    const int wLeitor = int(totalWidth * (leitorPercent / 100.0));
    const int wPainel = totalWidth - wLeitor;
    m_splitter->setSizes({wLeitor, wPainel});
}

void MainWindow::onTogglePainelLateral(bool visivel)
{
    m_painelDir->setVisible(visivel);
    m_actTogglePainel->setChecked(visivel);
    if (visivel) {
        const int totalWidth = width();
        if (m_splitter->sizes().value(1, 0) < 100) {
            m_splitter->setSizes({int(totalWidth * 0.72), int(totalWidth * 0.28)});
        }
    }
}

void MainWindow::onMudarAbaPainel(int index)
{
    onTogglePainelLateral(true);
    if (index >= 0 && index < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(index);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Funcionalidades de Tradução e Seleção
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::onAbrirPdf()
{
    const QString caminho = QFileDialog::getOpenFileName(
        this, "Selecionar Livro ou Documento PDF", QString(), "Documentos PDF (*.pdf);;Todos os Arquivos (*.*)");

    if (caminho.isEmpty()) return;

    m_pdfDoc->close();
    m_pdfDoc->load(caminho);

    if (m_pdfDoc->status() != QPdfDocument::Status::Ready) {
        QMessageBox::critical(this, "Erro", "Não foi possível abrir o arquivo PDF selecionado.");
        return;
    }

    const int total = m_pdfDoc->pageCount();
    m_spinPagina->setMaximum(total);
    m_spinPagina->setEnabled(total > 0);
    m_lblTotalPag->setText(QString("/ %1").arg(total));

    irParaPagina(0);
    atualizarInfoNavegacao();
    appendLog(QString("Documento carregado: %1 (%2 páginas)").arg(QFileInfo(caminho).fileName()).arg(total), "success");
}

void MainWindow::onFecharPdf()
{
    m_pdfDoc->close();
    m_spinPagina->setEnabled(false);
    m_spinPagina->setValue(1);
    m_lblTotalPag->setText("/ 0");
    m_btnPagAnterior->setEnabled(false);
    m_btnPagProxima->setEnabled(false);
    appendLog("Documento fechado.");
}

void MainWindow::onToggleModoCaptura(bool ativo)
{
    m_modoCaptura = ativo;
    m_btnModoCaptura->setChecked(ativo);

    const auto cursor = ativo ? Qt::CrossCursor : Qt::ArrowCursor;
    m_pdfView->viewport()->setCursor(cursor);
    m_pdfView2->viewport()->setCursor(cursor);

    if (ativo) {
        appendLog("Modo de seleção ativado: arraste o mouse sobre o parágrafo.");
    }
}

void MainWindow::onTraduzirDireto()
{
    if (m_textoOcrAtual.startsWith("BASE64:")) {
        const QByteArray b64 = m_textoOcrAtual.mid(7).toLatin1();
        m_net->traduzirDireto(QString(), b64);
    } else if (!m_textoOcrAtual.isEmpty()) {
        m_net->traduzirDireto(m_textoOcrAtual);
    }
}

void MainWindow::onTraducaoDiretaResultado(const QString &textoIngles, const QString &traducaoPortugues)
{
    m_textoOriginalEn = textoIngles;
    m_textoTraduzidoPt = traducaoPortugues;

    m_btnOuvir->setEnabled(!textoIngles.isEmpty());
    m_btnGravar->setEnabled(!textoIngles.isEmpty());

    // Se o painel estiver oculto, abre e foca na tradução
    if (!m_painelDir->isVisible()) {
        onTogglePainelLateral(true);
    }
    m_tabWidget->setCurrentIndex(0);

    m_txtTraducao->setHtml(
        "<div style='margin-bottom: 8px;'>"
        "<span style='color: #6b7280; font-size: 10px; font-weight: 600; text-transform: uppercase;'>Texto Original (EN)</span><br>"
        "<span style='color: #9ca3af; font-size: 12px; line-height: 1.4;'>" + textoIngles.toHtmlEscaped() + "</span>"
        "</div><hr style='border: 0; border-top: 1px solid #27282d; margin: 8px 0;'>"
        "<div>"
        "<span style='color: #60a5fa; font-size: 10px; font-weight: 600; text-transform: uppercase;'>Tradução Técnica (PT-BR)</span><br>"
        "<span style='color: #f3f4f6; font-size: 13px; font-weight: 500; line-height: 1.5;'>" + traducaoPortugues.toHtmlEscaped() + "</span>"
        "</div>"
    );

    appendLog("Tradução técnica concluída com sucesso.", "success");
}

void MainWindow::onCopiarTraducao()
{
    if (m_textoTraduzidoPt.isEmpty()) {
        appendLog("Nenhuma tradução disponível para copiar.", "warning");
        return;
    }
    QApplication::clipboard()->setText(m_textoTraduzidoPt);
    appendLog("Tradução copiada para a área de transferência.", "success");
}

// ═════════════════════════════════════════════════════════════════════════════
// Funcionalidades do Tutor de Pronúncia (TTS e Avaliação)
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::onOuvirPronuncia()
{
    const QString texto = m_textoOriginalEn.isEmpty()
        ? QApplication::clipboard()->text().trimmed()
        : m_textoOriginalEn;

    if (texto.isEmpty()) {
        appendLog("Selecione um texto antes para ouvir a pronúncia.", "warning");
        return;
    }

    const QString voz = m_comboVoz->currentData().toString();
    appendLog(QString("Reproduzindo pronúncia neural com voz '%1'...").arg(voz), "info");
    m_net->falarTexto(texto, voz);
}

void MainWindow::onFalaIniciada(const QString &voz)
{
    appendLog(QString("Reprodução de áudio iniciada (%1).").arg(voz), "success");
}

void MainWindow::onGravarVozTutor()
{
    if (!m_gravando) {
        if (m_textoOriginalEn.isEmpty()) {
            m_textoOriginalEn = QApplication::clipboard()->text().trimmed();
        }

        if (m_textoOriginalEn.isEmpty()) {
            appendLog("Selecione um parágrafo para praticar a leitura.", "warning");
            return;
        }

        m_gravando = true;
        m_btnGravar->setText("Parar e Avaliar");
        m_btnGravar->setStyleSheet("background-color: #dc2626; color: #ffffff; border: 1px solid #ef4444;");
        appendLog("Microfone aberto: leia o texto em inglês em voz alta...");
        m_net->iniciarGravacao();
    } else {
        m_gravando = false;
        m_btnGravar->setText("Gravar Minha Voz");
        m_btnGravar->setStyleSheet(QString());
        appendLog("Processando áudio com Whisper...");
        m_net->pararGravacao("en"); // Transcreve em inglês para o tutor
    }
}

void MainWindow::onTranscricaoResultado(const QString &texto)
{
    appendLog(QString("Whisper captou: \"%1\"").arg(texto), "info");

    // Envia automaticamente para avaliação fonética do tutor
    if (!m_textoOriginalEn.isEmpty()) {
        appendLog("Avaliando precisão da pronúncia com o tutor...", "info");
        m_net->avaliarPronuncia(m_textoOriginalEn, texto);
    }
}

void MainWindow::onAvaliacaoPronunciaResultado(int nota, const QString &feedback, const QStringList &palavrasAusentes)
{
    m_barAcuracia->setValue(nota);

    QString html = QString(
        "<div style='margin-bottom: 6px;'>"
        "<span style='color: #60a5fa; font-weight: 600; font-size: 13px;'>Nota de Correspondência: %1%</span>"
        "</div>"
        "<div style='margin-bottom: 8px; color: #e5e7eb; font-size: 12px; line-height: 1.4;'>%2</div>"
    ).arg(nota).arg(feedback.toHtmlEscaped());

    if (!palavrasAusentes.isEmpty()) {
        html += "<div style='color: #fbbf24; font-size: 11px; margin-top: 6px;'>"
                "<b>Atenção nestes termos:</b> " + palavrasAusentes.join(", ").toHtmlEscaped() +
                "</div>";
    }

    m_txtFeedbackTutor->setHtml(html);
    appendLog(QString("Avaliação do tutor concluída: %1% de acurácia.").arg(nota), "success");
}

void MainWindow::onPerfilAlterado(int index)
{
    if (index == 0) {
        appendLog("Perfil 'Econômico / Rápido' ativado (otimizado para notebooks e CPUs modestas).", "info");
    } else {
        appendLog("Perfil 'Alta Precisão' ativado.", "info");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Manipulação de Eventos do Mouse (Seleção Rubber Band)
// ═════════════════════════════════════════════════════════════════════════════

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    QPdfView *view = nullptr;
    if (watched == m_pdfView->viewport())   view = m_pdfView;
    if (watched == m_pdfView2->viewport())  view = m_pdfView2;

    if (!view) return QMainWindow::eventFilter(watched, event);

    m_activePdfView = view;

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_modoCaptura) {
            m_rbOrigin = me->pos();
            m_rubberBand->setParent(view->viewport());
            m_rubberBand->setGeometry(QRect(m_rbOrigin, QSize()));
            m_rubberBand->show();
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent*>(event);
        if (m_modoCaptura && m_rubberBand->isVisible()) {
            m_rubberBand->setGeometry(QRect(m_rbOrigin, me->pos()).normalized());
            return true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_modoCaptura && m_rubberBand->isVisible()) {
            const QRect rect = m_rubberBand->geometry();
            m_rubberBand->hide();
            onToggleModoCaptura(false);

            if (rect.width() > 15 && rect.height() > 10) {
                const QPixmap pix = view->viewport()->grab(rect);
                QByteArray bytes;
                QBuffer buf(&bytes);
                buf.open(QIODevice::WriteOnly);
                pix.save(&buf, "PNG");

                m_textoOcrAtual = "BASE64:" + bytes.toBase64();
                m_btnTraduzir->setEnabled(true);

                if (m_chkTraducaoAuto->isChecked()) {
                    onTraduzirDireto();
                } else {
                    appendLog("Região capturada. Clique em 'Traduzir Seleção'.");
                }
            }
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
// Navegação de Páginas e Zoom
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::irParaPagina(int pagina)
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) return;
    const int total = m_pdfDoc->pageCount();
    pagina = qBound(0, pagina, total - 1);
    m_paginaAtual = pagina;

    m_pdfView->pageNavigator()->jump(m_paginaAtual, {}, 0);

    if (m_modoVis == ModoVisualizacao::DuasPaginas) {
        if (m_paginaAtual + 1 < total) {
            m_pdfView2->pageNavigator()->jump(m_paginaAtual + 1, {}, 0);
            m_pdfView2->setVisible(true);
        } else {
            m_pdfView2->setVisible(false);
        }
    } else {
        m_pdfView2->setVisible(false);
    }

    atualizarInfoNavegacao();
}

void MainWindow::setModoVisualizacao(ModoVisualizacao modo)
{
    m_modoVis = modo;

    m_btnModo1Pag->setChecked(modo == ModoVisualizacao::UmaPagina);
    m_btnModo2Pag->setChecked(modo == ModoVisualizacao::DuasPaginas);
    m_btnModoCont->setChecked(modo == ModoVisualizacao::Continuo);

    switch (modo) {
    case ModoVisualizacao::UmaPagina:
        m_pdfView->setPageMode(QPdfView::PageMode::SinglePage);
        m_pdfView2->setVisible(false);
        irParaPagina(m_paginaAtual);
        break;
    case ModoVisualizacao::DuasPaginas:
        m_pdfView->setPageMode(QPdfView::PageMode::SinglePage);
        irParaPagina(m_paginaAtual);
        break;
    case ModoVisualizacao::Continuo:
        m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
        m_pdfView2->setVisible(false);
        break;
    }
}

void MainWindow::onModo1PagClicado() { setModoVisualizacao(ModoVisualizacao::UmaPagina); }
void MainWindow::onModo2PagClicado() { setModoVisualizacao(ModoVisualizacao::DuasPaginas); }
void MainWindow::onModoContClicado() { setModoVisualizacao(ModoVisualizacao::Continuo); }

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
    irParaPagina(pag - 1);
}

void MainWindow::atualizarInfoNavegacao()
{
    const int total = m_pdfDoc ? m_pdfDoc->pageCount() : 0;
    m_btnPagAnterior->setEnabled(m_paginaAtual > 0);
    m_btnPagProxima->setEnabled(m_paginaAtual < total - 1);

    m_spinPagina->blockSignals(true);
    m_spinPagina->setValue(m_paginaAtual + 1);
    m_spinPagina->blockSignals(false);
}

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

void MainWindow::onZoomMenos() { zoomDelta(-0.15); }
void MainWindow::onZoomMais()  { zoomDelta(0.15); }
void MainWindow::onZoomReset() { aplicarZoom(1.0); }

void MainWindow::onAjustarLargura()
{
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    if (m_pdfView2->isVisible()) m_pdfView2->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    atualizarLabelZoom();
}

void MainWindow::onAjustarPagina()
{
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    if (m_pdfView2->isVisible()) m_pdfView2->setZoomMode(QPdfView::ZoomMode::FitInView);
    atualizarLabelZoom();
}

void MainWindow::atualizarLabelZoom()
{
    const int perc = qRound(m_pdfView->zoomFactor() * 100);
    m_btnZoomReset->setText(QString("%1%").arg(perc));
}

// ═════════════════════════════════════════════════════════════════════════════
// Respostas de Rede e Registro
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::onServidorOnline(bool online)
{
    if (online) {
        m_lblStatus->setText("Motor de IA ativo");
    } else {
        m_lblStatus->setText("Motor offline (inicie ./iniciar_backend.sh)");
    }
}

void MainWindow::onRequisicaoIniciada(const QString &endpoint)
{
    if (endpoint == "/traduzir") {
        setButtonBusy(m_btnTraduzir, true);
        m_lblStatus->setText("Traduzindo...");
    } else if (endpoint == "/falar") {
        setButtonBusy(m_btnOuvir, true);
        m_lblStatus->setText("Gerando áudio da fala...");
    } else if (endpoint.startsWith("/parar_gravacao")) {
        setButtonBusy(m_btnGravar, true);
        m_lblStatus->setText("Transcrevendo com Whisper...");
    } else if (endpoint == "/avaliar_pronuncia") {
        m_lblStatus->setText("Analisando pronúncia...");
    }
}

void MainWindow::onRequisicaoConcluida(const QString &endpoint)
{
    if (endpoint == "/traduzir") setButtonBusy(m_btnTraduzir, false);
    if (endpoint == "/falar")    setButtonBusy(m_btnOuvir, false);
    if (endpoint.startsWith("/parar_gravacao")) setButtonBusy(m_btnGravar, false);
    m_lblStatus->setText("Pronto");
}

void MainWindow::onErroRequisicao(const QString &endpoint, const QString &mensagem)
{
    m_lblStatus->setText("Falha na operação");
    appendLog("Erro em " + endpoint + ": " + mensagem, "error");
}

void MainWindow::onLimparOcrResultado(const QString &texto)
{
    m_textoOriginalEn = texto;
    m_btnOuvir->setEnabled(true);
    m_btnGravar->setEnabled(true);
    appendLog("Texto extraído com sucesso.");
}

void MainWindow::onGravacaoIniciada()
{
    appendLog("Gravação ativa no microfone.");
}

void MainWindow::appendLog(const QString &mensagem, const QString &tipo)
{
    const QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString cor = "#9ca3af";
    if (tipo == "error")   cor = "#f87171";
    if (tipo == "warning") cor = "#fbbf24";
    if (tipo == "success") cor = "#34d399";

    if (m_logBox) {
        m_logBox->append(QString("<span style='color: #4b5563;'>[%1]</span> <span style='color: %2;'>%3</span>")
                         .arg(timestamp, cor, mensagem.toHtmlEscaped()));
        m_logBox->verticalScrollBar()->setValue(m_logBox->verticalScrollBar()->maximum());
    }
}

void MainWindow::setButtonBusy(QPushButton *btn, bool busy)
{
    if (!btn) return;
    btn->setEnabled(!busy);
}
