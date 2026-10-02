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
#include <QTimer>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextBrowser>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStatusBar>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
// Construtor e Inicialização
// ═════════════════════════════════════════════════════════════════════════════

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Leitor Técnico de Documentos");
    setMinimumSize(960, 640);

    m_timerProgresso = new QTimer(this);
    connect(m_timerProgresso, &QTimer::timeout, this, &MainWindow::atualizarProgressoPasso);

    m_net = new NetworkManager("http://localhost:8000", this);

    setupMenuBar();
    setupUi();
    setupStyleSheet();
    connectSignals();

    carregarConfiguracoes();

    // Garante tela cheia e painel fechado sempre ao abrir
    showMaximized();
    m_painelDir->setVisible(false);
    m_actTogglePainel->setChecked(false);

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

    auto *actAbaChat = menuPainel->addAction("Ir para Assistente IA & Dúvidas");
    actAbaChat->setShortcut(QKeySequence("Ctrl+I"));
    connect(actAbaChat, &QAction::triggered, this, [this]() { onMudarAbaPainel(2); });

    auto *actAbaConfig = menuPainel->addAction("Ir para Desempenho & Layout");
    actAbaConfig->setShortcut(QKeySequence("Ctrl+D"));
    connect(actAbaConfig, &QAction::triggered, this, [this]() { onMudarAbaPainel(3); });

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
    m_tabWidget->addTab(criarAbaChatIA(), "Assistente IA");
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

    // Preview do Circuito / Imagem Anexada (inicialmente oculto)
    m_chatPreviewWidget = new QWidget(aba);
    auto *previewLayout = new QHBoxLayout(m_chatPreviewWidget);
    previewLayout->setContentsMargins(6, 4, 6, 4);
    previewLayout->setSpacing(8);
    m_chatPreviewWidget->setStyleSheet("background-color: #1a1f29; border: 1px solid #334155; border-radius: 6px;");

    m_chatThumbLabel = new QLabel(m_chatPreviewWidget);
    m_chatThumbLabel->setFixedSize(48, 48);
    m_chatThumbLabel->setScaledContents(true);
    m_chatThumbLabel->setStyleSheet("border: 1px solid #475569; border-radius: 4px; background: #0f172a;");
    previewLayout->addWidget(m_chatThumbLabel);

    m_chatThumbTexto = new QLabel("Circuito anexado para analise tecnica", m_chatPreviewWidget);
    m_chatThumbTexto->setStyleSheet("color: #e2e8f0; font-size: 11px;");
    previewLayout->addWidget(m_chatThumbTexto, 1);

    m_btnRemoverThumb = new QPushButton("X", m_chatPreviewWidget);
    m_btnRemoverThumb->setFixedSize(24, 24);
    m_btnRemoverThumb->setToolTip("Remover anexo");
    m_btnRemoverThumb->setStyleSheet("background: transparent; color: #ef4444; font-size: 12px; border: none; font-weight: bold;");
    previewLayout->addWidget(m_btnRemoverThumb);

    m_chatPreviewWidget->setVisible(false);
    layout->addWidget(m_chatPreviewWidget);

    // Barra de Ferramentas / Acoes Rapidas
    auto *layoutAcoes = new QHBoxLayout();
    layoutAcoes->setSpacing(6);

    m_btnCapturarCircuito = new QPushButton("Capturar Circuito", aba);
    m_btnCapturarCircuito->setObjectName("btnSecundario");
    m_btnCapturarCircuito->setToolTip("Selecione um circuito ou esquema no livro para a IA analisar");
    layoutAcoes->addWidget(m_btnCapturarCircuito);

    m_btnAnexarImagem = new QPushButton("Anexar Imagem", aba);
    m_btnAnexarImagem->setObjectName("btnSecundario");
    m_btnAnexarImagem->setToolTip("Anexar arquivo de imagem do computador");
    layoutAcoes->addWidget(m_btnAnexarImagem);

    m_btnColarTrecho = new QPushButton("Colar Trecho", aba);
    m_btnColarTrecho->setObjectName("btnSecundario");
    m_btnColarTrecho->setToolTip("Inclui o texto do OCR na sua pergunta");
    layoutAcoes->addWidget(m_btnColarTrecho);

    layout->addLayout(layoutAcoes);

    // Campo de Entrada e Botao Enviar
    auto *layoutInput = new QHBoxLayout();
    layoutInput->setSpacing(6);

    m_chatInput = new QTextEdit(aba);
    m_chatInput->setObjectName("chatInput");
    m_chatInput->setMaximumHeight(70);
    m_chatInput->setPlaceholderText("Pergunte sobre o circuito ou texto... (Ctrl+Enter para enviar)");
    m_chatInput->setStyleSheet(
        "QTextEdit#chatInput {"
        "  background-color: #1a1c22;"
        "  border: 1px solid #2d313b;"
        "  border-radius: 6px;"
        "  padding: 6px;"
        "  color: #f1f5f9;"
        "  font-size: 12px;"
        "}"
        "QTextEdit#chatInput:focus {"
        "  border: 1px solid #3b82f6;"
        "}"
    );
    layoutInput->addWidget(m_chatInput, 1);

    m_btnChatEnviar = new QPushButton("Enviar", aba);
    m_btnChatEnviar->setObjectName("btnPrimario");
    m_btnChatEnviar->setFixedSize(65, 70);
    m_btnChatEnviar->setToolTip("Enviar pergunta (Ctrl+Enter)");
    layoutInput->addWidget(m_btnChatEnviar);

    layout->addLayout(layoutInput);

    return aba;
}

