#include "MainWindow.h"
#include "NetworkManager.h"
#include "ThemeManager.h"
#include "PromptInputWidget.h"

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
#include <QTimer>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QAbstractSpinBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStatusBar>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QColorDialog>
#include <QTextStream>
#include <cmath>

#include <QTcpServer>

// ═════════════════════════════════════════════════════════════════════════════
// Construtor e Inicialização
// ═════════════════════════════════════════════════════════════════════════════

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Vellum — Leitor Técnico & Tutor");
    setWindowIcon(QIcon(":/app_icon.png"));
    setMinimumSize(960, 640);

    m_timerProgresso = new QTimer(this);
    connect(m_timerProgresso, &QTimer::timeout, this, &MainWindow::atualizarProgressoPasso);

    QTcpServer tempServer;
    if (tempServer.listen(QHostAddress::LocalHost, 0)) {
        m_backendPort = tempServer.serverPort();
        tempServer.close();
    }
    m_net = new NetworkManager(QString("http://localhost:%1").arg(m_backendPort), this);

    setupMenuBar();
    setupUi();
    m_themeManager = new ThemeManager(this);
    m_themeManager->applyTheme(ThemeManager::Theme::Dark);
    connectSignals();

    carregarConfiguracoes();

    setAcceptDrops(true);

    // Garante tela cheia e painel fechado sempre ao abrir
    showMaximized();
    m_painelDir->setVisible(false);
    m_actTogglePainel->setChecked(false);

    m_net->verificarConexao();

    // Se o backend não estiver rodando após breve intervalo, tenta auto-iniciar
    QTimer::singleShot(1200, this, [this]() {
        if (m_lblStatus && m_lblStatus->text().contains("offline", Qt::CaseInsensitive)) {
            verificarEIniciarBackend();
        }
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_isFechando = true;
    salvarConfiguracoes();
    pararBackend();
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

    m_actTelaCheia = menuExibir->addAction("Alternar Tela Cheia");
    m_actTelaCheia->setShortcut(QKeySequence(Qt::Key_F11));
    m_actTelaCheia->setShortcutContext(Qt::ApplicationShortcut);
    addAction(m_actTelaCheia);
    connect(m_actTelaCheia, &QAction::triggered, this, &MainWindow::onToggleTelaCheia);

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

    auto *actAbaChat = menuPainel->addAction("Ir para Assistente IA & Dúvidas");
    actAbaChat->setShortcut(QKeySequence("Ctrl+I"));
    connect(actAbaChat, &QAction::triggered, this, [this]() { onMudarAbaPainel(2); });

    auto *actAbaNotas = menuPainel->addAction("Ir para Bloco de Notas");
    actAbaNotas->setShortcut(QKeySequence("Ctrl+N"));
    connect(actAbaNotas, &QAction::triggered, this, [this]() { onMudarAbaPainel(3); });

    auto *actAbaConfig = menuPainel->addAction("Ir para Desempenho & Layout");
    actAbaConfig->setShortcut(QKeySequence("Ctrl+D"));
    connect(actAbaConfig, &QAction::triggered, this, [this]() { onMudarAbaPainel(4); });

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

    // ── Menu Configurações ────────────────────────────────────────────────
    auto *menuConfig = menuBar->addMenu("&Configurações");

    auto *actConfigurarIA = menuConfig->addAction("Chaves de API e IA (Gemini / Ollama)...");
    actConfigurarIA->setShortcut(QKeySequence("Ctrl+Shift+I"));
    connect(actConfigurarIA, &QAction::triggered, this, &MainWindow::onConfigurarIA);

    // ── Indicador de Modelo Ativo e Progresso no Canto Superior Direito ───
    auto *cornerWidget = new QWidget(this);
    auto *cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 10, 0);
    cornerLayout->setSpacing(8);

    m_lblProgressoTopo = new QLabel(cornerWidget);
    m_lblProgressoTopo->setStyleSheet("color: #38bdf8; font-weight: 700; font-size: 11px;");
    m_lblProgressoTopo->setVisible(false);
    cornerLayout->addWidget(m_lblProgressoTopo);

    m_badgeModeloAtivo = new QLabel("Tradução: —", cornerWidget);
    m_badgeModeloAtivo->setObjectName("badgeModeloAtivo");
    m_badgeModeloAtivo->setStyleSheet(
        "background-color: #1e2025; color: #cbd5e1; border: 1px solid #2e323b; "
        "border-radius: 4px; padding: 2px 10px; font-size: 11px; font-weight: 600;"
    );
    cornerLayout->addWidget(m_badgeModeloAtivo);

    menuBar->setCornerWidget(cornerWidget, Qt::TopRightCorner);
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
    m_pdfView->installEventFilter(this);
    m_pdfView->viewport()->installEventFilter(this);

    m_pdfView2 = new QPdfView(viewContainer);
    m_pdfView2->setDocument(m_pdfDoc);
    m_pdfView2->setPageMode(QPdfView::PageMode::SinglePage);
    m_pdfView2->setZoomMode(QPdfView::ZoomMode::FitInView);
    m_pdfView2->setVisible(false);
    m_pdfView2->installEventFilter(this);
    m_pdfView2->viewport()->installEventFilter(this);

    m_activePdfView = m_pdfView;

    m_emptyStateWidget = new QWidget(viewContainer);
    m_emptyStateWidget->setObjectName("emptyStateWidget");
    m_emptyStateWidget->setStyleSheet(
        "QWidget#emptyStateWidget {"
        "  background-color: #0f1115;"
        "}"
    );
    auto *emptyLayout = new QVBoxLayout(m_emptyStateWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(14);

    auto *lblIcone = new QLabel(m_emptyStateWidget);
    lblIcone->setPixmap(QPixmap(":/app_icon.png").scaled(92, 92, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    lblIcone->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(lblIcone);

    auto *lblTitulo = new QLabel("Vellum — Leitor Técnico & Tutor", m_emptyStateWidget);
    lblTitulo->setStyleSheet("color: #f1f5f9; font-size: 19px; font-weight: 700; letter-spacing: 0.5px;");
    lblTitulo->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(lblTitulo);

    auto *lblSub = new QLabel("Arraste e solte um livro técnico PDF aqui ou use o menu Arquivo > Abrir (Ctrl+O)", m_emptyStateWidget);
    lblSub->setStyleSheet("color: #94a3b8; font-size: 13px;");
    lblSub->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(lblSub);

    auto *btnAbrirInicial = new QPushButton("Selecionar Livro PDF...", m_emptyStateWidget);
    btnAbrirInicial->setObjectName("btnPrimario");
    btnAbrirInicial->setCursor(Qt::PointingHandCursor);
    btnAbrirInicial->setFixedSize(200, 38);
    connect(btnAbrirInicial, &QPushButton::clicked, this, &MainWindow::onAbrirPdf);
    emptyLayout->addWidget(btnAbrirInicial, 0, Qt::AlignCenter);

    viewLayout->addWidget(m_emptyStateWidget, 1);
    viewLayout->addWidget(m_pdfView, 1);
    viewLayout->addWidget(m_pdfView2, 1);

    m_pdfView->setVisible(false);
    m_emptyStateWidget->setVisible(true);

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
    m_tabWidget->addTab(criarAbaChatIA(), "Assistente IA");
    m_tabWidget->addTab(criarAbaAnotacoes(), "Bloco de Notas");
    m_tabWidget->addTab(criarAbaConfiguracoes(), "Desempenho & Layout");

    layoutDir->addWidget(m_tabWidget);

    m_splitter->addWidget(m_painelDir);

    // Inicia fechado por padrão para tela 100% limpa
    m_painelDir->setVisible(false);

    m_splitter->setStretchFactor(0, 75);
    m_splitter->setStretchFactor(1, 25);

    // ── Barra de Status Inferior Minimalista ──────────────────────────────
    auto *bar = statusBar();
    bar->setStyleSheet("QStatusBar { background-color: #16171a; border-top: 1px solid #27282d; color: #94a3b8; font-size: 11px; padding: 2px 8px; }");

    m_lblStatusGeral = new QLabel("Pronto", bar);
    m_lblStatusGeral->setStyleSheet("color: #64748b; font-size: 11px;");
    bar->addWidget(m_lblStatusGeral, 1);

    m_lblProgressoNumerico = new QLabel("", bar);
    m_lblProgressoNumerico->setStyleSheet("color: #38bdf8; font-weight: 600; font-size: 11px; margin-right: 12px;");
    bar->addPermanentWidget(m_lblProgressoNumerico);
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

    m_promptTraducao = new PromptInputWidget(aba);
    m_promptTraducao->setToolbarVisible(false);
    m_promptTraducao->setReadOnly(true);
    m_promptTraducao->setObjectName("promptTraducao");
    
    m_promptTraducao->setPlaceholderText("Selecione um parágrafo no documento para visualizar o texto original e a tradução técnica.");
    layout->addWidget(m_promptTraducao, 1);

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

    auto *layoutVozVel = new QHBoxLayout();
    layoutVozVel->setSpacing(6);

    auto *boxVoz = new QVBoxLayout();
    auto *lblVoz = new QLabel("Voz do Tutor:", aba);
    lblVoz->setObjectName("lblSecao");
    m_comboVoz = new QComboBox(aba);
    m_comboVoz->setObjectName("comboVoz");
    m_comboVoz->addItem("US Jenny (Feminina)", "en-US-JennyNeural");
    m_comboVoz->addItem("US Guy (Masculina)", "en-US-GuyNeural");
    m_comboVoz->addItem("UK Sonia (Feminina)", "en-GB-SoniaNeural");
    m_comboVoz->addItem("UK Ryan (Masculina)", "en-GB-RyanNeural");
    m_comboVoz->addItem("AU Natasha (Feminina)", "en-AU-NatashaNeural");
    boxVoz->addWidget(lblVoz);
    boxVoz->addWidget(m_comboVoz);

    auto *boxVel = new QVBoxLayout();
    auto *lblVel = new QLabel("Velocidade:", aba);
    lblVel->setObjectName("lblSecao");
    m_comboVelocidade = new QComboBox(aba);
    m_comboVelocidade->setObjectName("comboVelocidade");
    m_comboVelocidade->addItem("0.75x (Lenta)", "-25%");
    m_comboVelocidade->addItem("1.0x (Normal)", "+0%");
    m_comboVelocidade->addItem("1.25x (Rápida)", "+25%");
    m_comboVelocidade->addItem("1.5x (Avançada)", "+50%");
    m_comboVelocidade->setCurrentIndex(1);
    boxVel->addWidget(lblVel);
    boxVel->addWidget(m_comboVelocidade);

    layoutVozVel->addLayout(boxVoz, 65);
    layoutVozVel->addLayout(boxVel, 35);
    layout->addLayout(layoutVozVel);

    auto *lblNivel = new QLabel("Nível de Exigência do Tutor:", aba);
    lblNivel->setObjectName("lblSecao");
    layout->addWidget(lblNivel);

    m_comboNivelTutor = new QComboBox(aba);
    m_comboNivelTutor->setObjectName("comboNivelTutor");
    m_comboNivelTutor->addItem("Iniciante (Flexível & Encorajador)", "iniciante");
    m_comboNivelTutor->addItem("Intermediário (Equilibrado)", "intermediario");
    m_comboNivelTutor->addItem("Avançado (Exigente / Rigor Técnico)", "avancado");
    m_comboNivelTutor->setCurrentIndex(1);
    layout->addWidget(m_comboNivelTutor);

    auto *lblTextoPratica = new QLabel("Texto para Prática:", aba);
    lblTextoPratica->setObjectName("lblSecao");
    layout->addWidget(lblTextoPratica);

    m_promptTextoTutor = new PromptInputWidget(aba);
    m_promptTextoTutor->setToolbarVisible(false);
    m_promptTextoTutor->setObjectName("promptTextoTutor");
    m_promptTextoTutor->setPlaceholderText("O texto selecionado no PDF aparecerá aqui. Você também pode digitar ou colar o texto em inglês que deseja praticar.");
    layout->addWidget(m_promptTextoTutor, 1);

    auto *layoutBotoes = new QHBoxLayout();
    layoutBotoes->setSpacing(6);

    m_btnOuvir = new QPushButton("Ouvir Pronúncia", aba);
    m_btnOuvir->setObjectName("btnSecundario");
    m_btnOuvir->setEnabled(false);
    layoutBotoes->addWidget(m_btnOuvir);

    m_btnPararAudio = new QPushButton("Parar Fala", aba);
    m_btnPararAudio->setObjectName("btnSecundario");
    layoutBotoes->addWidget(m_btnPararAudio);

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

    auto *lblFeedback = new QLabel("Feedback:", aba);
    lblFeedback->setObjectName("lblSecao");
    layout->addWidget(lblFeedback);

    m_txtFeedbackTutor = new QTextEdit(aba);
    m_txtFeedbackTutor->setObjectName("txtFeedback");
    m_txtFeedbackTutor->setReadOnly(true);
    m_txtFeedbackTutor->setPlaceholderText("Grave sua leitura do texto selecionado para receber análise de pronúncia e dicas fonéticas do tutor.");
    layout->addWidget(m_txtFeedbackTutor, 1);

    return aba;
}

// ─────────────────────────────────────────────────────────────────────────────
// Aba 3: Assistente IA & Chat Técnico Multimodal
// ─────────────────────────────────────────────────────────────────────────────

QWidget* MainWindow::criarAbaChatIA()
{
    auto *aba = new QWidget();
    auto *layout = new QVBoxLayout(aba);
    layout->setContentsMargins(4, 8, 4, 4);
    layout->setSpacing(6);

    // Barra Superior: seletor de IA + status + botoes
    auto *layoutHeader = new QHBoxLayout();
    layoutHeader->setSpacing(6);

    // Seletor rapido de IA (como o Antigravity)
    m_comboChatIA = new QComboBox(aba);
    m_comboChatIA->setObjectName("comboChatIA");
    m_comboChatIA->setToolTip("Selecionar IA para responder");
    m_comboChatIA->addItem("Gemini",  "gemini");
    m_comboChatIA->addItem("Ollama",  "ollama");
    m_comboChatIA->setStyleSheet(
        "QComboBox#comboChatIA {"
        "  background-color: #1c1e24; border: 1px solid #334155;"
        "  border-radius: 5px; padding: 3px 8px; color: #60a5fa;"
        "  font-size: 11px; font-weight: 600; min-width: 80px;"
        "}"
        "QComboBox#comboChatIA::drop-down { border: none; }"
        "QComboBox#comboChatIA::down-arrow { image: none; width: 0; }"
        "QComboBox#comboChatIA QAbstractItemView {"
        "  background-color: #1c1e24; border: 1px solid #334155;"
        "  color: #e2e8f0; selection-background-color: #2563eb;"
        "}"
    );
    layoutHeader->addWidget(m_comboChatIA);

    // Label mostra o modelo do provedor selecionado (atualizado depois no carregarConfiguracoes)
    m_lblModeloAtivoChat = new QLabel(QStringLiteral("—"), aba);
    m_lblModeloAtivoChat->setStyleSheet("color: #64748b; font-size: 10px; font-weight: 500;");
    layoutHeader->addWidget(m_lblModeloAtivoChat, 1);

    m_btnConfigurarIA = new QPushButton("Configurar API", aba);
    m_btnConfigurarIA->setObjectName("btnSecundario");
    m_btnConfigurarIA->setToolTip("Configurar chave da API Gemini ou ativar modo offline");
    layoutHeader->addWidget(m_btnConfigurarIA);

    m_btnLimparChat = new QPushButton("Limpar", aba);
    m_btnLimparChat->setObjectName("btnSecundario");
    m_btnLimparChat->setToolTip("Limpar historico de conversa");
    layoutHeader->addWidget(m_btnLimparChat);

    layout->addLayout(layoutHeader);

    // Historico da Conversa
    m_chatHistorico = new QTextBrowser(aba);
    m_chatHistorico->setObjectName("chatHistorico");
    m_chatHistorico->setOpenExternalLinks(true);
    m_chatHistorico->setStyleSheet(
        "QTextBrowser#chatHistorico {"
        "  background-color: #12141a;"
        "  border: 1px solid #282c37;"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "  color: #e2e8f0;"
        "  font-size: 13px;"
        "  line-height: 1.5;"
        "}"
    );
    m_chatHistorico->setHtml(
        "<div style='color: #94a3b8; font-size: 13px; line-height: 1.5;'>"
        "<b style='color: #60a5fa;'>Assistente Tecnico</b><br>"
        "Faca perguntas conceituais ou anexe circuitos para analise da IA.<br><br>"
        "<b>Instrucoes:</b><br>"
        "• <b>Capturar Circuito:</b> Selecione um esquema diretamente no PDF com o mouse.<br>"
        "• <b>Colar Trecho:</b> Insere o texto selecionado na pergunta.<br>"
        "• <b>Configurar API:</b> Insira sua chave Gemini ou ative o modo offline (Ollama)."
        "</div>"
    );
    layout->addWidget(m_chatHistorico, 1);

    m_promptInput = new PromptInputWidget(aba);
    layout->addWidget(m_promptInput);

    return aba;
}

// ─────────────────────────────────────────────────────────────────────────────
// Aba 4: Bloco de Notas / Anotações
// ─────────────────────────────────────────────────────────────────────────────

QWidget* MainWindow::criarAbaAnotacoes()
{
    auto *aba = new QWidget();
    auto *layout = new QVBoxLayout(aba);
    layout->setContentsMargins(4, 8, 4, 4);
    layout->setSpacing(6);

    // Barra 1: Arquivo e Ações
    auto *layoutAcoes = new QHBoxLayout();
    layoutAcoes->setSpacing(6);

    m_btnNovoNota = new QPushButton("Novo", aba);
    m_btnNovoNota->setObjectName("btnSecundario");
    m_btnNovoNota->setToolTip("Iniciar um novo bloco de anotações em branco");
    layoutAcoes->addWidget(m_btnNovoNota);

    m_btnAbrirNota = new QPushButton("Abrir...", aba);
    m_btnAbrirNota->setObjectName("btnSecundario");
    m_btnAbrirNota->setToolTip("Abrir anotações salvas anteriormente (.html ou .txt)");
    layoutAcoes->addWidget(m_btnAbrirNota);

    m_btnSalvarNota = new QPushButton("Salvar...", aba);
    m_btnSalvarNota->setObjectName("btnPrimario");
    m_btnSalvarNota->setToolTip("Salvar anotações atuais em disco");
    layoutAcoes->addWidget(m_btnSalvarNota);

    m_btnCopiarNota = new QPushButton("Copiar Tudo", aba);
    m_btnCopiarNota->setObjectName("btnSecundario");
    m_btnCopiarNota->setToolTip("Copiar todas as anotações para a área de transferência");
    layoutAcoes->addWidget(m_btnCopiarNota);

    layoutAcoes->addStretch();

    m_lblArquivoNota = new QLabel("Novo Documento", aba);
    m_lblArquivoNota->setStyleSheet("color: #64748b; font-size: 11px; font-weight: 500; font-style: italic;");
    layoutAcoes->addWidget(m_lblArquivoNota);

    layout->addLayout(layoutAcoes);

    // Barra 2: Formatação (Fonte, Tamanho, B/I/U, Cor)
    auto *layoutFormatacao = new QHBoxLayout();
    layoutFormatacao->setSpacing(5);

    // Fonte
    m_comboFonteNota = new QComboBox(aba);
    m_comboFonteNota->setObjectName("comboFonteNota");
    m_comboFonteNota->setStyleSheet(
        "QComboBox#comboFonteNota {"
        "  background-color: #1a1c22; border: 1px solid #2d313b; border-radius: 5px;"
        "  padding: 3px 8px; color: #f1f5f9; font-size: 11px; min-width: 110px;"
        "}"
        "QComboBox#comboFonteNota QAbstractItemView {"
        "  background-color: #1a1c22; border: 1px solid #334155; color: #f1f5f9; selection-background-color: #2563eb;"
        "}"
    );
    m_comboFonteNota->addItem("Segoe UI", "Segoe UI");
    m_comboFonteNota->addItem("Arial", "Arial");
    m_comboFonteNota->addItem("Roboto", "Roboto");
    m_comboFonteNota->addItem("Times New Roman", "Times New Roman");
    m_comboFonteNota->addItem("Consolas", "Consolas");
    m_comboFonteNota->addItem("JetBrains Mono", "JetBrains Mono");
    layoutFormatacao->addWidget(m_comboFonteNota);

    // Tamanho da fonte
    m_spinTamanhoNota = new QSpinBox(aba);
    m_spinTamanhoNota->setRange(9, 36);
    m_spinTamanhoNota->setValue(13);
    m_spinTamanhoNota->setSuffix(" pt");
    m_spinTamanhoNota->setStyleSheet(
        "QSpinBox {"
        "  background-color: #1a1c22; border: 1px solid #2d313b; border-radius: 5px;"
        "  padding: 3px 4px; color: #f1f5f9; font-size: 11px; min-width: 55px;"
        "}"
    );
    layoutFormatacao->addWidget(m_spinTamanhoNota);

    m_btnFonteMenosNota = new QPushButton("-", aba);
    m_btnFonteMenosNota->setFixedSize(28, 26);
    m_btnFonteMenosNota->setToolTip("Diminuir tamanho da fonte (-)");
    m_btnFonteMenosNota->setStyleSheet(
        "QPushButton { background-color: #202227; color: #f1f5f9; border: 1px solid #2d3038; border-radius: 4px; padding: 0px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #292c33; border-color: #3f434d; color: #ffffff; }"
        "QPushButton:pressed { background-color: #1b1c20; }"
    );
    layoutFormatacao->addWidget(m_btnFonteMenosNota);

    m_btnFonteMaisNota = new QPushButton("+", aba);
    m_btnFonteMaisNota->setFixedSize(28, 26);
    m_btnFonteMaisNota->setToolTip("Aumentar tamanho da fonte (+)");
    m_btnFonteMaisNota->setStyleSheet(
        "QPushButton { background-color: #202227; color: #f1f5f9; border: 1px solid #2d3038; border-radius: 4px; padding: 0px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #292c33; border-color: #3f434d; color: #ffffff; }"
        "QPushButton:pressed { background-color: #1b1c20; }"
    );
    layoutFormatacao->addWidget(m_btnFonteMaisNota);

    // Separador vertical
    auto *sep = new QFrame(aba);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color: #2d313b;");
    layoutFormatacao->addWidget(sep);

    // Estilos N (Negrito), I (Itálico), S (Sublinhado)
    m_btnBoldNota = new QPushButton("N", aba);
    m_btnBoldNota->setCheckable(true);
    m_btnBoldNota->setFixedSize(28, 26);
    m_btnBoldNota->setToolTip("Negrito (N)");
    m_btnBoldNota->setStyleSheet(
        "QPushButton { background-color: #202227; color: #f1f5f9; border: 1px solid #2d3038; border-radius: 4px; padding: 0px; font-weight: 800; font-size: 13px; }"
        "QPushButton:hover { background-color: #292c33; border-color: #3f434d; color: #ffffff; }"
        "QPushButton:checked { background-color: #2563eb; color: #ffffff; border-color: #3b82f6; }"
    );
    layoutFormatacao->addWidget(m_btnBoldNota);

    m_btnItalicNota = new QPushButton("I", aba);
    m_btnItalicNota->setCheckable(true);
    m_btnItalicNota->setFixedSize(28, 26);
    m_btnItalicNota->setToolTip("Itálico (I)");
    m_btnItalicNota->setStyleSheet(
        "QPushButton { background-color: #202227; color: #f1f5f9; border: 1px solid #2d3038; border-radius: 4px; padding: 0px; font-style: italic; font-weight: 700; font-size: 13px; font-family: serif; }"
        "QPushButton:hover { background-color: #292c33; border-color: #3f434d; color: #ffffff; }"
        "QPushButton:checked { background-color: #2563eb; color: #ffffff; border-color: #3b82f6; }"
    );
    layoutFormatacao->addWidget(m_btnItalicNota);

    m_btnUnderlineNota = new QPushButton("S", aba);
    m_btnUnderlineNota->setCheckable(true);
    m_btnUnderlineNota->setFixedSize(28, 26);
    m_btnUnderlineNota->setToolTip("Sublinhado (S)");
    m_btnUnderlineNota->setStyleSheet(
        "QPushButton { background-color: #202227; color: #f1f5f9; border: 1px solid #2d3038; border-radius: 4px; padding: 0px; font-weight: 700; font-size: 13px; text-decoration: underline; }"
        "QPushButton:hover { background-color: #292c33; border-color: #3f434d; color: #ffffff; }"
        "QPushButton:checked { background-color: #2563eb; color: #ffffff; border-color: #3b82f6; }"
    );
    layoutFormatacao->addWidget(m_btnUnderlineNota);

    // Cor
    m_btnCorNota = new QPushButton("Cor...", aba);
    m_btnCorNota->setFixedHeight(26);
    m_btnCorNota->setToolTip("Alterar cor do texto");
    m_btnCorNota->setStyleSheet(
        "QPushButton { background-color: #202227; color: #f1f5f9; border: 1px solid #2d3038; border-radius: 4px; padding: 3px 10px; font-size: 11px; font-weight: 500; }"
        "QPushButton:hover { background-color: #292c33; border-color: #3f434d; color: #ffffff; }"
    );
    layoutFormatacao->addWidget(m_btnCorNota);

    layoutFormatacao->addStretch();
    layout->addLayout(layoutFormatacao);

    // Área de Edição
    m_txtAnotacoes = new QTextEdit(aba);
    m_txtAnotacoes->setObjectName("txtAnotacoes");
    m_txtAnotacoes->setPlaceholderText("Escreva aqui suas anotações de estudo, resumos e fórmulas do livro...\n\nVocê pode salvar em disco a qualquer momento para retomar seus estudos mais tarde.");
    m_txtAnotacoes->setStyleSheet(
        "QTextEdit#txtAnotacoes {"
        "  background-color: #12141a;"
        "  border: 1px solid #282c37;"
        "  border-radius: 8px;"
        "  padding: 12px;"
        "  color: #e2e8f0;"
        "  font-size: 13px;"
        "  font-family: 'Segoe UI', 'Arial', sans-serif;"
        "  line-height: 1.6;"
        "  selection-background-color: #2563eb;"
        "}"
        "QTextEdit#txtAnotacoes:focus {"
        "  border: 1px solid #3b82f6;"
        "}"
    );
    layout->addWidget(m_txtAnotacoes, 1);

    // Rodapé de Status
    auto *layoutStatus = new QHBoxLayout();
    m_lblStatusNota = new QLabel("0 palavras | 0 caracteres", aba);
    m_lblStatusNota->setStyleSheet("color: #64748b; font-size: 11px;");
    layoutStatus->addWidget(m_lblStatusNota);
    layoutStatus->addStretch();

    layout->addLayout(layoutStatus);
    return aba;
}

// ─────────────────────────────────────────────────────────────────────────────
// Aba 5: Desempenho & Layout
// ─────────────────────────────────────────────────────────────────────────────

QWidget* MainWindow::criarAbaConfiguracoes()
{
    auto *aba = new QWidget();
    auto *layout = new QVBoxLayout(aba);
    layout->setContentsMargins(4, 8, 4, 4);
    layout->setSpacing(10);

    // ── Tier de Hardware ──────────────────────────────────────────────────
    auto *lblPerfil = new QLabel("Perfil de Hardware:", aba);
    lblPerfil->setObjectName("lblSecao");
    layout->addWidget(lblPerfil);

    m_comboPerfil = new QComboBox(aba);
    m_comboPerfil->setObjectName("comboPerfil");
    // Tier 0: tudo na nuvem via chave API cadastrada (zero carga local alem de Tesseract)
    m_comboPerfil->addItem("Nuvem    — API Gemini/OpenAI  (sem Ollama, requer chave)",  "nuvem");
    m_comboPerfil->addItem("Basico   — phi3:mini + Whisper tiny.en  (~3 GB RAM)",        "basico");
    m_comboPerfil->addItem("Medio    — llama3.2:3b + Whisper base.en (~6 GB RAM)",       "medio");
    m_comboPerfil->addItem("Avancado — llama3 + Whisper base.en     (~12 GB RAM)",       "avancado");
    layout->addWidget(m_comboPerfil);

    auto *lblTierInfo = new QLabel(
        "<span style='color:#64748b; font-size:10px;'>"
        "Trocar o perfil requer reiniciar o backend para ter efeito."
        "</span>", aba);
    layout->addWidget(lblTierInfo);

    // ── Tema da Interface ──────────────────────────────────────────────────
    auto *lblTema = new QLabel("Tema da Interface:", aba);
    lblTema->setObjectName("lblSecao");
    layout->addWidget(lblTema);

    m_comboTema = new QComboBox(aba);
    m_comboTema->setObjectName("comboTema");
    m_comboTema->addItem("Escuro", "dark");
    m_comboTema->addItem("Claro", "light");
    m_comboTema->addItem("Sistema", "system");
    layout->addWidget(m_comboTema);

    // ── Gerenciamento de Layout ───────────────────────────────────────────
    auto *lblLayout = new QLabel("Gerenciamento de Layout:", aba);
    lblLayout->setObjectName("lblSecao");
    layout->addWidget(lblLayout);

    auto *layoutBtnsLayout = new QHBoxLayout();
    m_btnSalvarLayout = new QPushButton("Salvar Layout", aba);
    m_btnSalvarLayout->setObjectName("btnSecundario");
    layoutBtnsLayout->addWidget(m_btnSalvarLayout);

    m_btnRestaurarLayout = new QPushButton("Restaurar Padrao", aba);
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

    // ── Log ───────────────────────────────────────────────────────────────
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

    m_btnTelaCheia = new QPushButton("Tela Cheia", bar);
    m_btnTelaCheia->setObjectName("btnNav");
    m_btnTelaCheia->setToolTip("Alternar modo Tela Cheia sem as barras superiores (F11)");

    layout->addWidget(m_btnZoomMenos);
    layout->addWidget(m_btnZoomReset);
    layout->addWidget(m_btnZoomMais);
    layout->addWidget(m_btnAjustarLarg);
    layout->addWidget(m_btnAjustarPag);
    layout->addWidget(m_btnTelaCheia);

    layout->addStretch();
    return bar;
}

// ═════════════════════════════════════════════════════════════════════════════
// Folha de Estilos (Design Sóbrio, Minimalista, Dark Slate)
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::setupStyleSheet()
{
    // O estilo agora é gerenciado pelo ThemeManager.
    // O CSS original foi movido para resources/themes/dark.qss.
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
    connect(m_btnOuvir,      &QPushButton::clicked, this, &MainWindow::onOuvirPronuncia);
    connect(m_btnPararAudio, &QPushButton::clicked, this, &MainWindow::onPararAudio);
    connect(m_btnGravar,     &QPushButton::clicked, this, &MainWindow::onGravarVozTutor);

    // Layout
    connect(m_btnSalvarLayout,    &QPushButton::clicked, this, &MainWindow::onSalvarLayout);
    connect(m_btnRestaurarLayout, &QPushButton::clicked, this, &MainWindow::onRestaurarLayoutPadrao);
    connect(m_btnProporcao100,    &QPushButton::clicked, this, [this]() { onAplicarProporcao(100); });
    connect(m_btnProporcao70,     &QPushButton::clicked, this, [this]() { onAplicarProporcao(70); });
    connect(m_btnProporcao50,     &QPushButton::clicked, this, [this]() { onAplicarProporcao(50); });

    // Configurações
    connect(m_comboChatIA, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        const QString prov = m_comboChatIA->currentData().toString();
        m_iaProvedor = prov;
        if (m_lblModeloAtivoChat) {
            if (prov == "gemini") {
                if (m_iaModelo.isEmpty() || m_iaModelo.contains("llama") || m_iaModelo.contains("phi")) {
                    m_iaModelo = "gemini-3.5-flash-lite";
                }
                m_lblModeloAtivoChat->setText(m_iaModelo);
            } else {
                m_lblModeloAtivoChat->setText(
                    m_ollamaModelTier.isEmpty() ? "llama3" : m_ollamaModelTier
                );
            }
        }
        atualizarBadgeModeloAtivo();
        salvarConfiguracoes();
    });

    connect(m_comboPerfil, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPerfilAlterado);

    connect(m_comboTema, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (!m_themeManager) return;
        QString tema = m_comboTema->currentData().toString();
        if (tema == "dark") m_themeManager->applyTheme(ThemeManager::Theme::Dark);
        else if (tema == "light") m_themeManager->applyTheme(ThemeManager::Theme::Light);
        else m_themeManager->applyTheme(ThemeManager::Theme::System);
    });

    // Navegação
    connect(m_btnPagAnterior,  &QPushButton::clicked, this, &MainWindow::onPaginaAnterior);
    connect(m_btnPagProxima,   &QPushButton::clicked, this, &MainWindow::onPaginaProxima);
    connect(m_spinPagina, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPaginaSpinChanged);

    // Modos
    connect(m_btnModo1Pag,     &QPushButton::clicked, this, &MainWindow::onModo1PagClicado);
    connect(m_btnModo2Pag,     &QPushButton::clicked, this, &MainWindow::onModo2PagClicado);
    connect(m_btnModoCont,     &QPushButton::clicked, this, &MainWindow::onModoContClicado);

    // Zoom e Janela
    connect(m_btnZoomMenos,    &QPushButton::clicked, this, &MainWindow::onZoomMenos);
    connect(m_btnZoomMais,     &QPushButton::clicked, this, &MainWindow::onZoomMais);
    connect(m_btnZoomReset,    &QPushButton::clicked, this, &MainWindow::onZoomReset);
    connect(m_btnAjustarLarg,  &QPushButton::clicked, this, &MainWindow::onAjustarLargura);
    connect(m_btnAjustarPag,   &QPushButton::clicked, this, &MainWindow::onAjustarPagina);
    connect(m_btnTelaCheia,    &QPushButton::clicked, this, &MainWindow::onToggleTelaCheia);

    // NetworkManager
    connect(m_net, &NetworkManager::traducaoDiretaResultado,      this, &MainWindow::onTraducaoDiretaResultado);
    connect(m_net, &NetworkManager::limparOcrResultado,           this, &MainWindow::onLimparOcrResultado);
    connect(m_net, &NetworkManager::transcricaoResultado,         this, &MainWindow::onTranscricaoResultado);
    connect(m_net, &NetworkManager::servidorOnline,               this, &MainWindow::onServidorOnline);
    connect(m_net, &NetworkManager::modelosStatusRecebido,        this, &MainWindow::onModelosStatusRecebido);
    connect(m_net, &NetworkManager::requisicaoIniciada,           this, &MainWindow::onRequisicaoIniciada);
    connect(m_net, &NetworkManager::requisicaoConcluida,          this, &MainWindow::onRequisicaoConcluida);
    connect(m_net, &NetworkManager::erroRequisicao,               this, &MainWindow::onErroRequisicao);
    connect(m_net, &NetworkManager::gravacaoIniciada,             this, &MainWindow::onGravacaoIniciada);
    connect(m_net, &NetworkManager::falaIniciada,                 this, &MainWindow::onFalaIniciada);
    connect(m_net, &NetworkManager::avaliacaoPronunciaResultado,  this, &MainWindow::onAvaliacaoPronunciaResultado);
    connect(m_net, &NetworkManager::chatRespostaResultado,        this, &MainWindow::onChatRespostaResultado);

    // Assistente IA & Chat
    connect(m_btnConfigurarIA,     &QPushButton::clicked, this, &MainWindow::onConfigurarIA);
    connect(m_btnLimparChat,       &QPushButton::clicked, this, &MainWindow::onLimparChat);
    
    if (m_promptInput) {
        connect(m_promptInput, &PromptInputWidget::sendRequested, this, [this](const QString& text) {
            onEnviarChat();
        });
        connect(m_promptInput, &PromptInputWidget::captureCircuitRequested, this, &MainWindow::onCapturarCircuitoChat);
        connect(m_promptInput, &PromptInputWidget::attachImageRequested, this, &MainWindow::onAnexarImagemChat);
        connect(m_promptInput, &PromptInputWidget::removeImageRequested, this, &MainWindow::onRemoverImagemChat);
        connect(m_promptInput, &PromptInputWidget::pasteSnippetRequested, this, &MainWindow::onColarTrechoChat);
    }

    // ── Bloco de Notas ───────────────────────────────────────────────────
    if (m_btnNovoNota)       connect(m_btnNovoNota,       &QPushButton::clicked, this, &MainWindow::onLimparAnotacoes);
    if (m_btnAbrirNota)      connect(m_btnAbrirNota,      &QPushButton::clicked, this, &MainWindow::onAbrirAnotacoes);
    if (m_btnSalvarNota)     connect(m_btnSalvarNota,     &QPushButton::clicked, this, &MainWindow::onSalvarAnotacoes);
    if (m_btnCopiarNota)     connect(m_btnCopiarNota,     &QPushButton::clicked, this, &MainWindow::onCopiarAnotacoes);
    if (m_comboFonteNota)    connect(m_comboFonteNota,    &QComboBox::currentTextChanged, this, &MainWindow::onMudarFonteAnotacoes);
    if (m_spinTamanhoNota)   connect(m_spinTamanhoNota,   QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onMudarTamanhoAnotacoes);
    if (m_btnFonteMaisNota)  connect(m_btnFonteMaisNota,  &QPushButton::clicked, this, &MainWindow::onAumentarFonteAnotacoes);
    if (m_btnFonteMenosNota) connect(m_btnFonteMenosNota, &QPushButton::clicked, this, &MainWindow::onDiminuirFonteAnotacoes);
    if (m_btnBoldNota)       connect(m_btnBoldNota,       &QPushButton::clicked, this, &MainWindow::onToggleBoldAnotacoes);
    if (m_btnItalicNota)     connect(m_btnItalicNota,     &QPushButton::clicked, this, &MainWindow::onToggleItalicAnotacoes);
    if (m_btnUnderlineNota)  connect(m_btnUnderlineNota,  &QPushButton::clicked, this, &MainWindow::onToggleUnderlineAnotacoes);
    if (m_btnCorNota)        connect(m_btnCorNota,        &QPushButton::clicked, this, &MainWindow::onEscolherCorAnotacoes);
    if (m_txtAnotacoes)      connect(m_txtAnotacoes,      &QTextEdit::textChanged, this, &MainWindow::onAtualizarContadorNotas);

    connect(m_pdfView->pageNavigator(), &QPdfPageNavigator::currentPageChanged,
            this, [this](int pag) {
        if (m_bloquearSyncPagina) {
            return;
        }
        if (m_modoVis != ModoVisualizacao::Continuo) {
            m_paginaAtual = pag;
            atualizarInfoNavegacao();
        }
    });

    if (m_pdfView->verticalScrollBar()) {
        connect(m_pdfView->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
            if (m_bloquearSyncPagina || m_modoVis != ModoVisualizacao::Continuo || !m_pdfDoc || m_pdfDoc->pageCount() == 0) {
                return;
            }
            const int pagReal = obterPaginaVisivelNoModoContinuo();
            if (pagReal != m_paginaAtual) {
                m_paginaAtual = pagReal;
                atualizarInfoNavegacao();
            }
        });
    }

    // Proteção de salto indevido de página durante zoom (evita oscilação com roda do mouse ou botões)
    m_timerZoomDebounce = new QTimer(this);
    m_timerZoomDebounce->setSingleShot(true);
    connect(m_timerZoomDebounce, &QTimer::timeout, this, [this]() {
        if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) {
            m_bloquearSyncPagina = false;
            return;
        }
        const int pagFoco = qBound(0, m_paginaAtual, m_pdfDoc->pageCount() - 1);
        if (m_modoVis == ModoVisualizacao::Continuo) {
            const int y = calcularScrollVerticalParaPagina(pagFoco, m_pdfView->zoomFactor());
            if (m_pdfView->verticalScrollBar()) {
                m_pdfView->verticalScrollBar()->setValue(y);
            }
        }
        m_pdfView->pageNavigator()->jump(pagFoco, {}, 0);
        m_paginaAtual = pagFoco;
        atualizarInfoNavegacao();
        m_bloquearSyncPagina = false;
    });

    // Proteção de salto indevido de página durante redimensionamento do painel/splitter/janela/tela cheia
    m_timerDebounceResize = new QTimer(this);
    m_timerDebounceResize->setSingleShot(true);
    connect(m_timerDebounceResize, &QTimer::timeout, this, [this]() {
        if (m_pdfDoc && m_pdfDoc->pageCount() > 0 && m_paginaSalvaResize >= 0) {
            const int total = m_pdfDoc->pageCount();
            const int pagFoco = qBound(0, m_paginaSalvaResize, total - 1);
            if (m_modoVis == ModoVisualizacao::Continuo) {
                const int yBase = calcularScrollVerticalParaPagina(pagFoco, m_pdfView->zoomFactor());
                const int yFinal = yBase + m_scrollOffsetNaPagina;
                if (m_pdfView->verticalScrollBar()) {
                    m_pdfView->verticalScrollBar()->setValue(yFinal);
                }
            }
            m_pdfView->pageNavigator()->jump(pagFoco, QPointF(0, 0), 0);
            m_paginaAtual = pagFoco;
            atualizarInfoNavegacao();
        }
        m_bloquearSyncPagina = false;
    });

    connect(m_splitter, &QSplitter::splitterMoved, this, [this](int, int) {
        if (!m_bloquearSyncPagina) {
            const int pagReal = (m_modoVis == ModoVisualizacao::Continuo)
                                ? obterPaginaVisivelNoModoContinuo()
                                : m_paginaAtual;
            m_paginaSalvaResize = pagReal;
            if (m_modoVis == ModoVisualizacao::Continuo && m_pdfView && m_pdfView->verticalScrollBar()) {
                const int yBase = calcularScrollVerticalParaPagina(pagReal, m_pdfView->zoomFactor());
                m_scrollOffsetNaPagina = qMax(0, m_pdfView->verticalScrollBar()->value() - yBase);
            } else {
                m_scrollOffsetNaPagina = 0;
            }
            m_bloquearSyncPagina = true;
        }
        if (m_timerDebounceResize) {
            m_timerDebounceResize->start(200);
        }
    });
}

