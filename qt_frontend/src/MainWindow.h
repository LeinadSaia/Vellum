#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QRubberBand>
#include <QPdfDocument>
#include <QPdfView>
#include <QPoint>
#include <QString>
#include <QByteArray>
#include <QScrollBar>
#include <QSpinBox>
#include <QCheckBox>
#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QTabWidget>
#include <QComboBox>
#include <QProgressBar>

class NetworkManager;

enum class ModoVisualizacao {
    UmaPagina,     // 1 página por vez
    DuasPaginas,   // 2 páginas lado a lado (modo livro)
    Continuo       // Rolagem contínua vertical
};

/**
 * MainWindow
 * ──────────
 * Leitor e Tradutor Técnico de Documentos PDF com Tutor de Pronúncia Integrado.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    // ── Arquivo e Seleção ─────────────────────────────────────────────────
    void onAbrirPdf();
    void onFecharPdf();
    void onToggleModoCaptura(bool ativo);
    void onTraduzirDireto();
    void onCopiarTraducao();

    // ── Painel e Layout ───────────────────────────────────────────────────
    void onTogglePainelLateral(bool visivel);
    void onMudarAbaPainel(int index);
    void onSalvarLayout();
    void onRestaurarLayoutPadrao();
    void onAplicarProporcao(int leitorPercent);

    // ── Tutor de Pronúncia & Voz ──────────────────────────────────────────
    void onOuvirPronuncia();
    void onGravarVozTutor();
    void onFalaIniciada(const QString &voz);
    void onAvaliacaoPronunciaResultado(int nota, const QString &feedback, const QStringList &palavrasAusentes);

    // ── Navegação e Modos de Visualização ─────────────────────────────────
    void onPaginaAnterior();
    void onPaginaProxima();
    void onPaginaSpinChanged(int pag);
    void onModo1PagClicado();
    void onModo2PagClicado();
    void onModoContClicado();

    // ── Controles de Zoom ─────────────────────────────────────────────────
    void onZoomMenos();
    void onZoomMais();
    void onAjustarLargura();
    void onAjustarPagina();
    void onZoomReset();

    // ── Respostas de Rede ─────────────────────────────────────────────────
    void onTraducaoDiretaResultado(const QString &textoIngles, const QString &traducaoPortugues);
    void onLimparOcrResultado(const QString &texto);
    void onTranscricaoResultado(const QString &texto);
    void onServidorOnline(bool online);
    void onRequisicaoIniciada(const QString &endpoint);
    void onRequisicaoConcluida(const QString &endpoint);
    void onErroRequisicao(const QString &endpoint, const QString &mensagem);
    void onGravacaoIniciada();

    // ── Configurações e Desempenho ────────────────────────────────────────
    void onPerfilAlterado(int index);

private:
    void setupMenuBar();
    void setupUi();
    QWidget* criarBarraVisualizacao();
    QWidget* criarAbaTraducao();
    QWidget* criarAbaTutor();
    QWidget* criarAbaConfiguracoes();
    void setupStyleSheet();
    void connectSignals();

    void carregarConfiguracoes();
    void salvarConfiguracoes();

    void irParaPagina(int pagina);
    void setModoVisualizacao(ModoVisualizacao modo);
    void aplicarZoom(qreal fator);
    void zoomDelta(qreal delta);
    void atualizarInfoNavegacao();
    void atualizarLabelZoom();

    void capturarRegiaoRubberBand();

    void appendLog(const QString &mensagem, const QString &tipo = "info");
    void setButtonBusy(QPushButton *btn, bool busy);

    // ── Estado interno ────────────────────────────────────────────────────
    bool             m_modoCaptura   = false;
    bool             m_gravando      = false;
    bool             m_panAtivo      = false;
    bool             m_ajustandoZoom = false;
    ModoVisualizacao m_modoVis       = ModoVisualizacao::UmaPagina;
    int              m_paginaAtual   = 0;

    QString          m_textoOcrAtual;
    QString          m_textoOriginalEn;
    QString          m_textoTraduzidoPt;

    QPoint           m_rbOrigin;
    QPoint           m_panOrigin;

    // ── Layout e Divisor ──────────────────────────────────────────────────
    QSplitter       *m_splitter       = nullptr;
    QWidget         *m_painelEsq      = nullptr;
    QFrame          *m_painelDir      = nullptr;
    QTabWidget      *m_tabWidget      = nullptr;
    QWidget         *m_barraVis       = nullptr;

    // ── PDF ───────────────────────────────────────────────────────────────
    QPdfDocument    *m_pdfDoc         = nullptr;
    QPdfView        *m_pdfView        = nullptr;
    QPdfView        *m_pdfView2       = nullptr;
    QPdfView        *m_activePdfView  = nullptr;
    QRubberBand     *m_rubberBand     = nullptr;

    // ── Rede ──────────────────────────────────────────────────────────────
    NetworkManager  *m_net            = nullptr;

    // ── Ações do Menu ─────────────────────────────────────────────────────
    QAction         *m_actTogglePainel = nullptr;
    QAction         *m_actBarraLeitor  = nullptr;

    // ── Barra Superior de Navegação ───────────────────────────────────────
    QPushButton     *m_btnPagAnterior = nullptr;
    QPushButton     *m_btnPagProxima  = nullptr;
    QSpinBox        *m_spinPagina     = nullptr;
    QLabel          *m_lblTotalPag    = nullptr;

    QPushButton     *m_btnModo1Pag    = nullptr;
    QPushButton     *m_btnModo2Pag    = nullptr;
    QPushButton     *m_btnModoCont    = nullptr;

    QPushButton     *m_btnZoomMenos   = nullptr;
    QPushButton     *m_btnZoomMais    = nullptr;
    QPushButton     *m_btnZoomReset   = nullptr;
    QPushButton     *m_btnAjustarLarg = nullptr;
    QPushButton     *m_btnAjustarPag  = nullptr;

    // ── Aba 1: Tradução ───────────────────────────────────────────────────
    QPushButton     *m_btnModoCaptura    = nullptr;
    QPushButton     *m_btnTraduzir       = nullptr;
    QPushButton     *m_btnCopiarTraducao = nullptr;
    QCheckBox       *m_chkTraducaoAuto   = nullptr;
    QTextEdit       *m_txtTraducao       = nullptr;

    // ── Aba 2: Tutor & Pronúncia ──────────────────────────────────────────
    QComboBox       *m_comboVoz          = nullptr;
    QPushButton     *m_btnOuvir          = nullptr;
    QPushButton     *m_btnGravar         = nullptr;
    QProgressBar    *m_barAcuracia       = nullptr;
    QLabel          *m_lblAcuracia       = nullptr;
    QTextEdit       *m_txtFeedbackTutor  = nullptr;

    // ── Aba 3: Desempenho & Layout ────────────────────────────────────────
    QComboBox       *m_comboPerfil       = nullptr;
    QPushButton     *m_btnSalvarLayout   = nullptr;
    QPushButton     *m_btnRestaurarLayout= nullptr;
    QPushButton     *m_btnProporcao100   = nullptr;
    QPushButton     *m_btnProporcao70    = nullptr;
    QPushButton     *m_btnProporcao50    = nullptr;
    QTextEdit       *m_logBox            = nullptr;
    QLabel          *m_lblStatus         = nullptr;
};