// ─────────────────────────────────────────────────────────────────────────────
// Aba 4: Desempenho & Layout
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
                    m_iaModelo = "gemini-3.8-flash";
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
    connect(m_net, &NetworkManager::chatRespostaResultado,        this, &MainWindow::onChatRespostaResultado);

    // Assistente IA & Chat
    connect(m_btnConfigurarIA,     &QPushButton::clicked, this, &MainWindow::onConfigurarIA);
    connect(m_btnLimparChat,       &QPushButton::clicked, this, &MainWindow::onLimparChat);
    connect(m_btnCapturarCircuito, &QPushButton::clicked, this, &MainWindow::onCapturarCircuitoChat);
    connect(m_btnAnexarImagem,     &QPushButton::clicked, this, &MainWindow::onAnexarImagemChat);
    connect(m_btnRemoverThumb,     &QPushButton::clicked, this, &MainWindow::onRemoverImagemChat);
    connect(m_btnColarTrecho,      &QPushButton::clicked, this, &MainWindow::onColarTrechoChat);
    connect(m_btnChatEnviar,       &QPushButton::clicked, this, &MainWindow::onEnviarChat);

    connect(m_pdfView->pageNavigator(), &QPdfPageNavigator::currentPageChanged,
            this, [this](int pag) {
        if (!m_bloquearSyncPagina && m_modoVis == ModoVisualizacao::Continuo) {
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
    m_iaModelo   = settings.value("iaModelo", "gemini-3.8-flash").toString();
    // Migrar modelos obsoletos para o atual
    static const QSet<QString> obsoletos = {
        "gemini-1.5-flash", "gemini-2.0-flash", "gemini-1.5-pro", "gemini-2.5-flash"
    };
    if (m_iaModelo.isEmpty() || obsoletos.contains(m_iaModelo)) {
        m_iaModelo = "gemini-3.8-flash";
    }

    // Tier de hardware
    const int tierIdx = settings.value("tierIndex", 2).toInt(); // default: avancado
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
            m_iaModelo = "gemini-3.8-flash";
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
    QSettings settings("EnsinadorDeIngles", "LeitorTecnico");
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
    settings.setValue("tierIndex",        m_comboPerfil ? m_comboPerfil->currentIndex() : 2);
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
    // Passa a chave Gemini: se disponivel, a traducao roda na nuvem (leve para PCs fracos)
    const QString geminiKey = (m_iaProvedor == "gemini") ? m_iaApiKey : QString();
    if (m_textoOcrAtual.startsWith("BASE64:")) {
        const QByteArray b64 = m_textoOcrAtual.mid(7).toLatin1();
        m_net->traduzirDireto(QString(), b64, geminiKey);
    } else if (!m_textoOcrAtual.isEmpty()) {
        m_net->traduzirDireto(m_textoOcrAtual, QByteArray(), geminiKey);
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

    // Envia automaticamente para avaliação fonética do tutor com o nível de exigência escolhido
    if (!m_textoOriginalEn.isEmpty()) {
        const QString nivel = m_comboNivelTutor->currentData().toString();
        appendLog(QString("Avaliando precisão da pronúncia (Nível: %1)...").arg(m_comboNivelTutor->currentText()), "info");
        m_net->avaliarPronuncia(m_textoOriginalEn, texto, nivel);
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

    QSettings settings("EnsinadorDeIngles", "LeitorTecnico");
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
    const bool usaGemini = (m_iaProvedor == "gemini" && !m_iaApiKey.trimmed().isEmpty());

    if (usaGemini) {
        modeloNome = "Gemini (Nuvem)";
    } else {
        const int tierIdx = m_comboPerfil ? m_comboPerfil->currentIndex() : 2;
        if (tierIdx == 0) {
            modeloNome = "Nuvem (Chave API necessária)";
        } else if (!m_ollamaModelTier.isEmpty()) {
            modeloNome = QString("%1 (Local)").arg(m_ollamaModelTier);
        } else {
            modeloNome = "Ollama (Local)";
        }
    }

    m_badgeModeloAtivo->setText(QString("Tradução: %1").arg(modeloNome));
    m_badgeModeloAtivo->setToolTip(
        QString("Motor ativo para tradução técnica e OCR:\n%1\n(Altere nas abas Desempenho & Layout ou Configurar API)").arg(modeloNome)
    );
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
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
    }
    QMainWindow::keyPressEvent(event);
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
    case QEvent::KeyPress: {
        auto *ke = static_cast<QKeyEvent*>(event);
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
                    if (m_chatThumbLabel) {
                        m_chatThumbLabel->setPixmap(pix.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
                    if (m_chatThumbTexto) {
                        m_chatThumbTexto->setText(QString("Circuito capturado (%1x%2 px)").arg(rect.width()).arg(rect.height()));
                    }
                    if (m_chatPreviewWidget) {
                        m_chatPreviewWidget->setVisible(true);
                    }
                    onTogglePainelLateral(true);
                    m_tabWidget->setCurrentIndex(2); // Aba do Chat IA
                    if (m_chatInput) {
                        m_chatInput->setFocus();
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

void MainWindow::forcarNavegacaoPagina(int pagina)
{
    if (!m_pdfDoc || m_pdfDoc->pageCount() == 0) return;
    const int total = m_pdfDoc->pageCount();
    pagina = qBound(0, pagina, total - 1);

    m_bloquearSyncPagina = true;
    m_paginaAtual = pagina;

    // A técnica do salto intermediário força o QPdfPageNavigator a recalcular
    // o scrollbar interno mesmo quando zoom ou setPageMode invalidaram o layout
    if (total > 1) {
        const int dummy = (pagina == 0 ? 1 : 0);
        m_pdfView->pageNavigator()->jump(dummy, {}, 0);
    }
    m_pdfView->pageNavigator()->jump(pagina, {}, 0);

    if (m_modoVis == ModoVisualizacao::DuasPaginas) {
        if (pagina + 1 < total) {
            if (total > 2) {
                const int dummy2 = (pagina + 1 == 0 ? 1 : 0);
                m_pdfView2->pageNavigator()->jump(dummy2, {}, 0);
            }
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
    const int pagAlvo = m_paginaAtual;
    m_modoVis = modo;
    m_bloquearSyncPagina = true;

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

    forcarNavegacaoPagina(pagAlvo);
    QTimer::singleShot(60, this, [this, pagAlvo]() {
        forcarNavegacaoPagina(pagAlvo);
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
    m_btnPagAnterior->setEnabled(m_paginaAtual > 0);
    m_btnPagProxima->setEnabled(m_paginaAtual < total - 1);

    m_spinPagina->blockSignals(true);
    m_spinPagina->setValue(m_paginaAtual + 1);
    m_spinPagina->blockSignals(false);
}

void MainWindow::aplicarZoom(qreal fator)
{
    fator = qBound(0.2, fator, 4.0);
    const int pagAlvo = m_paginaAtual;
    m_bloquearSyncPagina = true;

    m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
    m_pdfView->setZoomFactor(fator);

    if (m_pdfView2->isVisible()) {
        m_pdfView2->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView2->setZoomFactor(fator);
    }
    atualizarLabelZoom();

    forcarNavegacaoPagina(pagAlvo);
    QTimer::singleShot(60, this, [this, pagAlvo]() {
        forcarNavegacaoPagina(pagAlvo);
    });
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
    const int pagAlvo = m_paginaAtual;
    m_bloquearSyncPagina = true;

    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    if (m_pdfView2->isVisible()) m_pdfView2->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    atualizarLabelZoom();

    forcarNavegacaoPagina(pagAlvo);
    QTimer::singleShot(60, this, [this, pagAlvo]() {
        forcarNavegacaoPagina(pagAlvo);
    });
}

void MainWindow::onAjustarPagina()
{
    const int pagAlvo = m_paginaAtual;
    m_bloquearSyncPagina = true;

    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    if (m_pdfView2->isVisible()) m_pdfView2->setZoomMode(QPdfView::ZoomMode::FitInView);
    atualizarLabelZoom();

    forcarNavegacaoPagina(pagAlvo);
    QTimer::singleShot(60, this, [this, pagAlvo]() {
        forcarNavegacaoPagina(pagAlvo);
    });
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
    } else {
        if (m_lblStatusGeral) m_lblStatusGeral->setText("Motor offline (inicie ./iniciar_backend.sh)");
        m_lblStatus->setText("Motor offline (inicie ./iniciar_backend.sh)");
    }
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
        setButtonBusy(m_btnChatEnviar, true);
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
    if (endpoint == "/chat_ia")  setButtonBusy(m_btnChatEnviar, false);

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
        setButtonBusy(m_btnChatEnviar, false);
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
        if (m_chatThumbLabel) {
            m_chatThumbLabel->setPixmap(pix.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        if (m_chatThumbTexto) {
            m_chatThumbTexto->setText(QFileInfo(arq).fileName());
        }
        if (m_chatPreviewWidget) {
            m_chatPreviewWidget->setVisible(true);
        }
        appendLog("Imagem anexada: " + QFileInfo(arq).fileName(), "success");
    }
}

void MainWindow::onRemoverImagemChat()
{
    m_chatImagemBase64.clear();
    if (m_chatPreviewWidget) {
        m_chatPreviewWidget->setVisible(false);
    }
    if (m_chatThumbLabel) {
        m_chatThumbLabel->clear();
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

    if (m_chatInput) {
        QString atual = m_chatInput->toPlainText().trimmed();
        if (!atual.isEmpty()) atual += "\n\n";
        atual += QString("Explicar este trecho: \"%1\"").arg(trecho);
        m_chatInput->setPlainText(atual);
        m_chatInput->setFocus();
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
    const QString texto = m_chatInput ? m_chatInput->toPlainText().trimmed() : QString();
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
    m_chatInput->clear();

    setButtonBusy(m_btnChatEnviar, true);
    m_lblStatus->setText("Consultando Assistente IA...");

    m_net->enviarMensagemChat(texto, imgB64, m_chatHistoricoJson, m_iaProvedor, m_iaApiKey, m_iaModelo);
}

void MainWindow::onChatRespostaResultado(const QString &resposta, const QString &provedor, const QString &modelo)
{
    setButtonBusy(m_btnChatEnviar, false);
    m_lblStatus->setText("Pronto");

    // Salva no histórico JSON
    QJsonObject botMsg;
    botMsg["role"] = "assistant";
    botMsg["content"] = resposta;
    m_chatHistoricoJson.append(botMsg);

    // Formata resposta em HTML com suporte a Markdown básico (código, negrito, itálico)
    QString formatted = resposta.toHtmlEscaped();
    static const QRegularExpression reCode("`([^`]+)`");
    formatted.replace(reCode, "<code style='background-color: #272a33; color: #38bdf8; padding: 2px 5px; border-radius: 4px; font-family: monospace;'>\\1</code>");
    static const QRegularExpression reBold("\\*\\*([^*]+)\\*\\*");
    formatted.replace(reBold, "<strong style='color: #f8fafc;'>\\1</strong>");
    static const QRegularExpression reItalic("(?<!\\*)\\*([^*\\n]+)\\*(?!\\*)");
    formatted.replace(reItalic, "<em>\\1</em>");
    formatted.replace("\n\n", "<br><br>");
    formatted.replace("\n", "<br>");

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
    comboModelo->addItem("gemini-3.8-flash", "gemini-3.8-flash");
    comboModelo->addItem("gemini-3.8-flash-lite", "gemini-3.8-flash-lite");
    const int idxMod = comboModelo->findData(m_iaModelo);
    if (idxMod >= 0) comboModelo->setCurrentIndex(idxMod);
    form->addRow("Modelo:", comboModelo);

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