// ═════════════════════════════════════════════════════════════════════════════
// Persistência de Layout (QSettings)
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::carregarConfiguracoes()
{
    QSettings settings("Vellum", "LeitorTecnico");
    if (!settings.contains("geometry")) {
        QSettings oldSettings("EnsinadorDeIngles", "LeitorTecnico");
        if (oldSettings.contains("geometry") || oldSettings.contains("iaApiKey")) {
            for (const QString &k : oldSettings.allKeys()) {
                settings.setValue(k, oldSettings.value(k));
            }
        }
    }
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

    const int velIndex = settings.value("velIndex", 1).toInt();
    if (velIndex >= 0 && velIndex < m_comboVelocidade->count()) {
        m_comboVelocidade->setCurrentIndex(velIndex);
    }

    const int nivelIndex = settings.value("nivelIndex", 1).toInt();
    if (nivelIndex >= 0 && nivelIndex < m_comboNivelTutor->count()) {
        m_comboNivelTutor->setCurrentIndex(nivelIndex);
    }

    m_iaProvedor = settings.value("iaProvedor", "gemini").toString();
    m_iaApiKey   = settings.value("iaApiKey", "").toString();
    m_iaModelo   = settings.value("iaModelo", "gemini-3.5-flash-lite").toString();
    // Migrar modelos obsoletos ou descontinuados para o atual estável e ultra rápido
    static const QSet<QString> obsoletos = {
        "gemini-1.5-flash", "gemini-2.0-flash", "gemini-1.5-pro", "gemini-2.5-flash", "gemini-3.8-flash-lite"
    };
    if (m_iaModelo.isEmpty() || obsoletos.contains(m_iaModelo)) {
        m_iaModelo = "gemini-3.5-flash-lite";
    }

    // Tier de hardware (Padrão: 0 - Nuvem / Gemini para funcionamento instantâneo)
    const int tierIdx = settings.value("tierIndex", 0).toInt();
    if (m_comboPerfil && tierIdx >= 0 && tierIdx < m_comboPerfil->count())
        m_comboPerfil->setCurrentIndex(tierIdx);
    m_ollamaModelTier = settings.value("ollamaModelTier", "llama3").toString();
    m_whisperModelTier = settings.value("whisperModelTier", "base.en").toString();

    // Seletor de IA no chat
    if (m_comboChatIA) {
        const int iaIdx = m_comboChatIA->findData(m_iaProvedor);
        if (iaIdx >= 0) m_comboChatIA->setCurrentIndex(iaIdx);
    }

    if (m_iaProvedor == "gemini") {
        if (m_iaModelo.isEmpty() || m_iaModelo.contains("llama") || m_iaModelo.contains("phi")) {
            m_iaModelo = "gemini-3.5-flash-lite";
        }
    }

    if (m_lblModeloAtivoChat) {
        if (m_iaProvedor == "gemini") {
            m_lblModeloAtivoChat->setText(m_iaModelo);
        } else {
            m_lblModeloAtivoChat->setText(
                m_ollamaModelTier.isEmpty() ? "llama3" : m_ollamaModelTier
            );
        }
    }

    atualizarBadgeModeloAtivo();
}

void MainWindow::salvarConfiguracoes()
{
    QSettings settings("Vellum", "LeitorTecnico");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("splitter", m_splitter->saveState());
    settings.setValue("painelVisivel", m_painelDir->isVisible());
    settings.setValue("abaAtual", m_tabWidget->currentIndex());
    settings.setValue("vozIndex", m_comboVoz->currentIndex());
    settings.setValue("velIndex", m_comboVelocidade->currentIndex());
    settings.setValue("nivelIndex", m_comboNivelTutor->currentIndex());
    settings.setValue("iaProvedor", m_iaProvedor);
    settings.setValue("iaApiKey", m_iaApiKey);
    settings.setValue("iaModelo", m_iaModelo);
    settings.setValue("tierIndex",        m_comboPerfil ? m_comboPerfil->currentIndex() : 0);
    settings.setValue("ollamaModelTier",  m_ollamaModelTier);
    settings.setValue("whisperModelTier", m_whisperModelTier);
}

void MainWindow::onSalvarLayout()
{
    salvarConfiguracoes();
    appendLog("Layout atual salvo com sucesso nas preferências.", "success");
}

void MainWindow::onRestaurarLayoutPadrao()
{
    const int pagSalva = m_paginaAtual;
    m_bloquearSyncPagina = true;

    m_painelDir->setVisible(true);
    m_actTogglePainel->setChecked(true);
    m_tabWidget->setCurrentIndex(0);

    const int totalWidth = width();
    m_splitter->setSizes({int(totalWidth * 0.72), int(totalWidth * 0.28)});
    salvarConfiguracoes();
    appendLog("Layout restaurado para o padrão ideal.", "info");

    QTimer::singleShot(80, this, [this, pagSalva]() {
        if (m_pdfDoc && m_pdfDoc->pageCount() > 0) {
            m_pdfView->pageNavigator()->jump(pagSalva, QPointF(0, 0), 0);
            m_paginaAtual = pagSalva;
            atualizarInfoNavegacao();
        }
        m_bloquearSyncPagina = false;
    });
}

void MainWindow::onAplicarProporcao(int leitorPercent)
{
    const int pagSalva = m_paginaAtual;
    m_bloquearSyncPagina = true;

    if (leitorPercent >= 100) {
        m_painelDir->setVisible(false);
        m_actTogglePainel->setChecked(false);
        appendLog("Modo foco total: leitor em 100% da janela.", "info");
    } else {
        m_painelDir->setVisible(true);
        m_actTogglePainel->setChecked(true);
        const int totalWidth = width();
        const int wLeitor = int(totalWidth * (leitorPercent / 100.0));
        const int wPainel = totalWidth - wLeitor;
        m_splitter->setSizes({wLeitor, wPainel});
    }

    QTimer::singleShot(80, this, [this, pagSalva]() {
        if (m_pdfDoc && m_pdfDoc->pageCount() > 0) {
            m_pdfView->pageNavigator()->jump(pagSalva, QPointF(0, 0), 0);
            m_paginaAtual = pagSalva;
            atualizarInfoNavegacao();
        }
        m_bloquearSyncPagina = false;
    });
}

void MainWindow::onTogglePainelLateral(bool visivel)
{
    const int pagSalva = m_paginaAtual;
    m_bloquearSyncPagina = true;

    m_painelDir->setVisible(visivel);
    m_actTogglePainel->setChecked(visivel);
    if (visivel) {
        const int totalWidth = width();
        if (m_splitter->sizes().value(1, 0) < 100) {
            m_splitter->setSizes({int(totalWidth * 0.72), int(totalWidth * 0.28)});
        }
    }

    QTimer::singleShot(80, this, [this, pagSalva]() {
        if (m_pdfDoc && m_pdfDoc->pageCount() > 0) {
            m_pdfView->pageNavigator()->jump(pagSalva, QPointF(0, 0), 0);
            m_paginaAtual = pagSalva;
            atualizarInfoNavegacao();
        }
        m_bloquearSyncPagina = false;
    });
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

void MainWindow::carregarArquivoPdf(const QString &caminho)
{
    if (caminho.isEmpty()) return;

    m_pdfDoc->close();
    m_pdfDoc->load(caminho);

    if (m_pdfDoc->status() != QPdfDocument::Status::Ready) {
        QMessageBox::critical(this, "Erro", "Não foi possível abrir o arquivo PDF selecionado.");
        atualizarVisibilidadeEmptyState();
        atualizarInfoNavegacao();
        return;
    }

    const int total = m_pdfDoc->pageCount();
    irParaPagina(0);
    atualizarVisibilidadeEmptyState();
    atualizarInfoNavegacao();
    setWindowTitle(QString("%1 — Vellum").arg(QFileInfo(caminho).fileName()));
    appendLog(QString("Documento carregado: %1 (%2 páginas)").arg(QFileInfo(caminho).fileName()).arg(total), "success");
}

void MainWindow::onAbrirPdf()
{
    const QString caminho = QFileDialog::getOpenFileName(
        this, "Selecionar Livro ou Documento PDF", QString(), "Documentos PDF (*.pdf);;Todos os Arquivos (*.*)");
    if (!caminho.isEmpty()) {
        carregarArquivoPdf(caminho);
    }
}

void MainWindow::onFecharPdf()
{
    m_pdfDoc->close();
    m_paginaAtual = 0;
    atualizarVisibilidadeEmptyState();
    atualizarInfoNavegacao();
    setWindowTitle("Vellum — Leitor Técnico & Tutor");
    appendLog("Documento fechado.");
}

void MainWindow::atualizarVisibilidadeEmptyState()
{
    const bool temDoc = (m_pdfDoc && m_pdfDoc->pageCount() > 0);
    if (m_emptyStateWidget) {
        m_emptyStateWidget->setVisible(!temDoc);
    }
    if (m_pdfView) {
        m_pdfView->setVisible(temDoc);
    }
    if (m_pdfView2) {
        m_pdfView2->setVisible(temDoc && m_modoVis == ModoVisualizacao::DuasPaginas);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.toLocalFile().endsWith(".pdf", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            const QString localFile = url.toLocalFile();
            if (localFile.endsWith(".pdf", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                carregarArquivoPdf(localFile);
                return;
            }
        }
    }
}

void MainWindow::onToggleModoCaptura(bool ativo)
{
    m_modoCaptura = ativo;
    m_btnModoCaptura->setChecked(ativo);
    if (ativo) {
        m_tipoCaptura = ModoCapturaRubberBand::Traducao;
    } else {
        if (m_rubberBand && m_rubberBand->isVisible()) {
            m_rubberBand->hide();
        }
        m_rbOrigin = QPoint();
        m_tipoCaptura = ModoCapturaRubberBand::Nenhum;
    }

    const auto cursor = ativo ? Qt::CrossCursor : Qt::OpenHandCursor;
    m_pdfView->viewport()->setCursor(cursor);
    m_pdfView2->viewport()->setCursor(cursor);

    if (ativo) {
        appendLog("Modo de seleção ativado: arraste sobre o parágrafo (Esc para cancelar).");
    }
}

void MainWindow::onTraduzirDireto()
{
    const int tierIdx = m_comboPerfil ? m_comboPerfil->currentIndex() : 0;
    QString geminiKey;
    QString modeloOllama;

    if (tierIdx == 0) {
        // Tier 0: Nuvem (Gemini)
        geminiKey = m_iaApiKey.trimmed();
        modeloOllama = m_iaModelo.trimmed();
    } else if (tierIdx == 1) {
        // Tier 1: Básico (phi3:mini local)
        geminiKey = QString();
        modeloOllama = "phi3:mini";
    } else if (tierIdx == 2) {
        // Tier 2: Médio (llama3.2:3b local)
        geminiKey = QString();
        modeloOllama = "llama3.2:3b";
    } else {
        // Tier 3: Avançado (llama3 local)
        geminiKey = QString();
        modeloOllama = "llama3";
    }

    if (m_textoOcrAtual.startsWith("BASE64:")) {
        const QByteArray b64 = m_textoOcrAtual.mid(7).toLatin1();
        m_net->traduzirDireto(QString(), b64, geminiKey, modeloOllama);
    } else if (!m_textoOcrAtual.isEmpty()) {
        m_net->traduzirDireto(m_textoOcrAtual, QByteArray(), geminiKey, modeloOllama);
    }
}

void MainWindow::onTraducaoDiretaResultado(const QString &textoIngles, const QString &traducaoPortugues, const QString &modeloUsado)
{
    m_textoOriginalEn = textoIngles;
    m_textoTraduzidoPt = traducaoPortugues;
    
    if (m_promptTextoTutor) {
        m_promptTextoTutor->setText(textoIngles);
    }

    m_btnOuvir->setEnabled(!textoIngles.isEmpty());
    m_btnGravar->setEnabled(!textoIngles.isEmpty());

    // Se o painel estiver oculto, abre e foca na tradução
    if (!m_painelDir->isVisible()) {
        onTogglePainelLateral(true);
    }
    m_tabWidget->setCurrentIndex(0);

    const QString badgeTag = !modeloUsado.isEmpty()
        ? QString(" <span style='color: #10b981; font-size: 10px; font-weight: 600; background: #064e3b; padding: 2px 6px; border-radius: 4px;'>[%1]</span>")
            .arg(modeloUsado.toHtmlEscaped())
        : QString();

    m_promptTraducao->setHtml(
        "<div style='margin-bottom: 8px;'>"
        "<span style='color: #6b7280; font-size: 10px; font-weight: 600; text-transform: uppercase;'>Texto Original (EN)</span><br>"
        "<span style='color: #9ca3af; font-size: 12px; line-height: 1.4;'>" + textoIngles.toHtmlEscaped() + "</span>"
        "</div><hr style='border: 0; border-top: 1px solid #27282d; margin: 8px 0;'>"
        "<div>"
        "<span style='color: #60a5fa; font-size: 10px; font-weight: 600; text-transform: uppercase;'>Tradução Técnica (PT-BR)</span>" + badgeTag + "<br>"
        "<span style='color: #f3f4f6; font-size: 13px; font-weight: 500; line-height: 1.5;'>" + traducaoPortugues.toHtmlEscaped() + "</span>"
        "</div>"
    );

    const QString logMsg = !modeloUsado.isEmpty()
        ? QString("Tradução técnica concluída com sucesso via [%1].").arg(modeloUsado)
        : QStringLiteral("Tradução técnica concluída com sucesso.");
    appendLog(logMsg, "success");
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
    QString texto;
    if (m_promptTextoTutor) {
        texto = m_promptTextoTutor->selectedText();
        if (texto.isEmpty()) {
            texto = m_promptTextoTutor->text().trimmed();
        }
    } else {
        texto = m_textoOriginalEn.isEmpty()
            ? QApplication::clipboard()->text().trimmed()
            : m_textoOriginalEn;
    }

    if (texto.isEmpty()) {
        appendLog("Selecione um texto antes para ouvir a pronúncia.", "warning");
        return;
    }

    const QString voz = m_comboVoz->currentData().toString();
    const QString vel = m_comboVelocidade->currentData().toString();
    appendLog(QString("Reproduzindo pronúncia neural (%1, velocidade %2)...").arg(voz, m_comboVelocidade->currentText()), "info");
    m_net->falarTexto(texto, voz, vel);
}

void MainWindow::onPararAudio()
{
    m_net->pararAudio();
    appendLog("Reprodução de áudio interrompida pelo usuário.", "info");
}

void MainWindow::onFalaIniciada(const QString &voz)
{
    appendLog(QString("Reprodução de áudio iniciada (%1).").arg(voz), "success");
}

void MainWindow::onGravarVozTutor()
{
    if (!m_gravando) {
        QString textoParaLer;
        if (m_promptTextoTutor) {
            textoParaLer = m_promptTextoTutor->selectedText();
            if (textoParaLer.isEmpty()) {
                textoParaLer = m_promptTextoTutor->text().trimmed();
            }
        }
        
        if (textoParaLer.isEmpty()) {
            textoParaLer = m_textoOriginalEn;
        }

        if (textoParaLer.isEmpty()) {
            textoParaLer = QApplication::clipboard()->text().trimmed();
        }

        if (textoParaLer.isEmpty()) {
            appendLog("Selecione ou digite um parágrafo para praticar a leitura.", "warning");
            return;
        }
        
        m_textoOriginalEn = textoParaLer; // Salva o texto que será usado para a avaliação

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

    // Envia automaticamente para avaliação fonética do tutor com o nível de exigência escolhido
    if (!m_textoOriginalEn.isEmpty()) {
        const QString nivel = m_comboNivelTutor->currentData().toString();
        const int tierIdx = m_comboPerfil ? m_comboPerfil->currentIndex() : 0;
        QString modelo;
        QString apiKey;

        if (tierIdx == 0) {
            apiKey = m_iaApiKey.trimmed();
            modelo = "Gemini";
        } else if (tierIdx == 1) {
            modelo = "phi3:mini";
        } else if (tierIdx == 2) {
            modelo = "llama3.2:3b";
        } else {
            modelo = "llama3";
        }

        appendLog(QString("Avaliando precisão da pronúncia (Nível: %1, Modelo: %2)...").arg(m_comboNivelTutor->currentText(), modelo), "info");
        m_net->avaliarPronuncia(m_textoOriginalEn, texto, nivel, modelo, apiKey);
    }
}

void MainWindow::onAvaliacaoPronunciaResultado(int nota, const QString &feedback, const QString &textoFalado, const QStringList &palavrasAusentes)
{
    m_barAcuracia->setValue(nota);

    QString html = QString(
        "<div style='margin-bottom: 8px;'>"
        "<span style='color: #60a5fa; font-weight: 600; font-size: 13px;'>Nota de Correspondência: %1%</span>"
        "</div>"
        "<div style='background-color: #1e222a; border-left: 3px solid #3b82f6; padding: 6px 8px; border-radius: 4px; margin-bottom: 8px;'>"
        "<span style='color: #9ca3af; font-size: 10px; font-weight: 600; text-transform: uppercase;'>O que o Tutor ouviu da sua fala:</span><br>"
        "<span style='color: #e5e7eb; font-size: 12px; font-weight: 500;'>\"%2\"</span>"
        "</div>"
        "<div style='margin-bottom: 8px; color: #d1d5db; font-size: 12px; line-height: 1.4;'>"
        "<b>Dica do Tutor:</b><br>%3"
        "</div>"
    ).arg(nota).arg(textoFalado.toHtmlEscaped()).arg(feedback.toHtmlEscaped());

    if (!palavrasAusentes.isEmpty()) {
        html += "<div style='background-color: #26201a; border-left: 3px solid #f59e0b; padding: 5px 8px; border-radius: 4px; color: #fbbf24; font-size: 11px; margin-top: 6px;'>"
                "<b>Atenção nestes termos:</b><br>" + palavrasAusentes.join(", ").toHtmlEscaped() +
                "</div>";
    }

    m_txtFeedbackTutor->setHtml(html);
    appendLog(QString("Avaliação concluída: %1% de acurácia.").arg(nota), "success");
}

void MainWindow::onPerfilAlterado(int index)
{
    // Tier 0 = Nuvem: toda IA vai pela chave API configurada, sem Ollama
    // Tier 1 = Basico, Tier 2 = Medio, Tier 3 = Avancado
    struct Tier { const char *ollama; const char *whisper; const char *descricao; };
    static const Tier tiers[] = {
        { "",            "base.en",  "Nuvem (sem Ollama, usa API cadastrada)" },
        { "phi3:mini",   "tiny.en",  "Basico (phi3:mini + tiny.en)" },
        { "llama3.2:3b", "base.en",  "Medio (llama3.2:3b + base.en)" },
        { "llama3",      "base.en",  "Avancado (llama3 + base.en)" },
    };
    const int idx = qBound(0, index, 3);
    m_ollamaModelTier  = tiers[idx].ollama;   // vazio no tier Nuvem
    m_whisperModelTier = tiers[idx].whisper;

    QSettings settings("Vellum", "LeitorTecnico");
    settings.setValue("tierIndex",        idx);
    settings.setValue("ollamaModelTier",  m_ollamaModelTier);
    settings.setValue("whisperModelTier", m_whisperModelTier);

    const QString aviso = (idx == 0)
        ? QStringLiteral(" — Certifique-se de ter a chave API configurada.")
        : QStringLiteral(" Reinicie o backend para ter efeito.");
    appendLog(QString("Perfil alterado para: %1.%2").arg(tiers[idx].descricao, aviso), "info");

    if (m_lblModeloAtivoChat && m_iaProvedor == "ollama") {
        m_lblModeloAtivoChat->setText(m_ollamaModelTier.isEmpty() ? "llama3" : m_ollamaModelTier);
    }
    atualizarBadgeModeloAtivo();
}

void MainWindow::atualizarBadgeModeloAtivo()
{
    if (!m_badgeModeloAtivo) return;

    QString modeloNome;
    QString whisperNome;
    const int tierIdx = m_comboPerfil ? m_comboPerfil->currentIndex() : 0;

    switch (tierIdx) {
    case 0:
        if (!m_iaApiKey.trimmed().isEmpty()) {
            modeloNome = "Gemini (Nuvem)";
        } else {
            modeloNome = "Nuvem (Chave necessária)";
        }
        whisperNome = "base.en";
        break;
    case 1:
        modeloNome = "phi3:mini (Local)";
        whisperNome = "tiny.en";
        break;
    case 2:
        modeloNome = "llama3.2:3b (Local)";
        whisperNome = "base.en";
        break;
    case 3:
    default:
        modeloNome = "llama3 (Local)";
        whisperNome = "base.en";
        break;
    }

    m_badgeModeloAtivo->setText(QString("Tradução: %1").arg(modeloNome));
    m_badgeModeloAtivo->setToolTip(
        QString("Motor ativo para tradução técnica e OCR:\n• Tradução: %1\n• Reconhecimento de Voz: %2\n(Altere a qualquer momento na aba Desempenho & Layout ou Configurar API)")
            .arg(modeloNome, whisperNome)
    );

    // No modo local/offline, modelos de texto não suportam visão computacional.
    // As opções de capturar circuito ou anexar imagem só aparecem no modo Nuvem (Gemini).
    const bool isNuvem = (tierIdx == 0);
    if (m_promptInput) {
        m_promptInput->setButtonsVisible(isNuvem, isNuvem);
    }
    if (!isNuvem && m_promptInput) {
        m_promptInput->hideImagePreview();
        m_chatImagemBase64.clear();
    }
    if (m_promptInput) {
        m_promptInput->setPlaceholderText(
            isNuvem ? "Pergunte sobre o circuito ou texto técnico... (Enter para enviar, Shift+Enter para nova linha)"
                    : "Pergunte sobre o texto técnico... (Enter para enviar, Shift+Enter para nova linha)"
        );
    }
}

void MainWindow::onToggleTelaCheia()
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) {
        if (!isFullScreen()) showFullScreen(); else showMaximized();
        return;
    }

    const int pagReal = (m_modoVis == ModoVisualizacao::Continuo) 
                        ? obterPaginaVisivelNoModoContinuo() 
                        : m_paginaAtual;
    m_paginaSalvaResize = pagReal;

    if (m_modoVis == ModoVisualizacao::Continuo && m_pdfView && m_pdfView->verticalScrollBar()) {
        const int yBase = calcularScrollVerticalParaPagina(pagReal, m_pdfView->zoomFactor());
        m_scrollOffsetNaPagina = qMax(0, m_pdfView->verticalScrollBar()->value() - yBase);
    } else {
        m_scrollOffsetNaPagina = 0;
    }

    m_bloquearSyncPagina = true;

    const bool paraTelaCheia = !isFullScreen();
    if (paraTelaCheia) {
        m_barraVisPreviaVisivel = m_barraVis ? m_barraVis->isVisible() : true;
        menuBar()->setVisible(false);
        if (m_barraVis) m_barraVis->setVisible(false);
        showFullScreen();
        appendLog("Modo Tela Cheia ativado: barras superiores ocultadas. Pressione F11 para restaurar.", "info");
    } else {
        showMaximized();
        menuBar()->setVisible(true);
        if (m_barraVis) m_barraVis->setVisible(m_barraVisPreviaVisivel);
        appendLog("Modo Tela Cheia desativado: barras superiores restauradas.", "info");
    }

    if (m_timerDebounceResize) {
        m_timerDebounceResize->stop();
        m_timerDebounceResize->start(350);
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if (!m_bloquearSyncPagina && m_pdfDoc && m_pdfDoc->pageCount() > 0) {
        const int pagReal = (m_modoVis == ModoVisualizacao::Continuo) 
                            ? obterPaginaVisivelNoModoContinuo() 
                            : m_paginaAtual;
        m_paginaSalvaResize = pagReal;
        if (m_modoVis == ModoVisualizacao::Continuo && m_pdfView && m_pdfView->verticalScrollBar()) {
            const int yBase = calcularScrollVerticalParaPagina(pagReal, m_pdfView->zoomFactor());
            m_scrollOffsetNaPagina = qMax(0, m_pdfView->verticalScrollBar()->value() - yBase);
        } else {
            m_scrollOffsetNaPagina = 0;
        }
        m_bloquearSyncPagina = true;
    }
    QMainWindow::resizeEvent(event);
    if (m_timerDebounceResize) {
        m_timerDebounceResize->start(300);
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Navegação de páginas com setas do teclado nos modos individual e lado a lado
    if (m_modoVis == ModoVisualizacao::UmaPagina || m_modoVis == ModoVisualizacao::DuasPaginas) {
        QWidget *focused = focusWidget();
        bool isInput = false;
        if (auto *le = qobject_cast<QLineEdit*>(focused)) isInput = !le->isReadOnly();
        else if (auto *te = qobject_cast<QTextEdit*>(focused)) isInput = !te->isReadOnly();
        else if (auto *pe = qobject_cast<QPlainTextEdit*>(focused)) isInput = !pe->isReadOnly();
        else if (qobject_cast<QAbstractSpinBox*>(focused)) isInput = true;

        if (!isInput) {
            if (event->key() == Qt::Key_Left || event->key() == Qt::Key_PageUp) {
                onPaginaAnterior();
                event->accept();
                return;
            } else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_PageDown) {
                onPaginaProxima();
                event->accept();
                return;
            }
        }
    }

    if (event->key() == Qt::Key_Escape) {
        if (m_rubberBand && m_rubberBand->isVisible()) {
            m_rubberBand->hide();
            m_rbOrigin = QPoint();
            appendLog("Desenho da seleção cancelado. Selecione novamente quando desejar.", "info");
            event->accept();
            return;
        }
        if (m_modoCaptura) {
            onToggleModoCaptura(false);
            m_tipoCaptura = ModoCapturaRubberBand::Nenhum;
            appendLog("Modo de seleção desativado.", "info");
            event->accept();
            return;
        }
        if (isFullScreen()) {
            onToggleTelaCheia();
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

// ═════════════════════════════════════════════════════════════════════════════
// Manipulação de Eventos do Mouse (Seleção Rubber Band)
// ═════════════════════════════════════════════════════════════════════════════

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Eventos do m_chatInput removidos pois o PromptInputWidget gerencia isso.
    QPdfView *view = nullptr;
    if (watched == m_pdfView || watched == m_pdfView->viewport())   view = m_pdfView;
    if (watched == m_pdfView2 || watched == m_pdfView2->viewport()) view = m_pdfView2;

    if (!view) return QMainWindow::eventFilter(watched, event);

    m_activePdfView = view;

    switch (event->type()) {
    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent*>(event);
        if (m_modoVis == ModoVisualizacao::UmaPagina || m_modoVis == ModoVisualizacao::DuasPaginas) {
            if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_PageUp) {
                onPaginaAnterior();
                return true;
            } else if (ke->key() == Qt::Key_Right || ke->key() == Qt::Key_PageDown) {
                onPaginaProxima();
                return true;
            }
        }

        if (ke->key() == Qt::Key_Escape) {
            if (m_rubberBand && m_rubberBand->isVisible()) {
                m_rubberBand->hide();
                m_rbOrigin = QPoint();
                appendLog("Desenho da seleção cancelado. Selecione novamente quando desejar.", "info");
                return true;
            }
            if (m_modoCaptura) {
                onToggleModoCaptura(false);
                m_tipoCaptura = ModoCapturaRubberBand::Nenhum;
                appendLog("Modo de seleção desativado.", "info");
                return true;
            }
            if (isFullScreen()) {
                onToggleTelaCheia();
                return true;
            }
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        auto *me = static_cast<QMouseEvent*>(event);
        // Modo de captura (rubber band)
        if (me->button() == Qt::LeftButton && m_modoCaptura) {
            m_rbOrigin = me->pos();
            m_rubberBand->setParent(view->viewport());
            m_rubberBand->setGeometry(QRect(m_rbOrigin, QSize()));
            m_rubberBand->show();
            return true;
        }
        // Modo pan (mao): clique e segura para arrastar
        if (me->button() == Qt::LeftButton && !m_modoCaptura) {
            m_panModo  = true;
            m_panOrigin = me->pos();
            view->viewport()->setCursor(Qt::ClosedHandCursor);
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto *me = static_cast<QMouseEvent*>(event);
        // Arrastar rubber band
        if (m_modoCaptura && m_rubberBand->isVisible()) {
            m_rubberBand->setGeometry(QRect(m_rbOrigin, me->pos()).normalized());
            return true;
        }
        // Arrastar pan
        if (m_panModo && (me->buttons() & Qt::LeftButton)) {
            const QPoint delta = me->pos() - m_panOrigin;
            m_panOrigin = me->pos();
            auto *hsb = view->horizontalScrollBar();
            auto *vsb = view->verticalScrollBar();
            if (hsb) hsb->setValue(hsb->value() - delta.x());
            if (vsb) vsb->setValue(vsb->value() - delta.y());
            return true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto *me = static_cast<QMouseEvent*>(event);
        // Soltar rubber band
        if (me->button() == Qt::LeftButton && m_modoCaptura && m_rubberBand->isVisible()) {
            const QRect rect = m_rubberBand->geometry();
            m_rubberBand->hide();
            const auto tipoCaptura = m_tipoCaptura;
            onToggleModoCaptura(false);

            if (rect.width() > 15 && rect.height() > 10) {
                const QPixmap pix = view->viewport()->grab(rect);
                QByteArray bytes;
                QBuffer buf(&bytes);
                buf.open(QIODevice::WriteOnly);
                pix.save(&buf, "PNG");

                if (tipoCaptura == ModoCapturaRubberBand::CircuitoChat) {
                    m_chatImagemBase64 = QString::fromLatin1(bytes.toBase64());
                    if (m_promptInput) {
                        m_promptInput->showImagePreview(m_chatImagemBase64, QString("Circuito capturado (%1x%2 px)").arg(rect.width()).arg(rect.height()));
                    }
                    onTogglePainelLateral(true);
                    m_tabWidget->setCurrentIndex(2); // Aba do Chat IA
                    if (m_promptInput) {
                        // m_promptInput->setFocus();
                    }
                    appendLog("Circuito capturado com sucesso e anexado ao Assistente IA.", "success");
                } else {
                    m_textoOcrAtual = "BASE64:" + bytes.toBase64();
                    m_btnTraduzir->setEnabled(true);

                    if (m_chkTraducaoAuto->isChecked()) {
                        onTraduzirDireto();
                    } else {
                        appendLog("Região capturada. Clique em 'Traduzir Seleção'.");
                    }
                }
            }
            m_tipoCaptura = ModoCapturaRubberBand::Nenhum;
            return true;
        }
        // Soltar pan
        if (me->button() == Qt::LeftButton && m_panModo) {
            m_panModo = false;
            view->viewport()->setCursor(Qt::OpenHandCursor);
            return true;
        }
        break;
    }
    case QEvent::Enter: {
        // Cursor de mao aberta quando nao esta capturando
        if (!m_modoCaptura)
            view->viewport()->setCursor(Qt::OpenHandCursor);
        break;
    }
    case QEvent::Leave: {
        if (!m_modoCaptura && !m_panModo)
            view->viewport()->setCursor(Qt::ArrowCursor);
        break;
    }
    case QEvent::Wheel: {
        auto *we = static_cast<QWheelEvent*>(event);
        if (we->modifiers() & Qt::ControlModifier) {
            const int delta = we->angleDelta().y();
            if (delta > 0) zoomDelta(0.12);
            else if (delta < 0) zoomDelta(-0.12);
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

int MainWindow::calcularScrollVerticalParaPagina(int pagina, qreal zoom) const
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) return 0;
    pagina = qBound(0, pagina, m_pdfDoc->pageCount() - 1);

    qreal y = m_pdfView ? m_pdfView->documentMargins().top() : 0;
    const int spacing = m_pdfView ? m_pdfView->pageSpacing() : 4;

    for (int i = 0; i < pagina; ++i) {
        const QSizeF sz = m_pdfDoc->pagePointSize(i);
        y += (sz.height() * zoom) + spacing;
    }
    return qRound(y);
}

int MainWindow::obterPaginaVisivelNoModoContinuo() const
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0 || !m_pdfView) return 0;
    if (m_modoVis != ModoVisualizacao::Continuo) return m_paginaAtual;

    const auto *vsb = m_pdfView->verticalScrollBar();
    if (!vsb) return m_paginaAtual;

    const int scrollY = vsb->value();
    qreal yAcumulado = m_pdfView->documentMargins().top();
    const int spacing = m_pdfView->pageSpacing();
    const qreal zoom = m_pdfView->zoomFactor();
    const int total = m_pdfDoc->pageCount();

    for (int i = 0; i < total; ++i) {
        const QSizeF sz = m_pdfDoc->pagePointSize(i);
        const qreal h = (sz.height() * zoom) + spacing;
        if (scrollY < yAcumulado + (h * 0.7)) {
            return i;
        }
        yAcumulado += h;
    }
    return total - 1;
}

void MainWindow::forcarNavegacaoPagina(int pagina)
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) return;
    const int total = m_pdfDoc->pageCount();
    pagina = qBound(0, pagina, total - 1);

    m_bloquearSyncPagina = true;
    m_paginaAtual = pagina;

    if (m_modoVis == ModoVisualizacao::Continuo) {
        const int y = calcularScrollVerticalParaPagina(pagina, m_pdfView->zoomFactor());
        if (m_pdfView->verticalScrollBar()) {
            m_pdfView->verticalScrollBar()->setValue(y);
        }
    }
    m_pdfView->pageNavigator()->jump(pagina, {}, 0);

    if (m_modoVis == ModoVisualizacao::DuasPaginas) {
        if (pagina + 1 < total) {
            m_pdfView2->pageNavigator()->jump(pagina + 1, {}, 0);
            m_pdfView2->setVisible(true);
        } else {
            m_pdfView2->setVisible(false);
        }
    } else {
        m_pdfView2->setVisible(false);
    }

    atualizarInfoNavegacao();

    // Mantém o bloqueio ativo durante o ciclo de layout do Qt para evitar eventos espúrios
    QTimer::singleShot(150, this, [this, pagina]() {
        if (m_paginaAtual == pagina) {
            m_bloquearSyncPagina = false;
        }
    });
}

void MainWindow::irParaPagina(int pagina)
{
    forcarNavegacaoPagina(pagina);
}

void MainWindow::setModoVisualizacao(ModoVisualizacao modo)
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) return;

    // Obtém com máxima precisão a página ativa real
    int pagAlvo = m_paginaAtual;
    if (m_pdfView && m_pdfView->pageNavigator()) {
        const int pNav = m_pdfView->pageNavigator()->currentPage();
        if (pNav >= 0 && pNav < m_pdfDoc->pageCount()) {
            pagAlvo = pNav;
        }
    }
    pagAlvo = qBound(0, pagAlvo, m_pdfDoc->pageCount() - 1);

    m_modoVis = modo;
    m_bloquearSyncPagina = true;
    m_paginaAtual = pagAlvo;

    m_btnModo1Pag->setChecked(modo == ModoVisualizacao::UmaPagina);
    m_btnModo2Pag->setChecked(modo == ModoVisualizacao::DuasPaginas);
    m_btnModoCont->setChecked(modo == ModoVisualizacao::Continuo);

    switch (modo) {
    case ModoVisualizacao::UmaPagina:
        m_pdfView->setPageMode(QPdfView::PageMode::SinglePage);
        m_pdfView2->setVisible(false);
        break;
    case ModoVisualizacao::DuasPaginas:
        m_pdfView->setPageMode(QPdfView::PageMode::SinglePage);
        break;
    case ModoVisualizacao::Continuo:
        m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
        m_pdfView2->setVisible(false);
        break;
    }

    m_paginaAtual = pagAlvo;
    atualizarInfoNavegacao();

    auto executarAncoragem = [this, pagAlvo]() {
        if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) return;
        if (m_modoVis == ModoVisualizacao::Continuo) {
            const int y = calcularScrollVerticalParaPagina(pagAlvo, m_pdfView->zoomFactor());
            if (m_pdfView->verticalScrollBar()) {
                m_pdfView->verticalScrollBar()->setValue(y);
            }
        }
        m_pdfView->pageNavigator()->jump(pagAlvo, {}, 0);

        if (m_modoVis == ModoVisualizacao::DuasPaginas) {
            if (pagAlvo + 1 < m_pdfDoc->pageCount()) {
                m_pdfView2->pageNavigator()->jump(pagAlvo + 1, {}, 0);
                m_pdfView2->setVisible(true);
            } else {
                m_pdfView2->setVisible(false);
            }
        } else {
            m_pdfView2->setVisible(false);
        }
        m_paginaAtual = pagAlvo;
        atualizarInfoNavegacao();
    };

    // Ancoragem sequencial durante a reconstrução assíncrona do layout MultiPage pelo Qt
    executarAncoragem();
    QTimer::singleShot(60,  this, executarAncoragem);
    QTimer::singleShot(180, this, executarAncoragem);
    QTimer::singleShot(350, this, executarAncoragem);
    QTimer::singleShot(600, this, [this, executarAncoragem]() {
        executarAncoragem();
        m_bloquearSyncPagina = false;
    });
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
    const bool temDoc = (total > 0);
    m_btnPagAnterior->setEnabled(temDoc && m_paginaAtual > 0);
    m_btnPagProxima->setEnabled(temDoc && m_paginaAtual < total - 1);
    m_spinPagina->setEnabled(temDoc);

    m_spinPagina->blockSignals(true);
    if (temDoc) {
        m_spinPagina->setRange(1, total);
        m_spinPagina->setValue(m_paginaAtual + 1);
        if (m_lblTotalPag) {
            m_lblTotalPag->setText(QString("/ %1").arg(total));
        }
    } else {
        m_spinPagina->setRange(0, 0);
        m_spinPagina->setValue(0);
        if (m_lblTotalPag) {
            m_lblTotalPag->setText("/ 0");
        }
    }
    m_spinPagina->blockSignals(false);
}

void MainWindow::aplicarZoom(qreal fator)
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) return;

    // 1. Identifica a página onde o usuário está lendo no exato momento
    int pagFoco = m_paginaAtual;
    if (m_pdfView && m_pdfView->pageNavigator()) {
        const int pNav = m_pdfView->pageNavigator()->currentPage();
        if (pNav >= 0 && pNav < m_pdfDoc->pageCount()) {
            pagFoco = pNav;
        }
    }
    pagFoco = qBound(0, pagFoco, m_pdfDoc->pageCount() - 1);
    m_bloquearSyncPagina = true;

    fator = qBound(0.5, fator, 4.0);

    m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
    m_pdfView->setZoomFactor(fator);

    if (m_pdfView2->isVisible()) {
        m_pdfView2->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView2->setZoomFactor(fator);
    }
    atualizarLabelZoom();

    // 2. Trava e ancora diretamente e imediatamente EM CIMA da página atual
    if (m_modoVis == ModoVisualizacao::Continuo) {
        const int y = calcularScrollVerticalParaPagina(pagFoco, fator);
        if (m_pdfView->verticalScrollBar()) {
            m_pdfView->verticalScrollBar()->setValue(y);
        }
    }
    m_pdfView->pageNavigator()->jump(pagFoco, {}, 0);
    m_paginaAtual = pagFoco;
    atualizarInfoNavegacao();

    if (m_modoVis != ModoVisualizacao::Continuo) {
        m_bloquearSyncPagina = false;
        return;
    }

    // 3. Em modo contínuo, reinicia o temporizador de debounce único:
    // Mantém o bloqueio ativo durante múltiplos passos rápidos de zoom (botão ou roda do mouse)
    // e reancora na página ao finalizar a operação
    if (m_timerZoomDebounce) {
        m_timerZoomDebounce->stop();
        m_timerZoomDebounce->start(200);
    }
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
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) return;
    const int pagFoco = qBound(0, m_paginaAtual, m_pdfDoc->pageCount() - 1);

    const int viewW = m_pdfView->viewport()->width();
    const QSizeF tamPag = m_pdfDoc->pagePointSize(pagFoco);
    qreal fator = 1.0;
    if (tamPag.width() > 0) {
        // Desconta scrollbar vertical e margem lateral (~24px)
        const qreal larguraUtil = qMax(100.0, (qreal)viewW - 24.0);
        fator = qBound(0.5, larguraUtil / tamPag.width(), 4.0);
    }

    aplicarZoom(fator);
}

void MainWindow::onAjustarPagina()
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) return;
    const int pagFoco = qBound(0, m_paginaAtual, m_pdfDoc->pageCount() - 1);

    const int viewW = m_pdfView->viewport()->width();
    const int viewH = m_pdfView->viewport()->height();
    const QSizeF tamPag = m_pdfDoc->pagePointSize(pagFoco);
    qreal fator = 1.0;
    if (tamPag.width() > 0 && tamPag.height() > 0) {
        const qreal fW = qMax(100.0, (qreal)viewW - 24.0) / tamPag.width();
        const qreal fH = qMax(100.0, (qreal)viewH - 24.0) / tamPag.height();
        fator = qBound(0.5, qMin(fW, fH), 4.0);
    }

    aplicarZoom(fator);
}

void MainWindow::atualizarLabelZoom()
{
    const int perc = qRound(m_pdfView->zoomFactor() * 100);
    m_btnZoomReset->setText(QString("%1%").arg(perc));
}

// ═════════════════════════════════════════════════════════════════════════════
// Respostas de Rede e Registro
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::iniciarProgresso(const QString &descricao, int duracaoEstimadaMs)
{
    m_operacaoAtual = descricao;
    m_tempoDecorridoMs = 0;
    m_duracaoEstimadaMs = qMax(600, duracaoEstimadaMs);
    m_progressoPercentual = 5;

    const QString texto = QString("%1: %2%").arg(m_operacaoAtual).arg(m_progressoPercentual);
    if (m_lblProgressoNumerico) {
        m_lblProgressoNumerico->setText(texto);
    }
    if (m_lblProgressoTopo) {
        m_lblProgressoTopo->setText(QString("%1%").arg(m_progressoPercentual));
        m_lblProgressoTopo->setVisible(true);
    }
    if (m_timerProgresso) {
        m_timerProgresso->start(40);
    }
}

void MainWindow::atualizarProgressoPasso()
{
    m_tempoDecorridoMs += 40;
    double t = double(m_tempoDecorridoMs) / double(m_duracaoEstimadaMs);
    if (t > 1.0) t = 1.0 + (t - 1.0) * 0.15;

    // Curva assintótica que desacelera perto de 96% aguardando a resposta real
    int pct = qBound(5, int((1.0 - std::exp(-2.5 * t)) * 105.0), 96);
    m_progressoPercentual = pct;

    const QString texto = QString("%1: %2%").arg(m_operacaoAtual).arg(m_progressoPercentual);
    if (m_lblProgressoNumerico) {
        m_lblProgressoNumerico->setText(texto);
    }
    if (m_lblProgressoTopo) {
        m_lblProgressoTopo->setText(QString("%1%").arg(m_progressoPercentual));
    }
}

void MainWindow::finalizarProgresso(bool sucesso)
{
    if (m_timerProgresso) {
        m_timerProgresso->stop();
    }
    m_progressoPercentual = sucesso ? 100 : m_progressoPercentual;

    if (m_lblProgressoNumerico) {
        if (sucesso) {
            m_lblProgressoNumerico->setText(QString("%1: 100%").arg(m_operacaoAtual));
        } else {
            m_lblProgressoNumerico->setText(QString("%1: Falha").arg(m_operacaoAtual));
        }
    }
    if (m_lblProgressoTopo) {
        m_lblProgressoTopo->setText(sucesso ? "100%" : "Erro");
    }

    QTimer::singleShot(sucesso ? 1200 : 2500, this, [this]() {
        if (m_timerProgresso && !m_timerProgresso->isActive()) {
            if (m_lblProgressoNumerico) m_lblProgressoNumerico->setText("");
            if (m_lblProgressoTopo) {
                m_lblProgressoTopo->setText("");
                m_lblProgressoTopo->setVisible(false);
            }
        }
    });
}

void MainWindow::onServidorOnline(bool online)
{
    if (online) {
        if (m_lblStatusGeral) m_lblStatusGeral->setText("Motor de IA ativo");
        m_lblStatus->setText("Motor de IA ativo");
        emit backendPronto();
        m_net->verificarModelosStatus();
    } else {
        if (m_lblStatusGeral) m_lblStatusGeral->setText("Iniciando motor de IA em segundo plano...");
        m_lblStatus->setText("Iniciando motor de IA em segundo plano...");
        verificarEIniciarBackend();
    }
}

void MainWindow::onModelosStatusRecebido(bool ollamaOnline, bool temPhi3, bool temLlama32, bool temLlama3)
{
    if (!m_comboPerfil) return;
    auto *model = qobject_cast<QStandardItemModel*>(m_comboPerfil->model());
    if (!model) return;

    // Item 0: Nuvem (Gemini / OpenAI) - sempre habilitado
    if (auto *item = model->item(0)) {
        item->setEnabled(true);
        item->setText("Nuvem    — API Gemini/OpenAI  (sem Ollama, requer chave)");
    }

    // Item 1: Básico (phi3:mini)
    const bool hab1 = ollamaOnline && temPhi3;
    if (auto *item = model->item(1)) {
        item->setEnabled(hab1);
        if (!ollamaOnline) {
            item->setText("Básico   — phi3:mini (Ollama desligado)");
        } else if (!temPhi3) {
            item->setText("Básico   — phi3:mini (Não instalado no Ollama)");
        } else {
            item->setText("Básico   — phi3:mini + Whisper tiny.en  (~3 GB RAM)");
        }
    }

    // Item 2: Médio (llama3.2:3b)
    const bool hab2 = ollamaOnline && temLlama32;
    if (auto *item = model->item(2)) {
        item->setEnabled(hab2);
        if (!ollamaOnline) {
            item->setText("Médio    — llama3.2:3b (Ollama desligado)");
        } else if (!temLlama32) {
            item->setText("Médio    — llama3.2:3b (Não instalado no Ollama)");
        } else {
            item->setText("Médio    — llama3.2:3b + Whisper base.en (~6 GB RAM)");
        }
    }

    // Item 3: Avançado (llama3)
    const bool hab3 = ollamaOnline && temLlama3;
    if (auto *item = model->item(3)) {
        item->setEnabled(hab3);
        if (!ollamaOnline) {
            item->setText("Avançado — llama3 (Ollama desligado)");
        } else if (!temLlama3) {
            item->setText("Avançado — llama3 (Não instalado no Ollama)");
        } else {
            item->setText("Avançado — llama3 + Whisper base.en     (~12 GB RAM)");
        }
    }

    // Se o item atualmente selecionado estiver desabilitado, seleciona o melhor disponível
    const int idxAtual = m_comboPerfil->currentIndex();
    if (auto *itemAtual = model->item(idxAtual)) {
        if (!itemAtual->isEnabled()) {
            if (hab2) {
                m_comboPerfil->setCurrentIndex(2);
            } else if (hab1) {
                m_comboPerfil->setCurrentIndex(1);
            } else if (hab3) {
                m_comboPerfil->setCurrentIndex(3);
            } else {
                m_comboPerfil->setCurrentIndex(0); // Nuvem como fallback seguro
            }
        }
    }
    atualizarBadgeModeloAtivo();

    // Auto pull de modelos caso estejam faltando e o perfil exija
    if (ollamaOnline && m_iaProvedor == "ollama") {
        if (!temLlama32 && m_ollamaModelTier == "llama3.2:3b") {
            puxarModeloOllama("llama3.2:3b");
        } else if (!temPhi3 && m_ollamaModelTier == "phi3:mini") {
            puxarModeloOllama("phi3:mini");
        } else if (!temLlama3 && m_ollamaModelTier == "llama3") {
            puxarModeloOllama("llama3");
        }
    }
}

void MainWindow::puxarModeloOllama(const QString &modelo)
{
    if (m_processoOllamaPull && m_processoOllamaPull->state() == QProcess::Running) {
        return; // Já está baixando algo
    }
    
    if (!m_processoOllamaPull) {
        m_processoOllamaPull = new QProcess(this);
        connect(m_processoOllamaPull, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, modelo](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                if (m_net) m_net->verificarModelosStatus(); // Atualiza a UI para liberar o modelo
            }
        });
    }
    
#ifdef Q_OS_WIN
    QString ollamaPath = "ollama.exe";
    QString localAppPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Programs/Ollama/ollama.exe";
    if (QFile::exists(localAppPath)) {
        ollamaPath = localAppPath;
    }
    m_processoOllamaPull->start(ollamaPath, QStringList() << "pull" << modelo);
#else
    m_processoOllamaPull->start("ollama", QStringList() << "pull" << modelo);
#endif
}

void MainWindow::onRequisicaoIniciada(const QString &endpoint)
{
    if (endpoint == "/traduzir") {
        setButtonBusy(m_btnTraduzir, true);
        if (m_lblStatusGeral) m_lblStatusGeral->setText("Traduzindo seleção técnica...");
        m_lblStatus->setText("Traduzindo...");
        const int tempoEst = (m_iaProvedor == "gemini") ? 3200 : 1800;
        iniciarProgresso("Tradução", tempoEst);
    } else if (endpoint == "/falar") {
        setButtonBusy(m_btnOuvir, true);
        if (m_lblStatusGeral) m_lblStatusGeral->setText("Gerando áudio da fala...");
        m_lblStatus->setText("Gerando áudio da fala...");
        iniciarProgresso("Voz", 1200);
    } else if (endpoint.startsWith("/parar_gravacao")) {
        setButtonBusy(m_btnGravar, true);
        if (m_lblStatusGeral) m_lblStatusGeral->setText("Transcrevendo com Whisper...");
        m_lblStatus->setText("Transcrevendo com Whisper...");
        iniciarProgresso("Transcrição", 1500);
    } else if (endpoint == "/avaliar_pronuncia") {
        if (m_lblStatusGeral) m_lblStatusGeral->setText("Analisando pronúncia técnica...");
        m_lblStatus->setText("Analisando pronúncia...");
        iniciarProgresso("Avaliação", 1800);
    } else if (endpoint == "/chat_ia") {
        if (m_promptInput) m_promptInput->setSendButtonBusy(true);
        if (m_lblStatusGeral) m_lblStatusGeral->setText("Consultando Assistente IA...");
        m_lblStatus->setText("Consultando Assistente IA...");
        iniciarProgresso("Assistente", 2800);
    }
}

void MainWindow::onRequisicaoConcluida(const QString &endpoint)
{
    if (endpoint == "/traduzir") setButtonBusy(m_btnTraduzir, false);
    if (endpoint == "/falar")    setButtonBusy(m_btnOuvir, false);
    if (endpoint.startsWith("/parar_gravacao")) setButtonBusy(m_btnGravar, false);
    if (endpoint == "/chat_ia" && m_promptInput) m_promptInput->setSendButtonBusy(false);

    if (m_lblStatusGeral) m_lblStatusGeral->setText("Pronto");
    m_lblStatus->setText("Pronto");
    finalizarProgresso(true);
}

void MainWindow::onErroRequisicao(const QString &endpoint, const QString &mensagem)
{
    if (endpoint == "/traduzir") setButtonBusy(m_btnTraduzir, false);
    if (endpoint == "/falar")    setButtonBusy(m_btnOuvir, false);
    if (endpoint.startsWith("/parar_gravacao")) setButtonBusy(m_btnGravar, false);
    if (endpoint == "/chat_ia") {
        if (m_promptInput) m_promptInput->setSendButtonBusy(false);
        if (m_chatHistorico) {
            m_chatHistorico->append(
                QString("<div style='margin-bottom: 12px; margin-top: 6px; background-color: rgba(239, 68, 68, 0.12); "
                        "border: 1px solid rgba(239, 68, 68, 0.35); border-left: 3px solid #ef4444; border-radius: 8px; padding: 10px 12px;'>"
                        "<div style='font-size: 11px; font-weight: 700; color: #f87171; margin-bottom: 4px;'>Falha na comunicacao com a IA</div>"
                        "<div style='color: #fca5a5; font-size: 13px; line-height: 1.4;'>%1</div>"
                        "<div style='margin-top: 6px; font-size: 11px; color: #94a3b8;'>"
                        "Verifique sua chave de API em <b>Configurar API</b>.</div>"
                        "</div>").arg(mensagem.toHtmlEscaped())
            );
            m_chatHistorico->verticalScrollBar()->setValue(m_chatHistorico->verticalScrollBar()->maximum());
        }
    }
    if (m_lblStatusGeral) m_lblStatusGeral->setText("Falha na operação");
    m_lblStatus->setText("Falha na operação");
    appendLog("Erro em " + endpoint + ": " + mensagem, "error");
    finalizarProgresso(false);
}

void MainWindow::onLimparOcrResultado(const QString &texto)
{
    m_textoOriginalEn = texto;
    if (m_promptTextoTutor) {
        m_promptTextoTutor->setText(texto);
    }
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

    // Mostra na UI
    if (m_logBox) {
        m_logBox->append(QString("<span style='color: #4b5563;'>[%1]</span> <span style='color: %2;'>%3</span>")
                         .arg(timestamp, cor, mensagem.toHtmlEscaped()));
        m_logBox->verticalScrollBar()->setValue(m_logBox->verticalScrollBar()->maximum());
    }
    
    // Grava no arquivo vellum.log
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logPath);
    QFile logFile(logPath + "/vellum.log");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << "[" << timestamp << "] [" << tipo.toUpper() << "] " << mensagem << "\n";
        
        // Rotaciona o log se for maior que 5MB (limita tamanho do arquivo)
        if (logFile.size() > 5 * 1024 * 1024) {
            logFile.close();
            QFile::remove(logPath + "/vellum_old.log");
            QFile::rename(logPath + "/vellum.log", logPath + "/vellum_old.log");
        }
    }
}

void MainWindow::setButtonBusy(QPushButton *btn, bool busy)
{
    if (!btn) return;
    btn->setEnabled(!busy);
}

// ═════════════════════════════════════════════════════════════════════════════
// Assistente Técnico / Chat IA Multimodal (Circuitos, Dúvidas e API)
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::onCapturarCircuitoChat()
{
    m_tipoCaptura = ModoCapturaRubberBand::CircuitoChat;
    m_modoCaptura = true;
    m_pdfView->viewport()->setCursor(Qt::CrossCursor);
    m_pdfView2->viewport()->setCursor(Qt::CrossCursor);
    appendLog("Modo circuito ativado: selecione o circuito ou esquema no leitor PDF com o mouse.", "info");
}

void MainWindow::onAnexarImagemChat()
{
    const QString arq = QFileDialog::getOpenFileName(
        this, "Selecionar Imagem do Circuito", QString(), "Imagens (*.png *.jpg *.jpeg *.webp *.bmp)");
    if (arq.isEmpty()) return;

    QFile file(arq);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = file.readAll();
        m_chatImagemBase64 = QString::fromLatin1(bytes.toBase64());
        QPixmap pix;
        pix.loadFromData(bytes);
        if (m_promptInput) {
            m_promptInput->showImagePreview(m_chatImagemBase64, QFileInfo(arq).fileName());
        }
        appendLog("Imagem anexada: " + QFileInfo(arq).fileName(), "success");
    }
}

void MainWindow::onRemoverImagemChat()
{
    m_chatImagemBase64.clear();
    if (m_promptInput) {
        m_promptInput->hideImagePreview();
    }
}

void MainWindow::onColarTrechoChat()
{
    QString trecho = m_textoOriginalEn;
    if (trecho.isEmpty()) {
        trecho = m_textoTraduzidoPt;
    }
    if (trecho.isEmpty()) {
        appendLog("Nenhum trecho de texto capturado ainda.", "warning");
        return;
    }

    if (m_promptInput) {
        QString atual = m_promptInput->text();
        if (!atual.isEmpty()) atual += "\n\n";
        atual += QString("Explicar este trecho: \"%1\"").arg(trecho);
        m_promptInput->setText(atual);
        m_promptInput->setFocus();
    }
}

void MainWindow::onLimparChat()
{
    m_chatHistoricoJson = QJsonArray();
    m_chatHistorico->setHtml(
        "<div style='color: #94a3b8; font-size: 13px; line-height: 1.5;'>"
        "<b style='color: #60a5fa;'>Conversa reiniciada</b><br>"
        "Historico limpo. Faca uma nova pergunta ou anexe um circuito."
        "</div>"
    );
    appendLog("Historico do Assistente IA limpo.", "info");
}

void MainWindow::onEnviarChat()
{
    const QString texto = m_promptInput ? m_promptInput->text().trimmed() : QString();
    if (texto.isEmpty() && m_chatImagemBase64.isEmpty()) {
        return;
    }

    // Adiciona pergunta do usuário na UI
    QString userHtml = "<div style='margin-bottom: 12px; margin-top: 6px;'>"
                       "<div style='font-size: 11px; font-weight: 600; color: #38bdf8; margin-bottom: 2px;'>Você</div>"
                       "<div style='background-color: #1e293b; border: 1px solid #334155; border-radius: 8px; padding: 8px 10px; color: #f1f5f9; font-size: 13px; line-height: 1.4;'>";

    if (!m_chatImagemBase64.isEmpty()) {
        userHtml += "<div style='color: #93c5fd; font-size: 11px; margin-bottom: 4px;'>[Imagem anexada]</div>";
    }
    if (!texto.isEmpty()) {
        userHtml += texto.toHtmlEscaped().replace("\n", "<br>");
    }
    userHtml += "</div></div>";

    m_chatHistorico->append(userHtml);

    // Salva no histórico JSON
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = texto;
    m_chatHistoricoJson.append(userMsg);

    const QString imgB64 = m_chatImagemBase64;
    onRemoverImagemChat();
    m_promptInput->clearInput();

    if (m_promptInput) m_promptInput->setSendButtonBusy(true);
    m_lblStatus->setText("Consultando Assistente IA...");

    m_net->enviarMensagemChat(texto, imgB64, m_chatHistoricoJson, m_iaProvedor, m_iaApiKey, m_iaModelo);
}

static QString converterFormulaLatex(QString formula, bool isBlock)
{
    formula = formula.trimmed();
    if (formula.startsWith("$$") && formula.endsWith("$$")) formula = formula.mid(2, formula.length() - 4).trimmed();
    else if (formula.startsWith("$") && formula.endsWith("$")) formula = formula.mid(1, formula.length() - 2).trimmed();
    else if (formula.startsWith("\\[") && formula.endsWith("\\]")) formula = formula.mid(2, formula.length() - 4).trimmed();
    else if (formula.startsWith("\\(") && formula.endsWith("\\)")) formula = formula.mid(2, formula.length() - 4).trimmed();

    formula = formula.toHtmlEscaped();

    // 1. \text{...} ou \mathrm{...}
    static const QRegularExpression reText(R"(\\text\{([^}]+)\})");
    formula.replace(reText, "<span style='font-style:normal;font-family:sans-serif;'>\\1</span>");
    static const QRegularExpression reMathrm(R"(\\mathrm\{([^}]+)\})");
    formula.replace(reMathrm, "<span style='font-style:normal;font-family:sans-serif;'>\\1</span>");

    // 2. \frac{A}{B}
    static const QRegularExpression reFrac(R"(\\frac\{([^}]+)\}\{([^}]+)\})");
    if (isBlock) {
        formula.replace(reFrac, 
            "<table style='display:inline-table;vertical-align:middle;border-collapse:collapse;margin:0 4px;font-size:95%;'>"
            "<tr><td style='border-bottom:1px solid #94a3b8;text-align:center;padding:0 4px;'>\\1</td></tr>"
            "<tr><td style='text-align:center;padding:0 4px;'>\\2</td></tr>"
            "</table>");
    } else {
        formula.replace(reFrac, "(\\1 / \\2)");
    }

    // 3. Símbolos gregos
    formula.replace("\\alpha", "&alpha;");
    formula.replace("\\beta", "&beta;");
    formula.replace("\\gamma", "&gamma;");
    formula.replace("\\Gamma", "&Gamma;");
    formula.replace("\\delta", "&delta;");
    formula.replace("\\Delta", "&Delta;");
    formula.replace("\\epsilon", "&epsilon;");
    formula.replace("\\theta", "&theta;");
    formula.replace("\\lambda", "&lambda;");
    formula.replace("\\Lambda", "&Lambda;");
    formula.replace("\\mu", "&mu;");
    formula.replace("\\pi", "&pi;");
    formula.replace("\\Pi", "&Pi;");
    formula.replace("\\sigma", "&sigma;");
    formula.replace("\\Sigma", "&Sigma;");
    formula.replace("\\tau", "&tau;");
    formula.replace("\\phi", "&phi;");
    formula.replace("\\Phi", "&Phi;");
    formula.replace("\\omega", "&omega;");
    formula.replace("\\Omega", "&Omega;");

    // 4. Operadores e relações
    static const QRegularExpression reLatexImplies(R"([/\\]+(?:implies|Rightarrow)\b)", QRegularExpression::CaseInsensitiveOption);
    formula.replace(reLatexImplies, "&rArr;");

    static const QRegularExpression reLatexIff(R"([/\\]+(?:iff|Leftrightarrow)\b)", QRegularExpression::CaseInsensitiveOption);
    formula.replace(reLatexIff, "&hArr;");

    static const QRegularExpression reLatexLeftArrow(R"([/\\]+Leftarrow\b)", QRegularExpression::CaseInsensitiveOption);
    formula.replace(reLatexLeftArrow, "&lArr;");

    static const QRegularExpression reLatexTo(R"([/\\]+(?:to|rightarrow)\b)", QRegularExpression::CaseInsensitiveOption);
    formula.replace(reLatexTo, "&rarr;");

    static const QRegularExpression reLatexLeft(R"([/\\]+leftarrow\b)", QRegularExpression::CaseInsensitiveOption);
    formula.replace(reLatexLeft, "&larr;");

    formula.replace("\\equiv", "&equiv;");
    formula.replace("\\therefore", "&there4;");
    formula.replace("\\sum", "<span style='font-size:120%;font-weight:bold;'>&sum;</span>");
    formula.replace("\\int", "<span style='font-size:120%;font-weight:bold;'>&int;</span>");
    formula.replace("\\approx", "&asymp;");
    formula.replace("\\leq", "&le;");
    formula.replace("\\le", "&le;");
    formula.replace("\\geq", "&ge;");
    formula.replace("\\ge", "&ge;");
    formula.replace("\\neq", "&ne;");
    formula.replace("\\ne", "&ne;");
    formula.replace("\\pm", "&plusmn;");
    formula.replace("\\times", "&times;");
    formula.replace("\\cdot", "&sdot;");
    formula.replace("\\infty", "&infin;");
    formula.replace("\\partial", "&part;");
    formula.replace("\\sqrt", "&radic;");

    // 5. Subscritos e sobrescritos com chaves
    static const QRegularExpression reSubBrace(R"(_\{([^}]+)\})");
    formula.replace(reSubBrace, "<sub>\\1</sub>");
    static const QRegularExpression reSupBrace(R"(\^\{([^}]+)\})");
    formula.replace(reSupBrace, "<sup>\\1</sup>");

    // 6. Subscritos e sobrescritos simples
    static const QRegularExpression reSubSimple(R"(_([a-zA-Z0-9]))");
    formula.replace(reSubSimple, "<sub>\\1</sub>");
    static const QRegularExpression reSupSimple(R"(\^([a-zA-Z0-9]))");
    formula.replace(reSupSimple, "<sup>\\1</sup>");

    if (isBlock) {
        return QString(
            "<div style='margin: 8px 0; padding: 10px 14px; background-color: #161b26; "
            "border: 1px solid #2d3748; border-left: 3px solid #38bdf8; border-radius: 6px; "
            "text-align: center; font-family: \"DejaVu Serif\", \"Cambria Math\", \"Times New Roman\", serif; "
            "font-size: 15px; color: #38bdf8; font-weight: 500;'>"
            "%1"
            "</div>"
        ).arg(formula);
    } else {
        return QString(
            "<span style='font-family: \"DejaVu Serif\", \"Cambria Math\", \"Times New Roman\", serif; "
            "font-style: italic; color: #38bdf8; background-color: #17202f; padding: 1px 5px; "
            "border-radius: 4px; border: 1px solid #253347; font-size: 13px;'>"
            "%1"
            "</span>"
        ).arg(formula);
    }
}

static QString converterTabelaMarkdown(const QString &tableBlock)
{
    QStringList lines = tableBlock.trimmed().split('\n');
    if (lines.size() < 2) return tableBlock;

    QString html = "<table style='width: 100%; border-collapse: collapse; margin: 10px 0; font-size: 12px; background-color: #141822; border: 1px solid #2d3748; border-radius: 6px;'>";

    // Cabeçalho
    QString line0 = lines[0].trimmed();
    if (line0.startsWith('|')) line0 = line0.mid(1);
    if (line0.endsWith('|')) line0.chop(1);
    QStringList headers = line0.split('|');
    html += "<tr style='background-color: #1e2638;'>";
    for (const QString &h : headers) {
        html += QString("<th style='border: 1px solid #2d3748; padding: 7px 10px; color: #93c5fd; text-align: left; font-weight: 600;'>%1</th>").arg(h.trimmed().toHtmlEscaped());
    }
    html += "</tr>";

    int startRow = 1;
    if (lines.size() > 1 && lines[1].contains("---")) {
        startRow = 2;
    }

    for (int r = startRow; r < lines.size(); ++r) {
        QString rowLine = lines[r].trimmed();
        if (rowLine.isEmpty()) continue;
        if (rowLine.startsWith('|')) rowLine = rowLine.mid(1);
        if (rowLine.endsWith('|')) rowLine.chop(1);
        QStringList cells = rowLine.split('|');
        html += "<tr style='border-bottom: 1px solid #242c3d;'>";
        for (const QString &c : cells) {
            html += QString("<td style='border: 1px solid #242c3d; padding: 6px 10px; color: #e2e8f0;'>%1</td>").arg(c.trimmed().toHtmlEscaped());
        }
        html += "</tr>";
    }

    html += "</table>";
    return html;
}

static QString renderizarMarkdownELatex(const QString &texto)
{
    QString result = texto;

    // 1. Extrair Blocos de Código ``` ... ```
    QStringList codeBlocks;
    static const QRegularExpression reCodeBlock(R"(```([a-zA-Z0-9_-]*)\n([\s\S]*?)```)");
    auto matchCode = reCodeBlock.match(result);
    while (matchCode.hasMatch()) {
        const QString code = matchCode.captured(2).toHtmlEscaped();
        const QString htmlCode = QString(
            "<pre style='background-color: #151821; border: 1px solid #282f3f; border-radius: 6px; padding: 10px; font-family: monospace; font-size: 12px; color: #38bdf8; overflow-x: auto;'><code>%1</code></pre>"
        ).arg(code);
        const QString token = QString("@@CODE_BLOCK_%1@@").arg(codeBlocks.size());
        codeBlocks.append(htmlCode);
        result.replace(matchCode.capturedStart(), matchCode.capturedLength(), token);
        matchCode = reCodeBlock.match(result);
    }

    // 2. Extrair Fórmulas em Bloco $$ ... $$ e \[ ... \]
    QStringList mathBlocks;
    static const QRegularExpression reBlockMath(R"(\$\$([^$]+?)\$\$|\\\[([\s\S]*?)\\\])");
    auto matchBlockMath = reBlockMath.match(result);
    while (matchBlockMath.hasMatch()) {
        QString formula = matchBlockMath.captured(1);
        if (formula.isEmpty()) formula = matchBlockMath.captured(2);
        const QString htmlMath = converterFormulaLatex(formula, true);
        const QString token = QString("@@MATH_BLOCK_%1@@").arg(mathBlocks.size());
        mathBlocks.append(htmlMath);
        result.replace(matchBlockMath.capturedStart(), matchBlockMath.capturedLength(), token);
        matchBlockMath = reBlockMath.match(result);
    }

    // 3. Extrair Fórmulas Inline $ ... $ e \( ... \)
    QStringList mathInlines;
    static const QRegularExpression reInlineMath(R"((?<!\$)\$([^$\n]+?)\$(?!\$)|\\\(([\s\S]*?)\\\))");
    auto matchInlineMath = reInlineMath.match(result);
    while (matchInlineMath.hasMatch()) {
        QString formula = matchInlineMath.captured(1);
        if (formula.isEmpty()) formula = matchInlineMath.captured(2);
        const QString htmlMath = converterFormulaLatex(formula, false);
        const QString token = QString("@@MATH_INLINE_%1@@").arg(mathInlines.size());
        mathInlines.append(htmlMath);
        result.replace(matchInlineMath.capturedStart(), matchInlineMath.capturedLength(), token);
        matchInlineMath = reInlineMath.match(result);
    }

    // 4. Extrair Tabelas Markdown
    QStringList tableBlocks;
    static const QRegularExpression reTable(R"((?:^[ \t]*\|.+?\|[ \t]*\n?){2,})", QRegularExpression::MultilineOption);
    auto matchTable = reTable.match(result);
    while (matchTable.hasMatch()) {
        const QString tblRaw = matchTable.captured(0);
        const QString htmlTbl = converterTabelaMarkdown(tblRaw);
        const QString token = QString("@@TABLE_BLOCK_%1@@").arg(tableBlocks.size());
        tableBlocks.append(htmlTbl);
        result.replace(matchTable.capturedStart(), matchTable.capturedLength(), token);
        matchTable = reTable.match(result);
    }

    // 5. Escapar caracteres HTML no restante do texto
    result = result.toHtmlEscaped();

    // 6. Cabeçalhos Markdown
    static const QRegularExpression reH3(R"(^###[ \t]+([^\n]+))", QRegularExpression::MultilineOption);
    result.replace(reH3, "<div style='font-size: 14px; font-weight: 700; color: #c084fc; margin-top: 10px; margin-bottom: 4px;'>\\1</div>");
    static const QRegularExpression reH2(R"(^##[ \t]+([^\n]+))", QRegularExpression::MultilineOption);
    result.replace(reH2, "<div style='font-size: 15px; font-weight: 700; color: #a78bfa; margin-top: 12px; margin-bottom: 6px;'>\\1</div>");
    static const QRegularExpression reH1(R"(^#[ \t]+([^\n]+))", QRegularExpression::MultilineOption);
    result.replace(reH1, "<div style='font-size: 16px; font-weight: 700; color: #f1f5f9; margin-top: 14px; margin-bottom: 6px;'>\\1</div>");

    // Linha horizontal
    static const QRegularExpression reHr(R"(^---[ \t]*$|^\*\*\*[ \t]*$)", QRegularExpression::MultilineOption);
    result.replace(reHr, "<hr style='border: 0; border-top: 1px solid #282f3d; margin: 10px 0;'>");

    // Negrito e Itálico
    static const QRegularExpression reBold(R"(\*\*([^*]+)\*\*)");
    result.replace(reBold, "<strong style='color: #f8fafc;'>\\1</strong>");
    static const QRegularExpression reItalic(R"((?<!\*)\*([^*\n]+)\*(?!\*))");
    result.replace(reItalic, "<em>\\1</em>");

    // Código inline
    static const QRegularExpression reInlineCode(R"(`([^`]+)`)");
    result.replace(reInlineCode, "<code style='background-color: #272a33; color: #38bdf8; padding: 2px 5px; border-radius: 4px; font-family: monospace;'>\\1</code>");

    // Listas com marcadores (* ou -)
    static const QRegularExpression reBullet(R"(^[ \t]*[\*\-][ \t]+([^\n]+))", QRegularExpression::MultilineOption);
    result.replace(reBullet, "<div style='margin-left: 12px; margin-bottom: 3px;'>• \\1</div>");

    // Listas numeradas (1. 2. etc)
    static const QRegularExpression reNumList(R"(^[ \t]*([0-9]+)\.[ \t]+([^\n]+))", QRegularExpression::MultilineOption);
    result.replace(reNumList, "<div style='margin-left: 12px; margin-bottom: 3px;'><b style='color: #a78bfa;'>\\1.</b> \\2</div>");

    // Quebras de linha
    result.replace("\n\n", "<br><br>");
    result.replace("\n", "<br>");

    // Substituições de operadores lógicos comuns no texto (ex: \implies, /implies, \iff, /iff, etc.)
    static const QRegularExpression reImpliesText(R"([/\\]+(?:implies|Rightarrow)\b)", QRegularExpression::CaseInsensitiveOption);
    result.replace(reImpliesText, "<span style='font-size:115%; color:#38bdf8;'>&nbsp;&rArr;&nbsp;</span>");

    static const QRegularExpression reIffText(R"([/\\]+(?:iff|Leftrightarrow)\b)", QRegularExpression::CaseInsensitiveOption);
    result.replace(reIffText, "<span style='font-size:115%; color:#38bdf8;'>&nbsp;&hArr;&nbsp;</span>");

    static const QRegularExpression reToText(R"([/\\]+(?:to|rightarrow)\b)", QRegularExpression::CaseInsensitiveOption);
    result.replace(reToText, "<span style='font-size:115%; color:#38bdf8;'>&nbsp;&rarr;&nbsp;</span>");

    // Também suporta setas textuais comuns como ==> ou -->
    static const QRegularExpression reArrowDbl(R"((?<=\s)==&gt;(?=\s)|(?<=\s)=&gt;(?=\s))");
    result.replace(reArrowDbl, "<span style='font-size:115%; color:#38bdf8;'>&nbsp;&rArr;&nbsp;</span>");

    static const QRegularExpression reArrowSgl(R"((?<=\s)--&gt;(?=\s)|(?<=\s)-&gt;(?=\s))");
    result.replace(reArrowSgl, "<span style='font-size:115%; color:#38bdf8;'>&nbsp;&rarr;&nbsp;</span>");

    // 7. Reinserir Blocos Protegidos
    for (int i = 0; i < tableBlocks.size(); ++i) {
        result.replace(QString("@@TABLE_BLOCK_%1@@").arg(i), tableBlocks[i]);
    }
    for (int i = 0; i < mathBlocks.size(); ++i) {
        result.replace(QString("@@MATH_BLOCK_%1@@").arg(i), mathBlocks[i]);
    }
    for (int i = 0; i < mathInlines.size(); ++i) {
        result.replace(QString("@@MATH_INLINE_%1@@").arg(i), mathInlines[i]);
    }
    for (int i = 0; i < codeBlocks.size(); ++i) {
        result.replace(QString("@@CODE_BLOCK_%1@@").arg(i), codeBlocks[i]);
    }

    return result;
}

void MainWindow::onChatRespostaResultado(const QString &resposta, const QString &provedor, const QString &modelo)
{
    if (m_promptInput) m_promptInput->setSendButtonBusy(false);
    m_lblStatus->setText("Pronto");

    // Salva no histórico JSON
    QJsonObject botMsg;
    botMsg["role"] = "assistant";
    botMsg["content"] = resposta;
    m_chatHistoricoJson.append(botMsg);

    // Formata resposta com suporte a LaTeX, tabelas e Markdown avançado
    const QString formatted = renderizarMarkdownELatex(resposta);

    QString botHtml = QString(
        "<div style='margin-bottom: 16px; margin-top: 6px;'>"
        "<div style='font-size: 11px; font-weight: 600; color: #a78bfa; margin-bottom: 2px;'>Assistente Técnico (%1: %2)</div>"
        "<div style='background-color: #171b24; border: 1px solid #282f3d; border-left: 3px solid #8b5cf6; border-radius: 8px; padding: 10px 12px; color: #e2e8f0; font-size: 13px; line-height: 1.5;'>%3</div>"
        "</div>"
    ).arg(provedor.toUpper(), modelo, formatted);

    m_chatHistorico->append(botHtml);
    m_chatHistorico->verticalScrollBar()->setValue(m_chatHistorico->verticalScrollBar()->maximum());
    appendLog(QString("Resposta do Assistente IA recebida (%1).").arg(provedor), "success");
}

void MainWindow::onConfigurarIA()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Configurar API");
    dlg.setMinimumWidth(420);
    dlg.setStyleSheet(
        "QDialog { background-color: #131418; color: #e2e8f0; }"
        "QLabel { color: #cbd5e1; font-size: 12px; }"
        "QLineEdit, QComboBox {"
        "  background-color: #1c1e24; border: 1px solid #333846; border-radius: 6px; padding: 6px 8px; color: #f1f5f9; font-size: 12px;"
        "}"
        "QLineEdit:focus, QComboBox:focus { border-color: #3b82f6; }"
        "QPushButton { border-radius: 6px; padding: 6px 14px; font-weight: 500; font-size: 12px; }"
    );

    auto *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);

    // Modo: Online (Gemini) ou Offline (Ollama)
    auto *form = new QFormLayout();
    form->setSpacing(10);

    auto *comboModo = new QComboBox(&dlg);
    comboModo->addItem("Google Gemini (online, requer chave)", "gemini");
    comboModo->addItem("Ollama (offline, local)", "ollama");
    comboModo->setCurrentIndex(m_iaProvedor == "ollama" ? 1 : 0);
    form->addRow("Modo:", comboModo);

    // Chave Gemini
    auto *txtApiKey = new QLineEdit(&dlg);
    txtApiKey->setEchoMode(QLineEdit::Password);
    txtApiKey->setText(m_iaApiKey);
    txtApiKey->setPlaceholderText("Chave de API do Google Gemini");

    auto *chkMostrarKey = new QCheckBox("Mostrar chave", &dlg);
    chkMostrarKey->setStyleSheet("color: #94a3b8; font-size: 11px;");
    connect(chkMostrarKey, &QCheckBox::toggled, &dlg, [txtApiKey](bool checked) {
        txtApiKey->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });

    auto *layoutKey = new QVBoxLayout();
    layoutKey->addWidget(txtApiKey);
    layoutKey->addWidget(chkMostrarKey);

    auto *lblLinkKey = new QLabel(
        "<a href='https://aistudio.google.com/app/apikey' style='color:#60a5fa; text-decoration:none;'>"
        "Gerar chave gratuita em aistudio.google.com</a>", &dlg);
    lblLinkKey->setOpenExternalLinks(true);
    layoutKey->addWidget(lblLinkKey);

    form->addRow("Chave Gemini:", layoutKey);

    // Modelo Gemini
    auto *comboModelo = new QComboBox(&dlg);
    comboModelo->addItem("gemini-3.5-flash-lite (Ultra Rápido - Recomendado)", "gemini-3.5-flash-lite");
    comboModelo->addItem("gemini-3.5-flash (Balanceado / Alta Precisão)", "gemini-3.5-flash");
    comboModelo->addItem("gemini-3.8-flash (Experimental / Alta Demanda)", "gemini-3.8-flash");
    const int idxMod = comboModelo->findData(m_iaModelo);
    if (idxMod >= 0) comboModelo->setCurrentIndex(idxMod);
    form->addRow("Modelo Gemini:", comboModelo);

    // Modelo Ollama (offline)
    auto *txtOllamaModelo = new QLineEdit(&dlg);
    txtOllamaModelo->setText(m_iaProvedor == "ollama" ? m_iaModelo : "llama3");
    txtOllamaModelo->setPlaceholderText("ex: llama3, mistral");
    form->addRow("Modelo Ollama:", txtOllamaModelo);

    layout->addLayout(form);

    auto *btnBox = new QHBoxLayout();
    btnBox->addStretch();
    auto *btnCancelar = new QPushButton("Cancelar", &dlg);
    btnCancelar->setStyleSheet("background-color: #272a33; color: #94a3b8; border: 1px solid #3e4452;");
    connect(btnCancelar, &QPushButton::clicked, &dlg, &QDialog::reject);
    btnBox->addWidget(btnCancelar);

    auto *btnSalvar = new QPushButton("Salvar", &dlg);
    btnSalvar->setStyleSheet("background-color: #2563eb; color: #ffffff; border: none; font-weight: 600;");
    connect(btnSalvar, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnBox->addWidget(btnSalvar);

    layout->addLayout(btnBox);

    if (dlg.exec() == QDialog::Accepted) {
        m_iaProvedor = comboModo->currentData().toString();
        m_iaApiKey   = txtApiKey->text().trimmed();
        if (m_iaProvedor == "gemini") {
            m_iaModelo = comboModelo->currentData().toString();
        } else {
            m_iaModelo = txtOllamaModelo->text().trimmed();
            if (m_iaModelo.isEmpty()) m_iaModelo = "llama3";
        }
        salvarConfiguracoes();

        if (m_lblModeloAtivoChat) {
            m_lblModeloAtivoChat->setText(m_iaModelo);
        }
        if (m_comboChatIA) {
            const int idx = m_comboChatIA->findData(m_iaProvedor);
            if (idx >= 0) m_comboChatIA->setCurrentIndex(idx);
        }
        atualizarBadgeModeloAtivo();
        appendLog("Configurações de IA salvas.", "success");
    }
}

void MainWindow::verificarEIniciarBackend()
{
    if (m_backendProcess && m_backendProcess->state() == QProcess::Running) {
        return;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidatosDirs;
    candidatosDirs << appDir;
    candidatosDirs << appDir + "/backend";
    candidatosDirs << appDir + "/bin";
    candidatosDirs << QDir(appDir).filePath("..");
    candidatosDirs << QDir(appDir).filePath("../backend");
    candidatosDirs << QDir(appDir).filePath("../bin");
    candidatosDirs << QDir(appDir).filePath("../..");
    candidatosDirs << QDir::currentPath();

    QString execPath;
    QStringList args;
    QString workingDir;

#ifdef Q_OS_WIN
    const QStringList binCandidates = {"vellum_backend.exe", "leitor_backend.exe"};
#else
    const QStringList binCandidates = {"vellum_backend", "leitor_backend"};
#endif

    for (const QString &dirPath : candidatosDirs) {
        QDir d(dirPath);
        const QString base = d.canonicalPath();
        if (base.isEmpty()) continue;

        // 1. Procura executável congelado (distribuição/instalador final)
        bool achouBin = false;
        for (const QString &binName : binCandidates) {
            if (QFile::exists(base + "/" + binName)) {
                execPath = base + "/" + binName;
                workingDir = base;
                achouBin = true;
                break;
            }
            if (QFile::exists(base + "/bin/" + binName)) {
                execPath = base + "/bin/" + binName;
                workingDir = base;
                achouBin = true;
                break;
            }
            if (QFile::exists(base + "/backend/" + binName)) {
                execPath = base + "/backend/" + binName;
                workingDir = base + "/backend";
                achouBin = true;
                break;
            }
        }
        if (achouBin) break;

        // 2. Procura ambiente com main.py (desenvolvimento / venv)
        const QString scriptMain = base + "/main.py";
        if (QFile::exists(scriptMain)) {
            workingDir = base;
            const QString venvLinux = base + "/venv/bin/python3";
            const QString venvWinW = base + "/venv/Scripts/pythonw.exe";
            const QString venvWin = base + "/venv/Scripts/python.exe";

            if (QFile::exists(venvLinux)) {
                execPath = venvLinux;
            } else if (QFile::exists(venvWinW)) {
                execPath = venvWinW; // Sem console preto no Windows
            } else if (QFile::exists(venvWin)) {
                execPath = venvWin;
            } else {
#ifdef Q_OS_WIN
                execPath = "pythonw";
#else
                execPath = "python3";
#endif
            }
            args << scriptMain;
            break;
        }
    }

    if (execPath.isEmpty()) {
        appendLog("Não foi possível localizar o executável ou script do backend.", "warning");
        return;
    }

    if (!m_backendProcess) {
        m_backendProcess = new QProcess(this);
        
        // Capturar logs do backend e direcioná-los para o appendLog
        connect(m_backendProcess, &QProcess::readyReadStandardOutput, this, [this]() {
            QByteArray out = m_backendProcess->readAllStandardOutput();
            // Evitar linhas vazias indesejadas
            for (const QByteArray &linha : out.split('\n')) {
                if (!linha.trimmed().isEmpty()) {
                    appendLog(QString::fromUtf8(linha).trimmed(), "info");
                }
            }
        });
        connect(m_backendProcess, &QProcess::readyReadStandardError, this, [this]() {
            QByteArray err = m_backendProcess->readAllStandardError();
            for (const QByteArray &linha : err.split('\n')) {
                if (!linha.trimmed().isEmpty()) {
                    appendLog(QString::fromUtf8(linha).trimmed(), "warning");
                }
            }
        });
        
        // Alerta caso o processo do backend morra ou falhe
        connect(m_backendProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
            if (!m_isFechando) {
                QString msg = QString("O motor de IA encerrou inesperadamente (Código: %1, Status: %2).")
                                .arg(exitCode).arg(exitStatus == QProcess::CrashExit ? "Crash" : "Normal");
                appendLog(msg, "error");
                
                QMessageBox msgBox(this);
                msgBox.setWindowTitle("Erro no Motor de IA");
                msgBox.setText("<b>O Vellum perdeu a conexão com o motor de Inteligência Artificial.</b>");
                msgBox.setInformativeText("Isolamos os logs do backend em <i>vellum.log</i> para análise.\n\nO aplicativo tentará reiniciá-lo automaticamente.");
                msgBox.setIcon(QMessageBox::Critical);
                msgBox.exec();
                
                // Reinicia em 3s
                QTimer::singleShot(3000, this, &MainWindow::verificarEIniciarBackend);
            }
        });
        
        connect(m_backendProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (!m_isFechando) {
                appendLog(QString("Erro no processo do motor de IA: %1").arg(error), "error");
            }
        });
    }

    m_backendProcess->setWorkingDirectory(workingDir);
    appendLog(QString("Iniciando motor de IA na porta %1...").arg(m_backendPort), "info");
    args << "--port" << QString::number(m_backendPort)
         << "--ppid" << QString::number(QCoreApplication::applicationPid());
    
    // Configurar o QProcess para interceptar stdout e stderr antes de iniciar
    m_backendProcess->setProcessChannelMode(QProcess::SeparateChannels);
    
    m_backendProcess->start(execPath, args);

    // Tenta pingar a conexão gradualmente
    for (int ms : {1200, 2500, 4500, 7000}) {
        QTimer::singleShot(ms, this, [this]() {
            if (m_net) m_net->verificarConexao();
        });
    }
}

void MainWindow::pararBackend()
{
    if (m_backendProcess && m_backendProcess->state() == QProcess::Running) {
        m_backendProcess->terminate();
        if (!m_backendProcess->waitForFinished(2000)) {
            m_backendProcess->kill();
        }
        m_backendProcess->deleteLater();
        m_backendProcess = nullptr;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Aba: Bloco de Notas / Anotações
// ═════════════════════════════════════════════════════════════════════════════

void MainWindow::onSalvarAnotacoes()
{
    if (!m_txtAnotacoes) return;

    QString caminho = m_caminhoArquivoNota;
    if (caminho.isEmpty()) {
        caminho = QFileDialog::getSaveFileName(
            this,
            "Salvar Anotações",
            QDir::homePath() + "/MinhasAnotacoes.html",
            "Documento HTML com formatação (*.html);;Texto Puro (*.txt);;Todos os Arquivos (*.*)"
        );
    }
    if (caminho.isEmpty()) return;

    QFile file(caminho);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Erro ao Salvar", "Não foi possível gravar no arquivo:\n" + file.errorString());
        return;
    }

    QTextStream out(&file);
    if (caminho.endsWith(".txt", Qt::CaseInsensitive)) {
        out << m_txtAnotacoes->toPlainText();
    } else {
        out << m_txtAnotacoes->toHtml();
    }
    file.close();

    m_caminhoArquivoNota = caminho;
    if (m_lblArquivoNota) {
        m_lblArquivoNota->setText(QFileInfo(caminho).fileName());
    }
    appendLog(QString("Anotações salvas com sucesso em %1").arg(QFileInfo(caminho).fileName()), "success");
}

void MainWindow::onAbrirAnotacoes()
{
    if (!m_txtAnotacoes) return;

    const QString caminho = QFileDialog::getOpenFileName(
        this,
        "Abrir Anotações",
        QDir::homePath(),
        "Arquivos Suportados (*.html *.htm *.txt *.md);;Documentos HTML (*.html *.htm);;Texto Puro (*.txt *.md);;Todos os Arquivos (*.*)"
    );
    if (caminho.isEmpty()) return;

    QFile file(caminho);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Erro ao Abrir", "Não foi possível abrir o arquivo:\n" + file.errorString());
        return;
    }

    const QString conteudo = QString::fromUtf8(file.readAll());
    file.close();

    if (caminho.endsWith(".html", Qt::CaseInsensitive) || caminho.endsWith(".htm", Qt::CaseInsensitive)) {
        m_txtAnotacoes->setHtml(conteudo);
    } else {
        m_txtAnotacoes->setPlainText(conteudo);
    }

    m_caminhoArquivoNota = caminho;
    if (m_lblArquivoNota) {
        m_lblArquivoNota->setText(QFileInfo(caminho).fileName());
    }
    appendLog(QString("Anotações carregadas: %1").arg(QFileInfo(caminho).fileName()), "info");
}

void MainWindow::onLimparAnotacoes()
{
    if (!m_txtAnotacoes) return;
    if (!m_txtAnotacoes->toPlainText().trimmed().isEmpty()) {
        const auto resp = QMessageBox::question(
            this,
            "Novo Bloco de Notas",
            "Deseja limpar as anotações atuais e começar uma folha em branco?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (resp != QMessageBox::Yes) return;
    }
    m_txtAnotacoes->clear();
    m_caminhoArquivoNota.clear();
    if (m_lblArquivoNota) {
        m_lblArquivoNota->setText("Novo Documento");
    }
}

void MainWindow::onCopiarAnotacoes()
{
    if (!m_txtAnotacoes) return;
    QClipboard *clip = QGuiApplication::clipboard();
    if (clip) {
        clip->setText(m_txtAnotacoes->toPlainText());
        appendLog("Anotações copiadas para a área de transferência.", "info");
    }
}

void MainWindow::onMudarFonteAnotacoes(const QString &familia)
{
    if (!m_txtAnotacoes) return;
    m_txtAnotacoes->setFontFamily(familia);
}

void MainWindow::onMudarTamanhoAnotacoes(int pt)
{
    if (!m_txtAnotacoes) return;
    m_txtAnotacoes->setFontPointSize(pt);
}

void MainWindow::onAumentarFonteAnotacoes()
{
    if (!m_spinTamanhoNota) return;
    m_spinTamanhoNota->setValue(std::min(m_spinTamanhoNota->value() + 1, 36));
}

void MainWindow::onDiminuirFonteAnotacoes()
{
    if (!m_spinTamanhoNota) return;
    m_spinTamanhoNota->setValue(std::max(m_spinTamanhoNota->value() - 1, 9));
}

void MainWindow::onToggleBoldAnotacoes()
{
    if (!m_txtAnotacoes || !m_btnBoldNota) return;
    m_txtAnotacoes->setFontWeight(m_btnBoldNota->isChecked() ? QFont::Bold : QFont::Normal);
}

void MainWindow::onToggleItalicAnotacoes()
{
    if (!m_txtAnotacoes || !m_btnItalicNota) return;
    m_txtAnotacoes->setFontItalic(m_btnItalicNota->isChecked());
}

void MainWindow::onToggleUnderlineAnotacoes()
{
    if (!m_txtAnotacoes || !m_btnUnderlineNota) return;
    m_txtAnotacoes->setFontUnderline(m_btnUnderlineNota->isChecked());
}

void MainWindow::onEscolherCorAnotacoes()
{
    if (!m_txtAnotacoes) return;
    const QColor inicial = m_txtAnotacoes->textColor().isValid() ? m_txtAnotacoes->textColor() : QColor("#f1f5f9");
    const QColor escolhida = QColorDialog::getColor(inicial, this, "Selecionar Cor do Texto");
    if (escolhida.isValid()) {
        m_txtAnotacoes->setTextColor(escolhida);
        if (m_btnCorNota) {
            m_btnCorNota->setStyleSheet(QString(
                "QPushButton { background-color: %1; color: %2; font-weight: 600; border: 1px solid %1; border-radius: 4px; padding: 3px 10px; font-size: 11px; }"
                "QPushButton:hover { opacity: 0.9; }"
            ).arg(escolhida.name(), escolhida.lightness() > 140 ? "#000000" : "#ffffff"));
        }
    }
}

void MainWindow::onAtualizarContadorNotas()
{
    if (!m_txtAnotacoes || !m_lblStatusNota) return;
    const QString texto = m_txtAnotacoes->toPlainText().trimmed();
    const int chars = texto.length();
    const int palavras = texto.isEmpty() ? 0 : texto.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).count();
    m_lblStatusNota->setText(QString("%1 palavras | %2 caracteres").arg(palavras).arg(chars));
}

