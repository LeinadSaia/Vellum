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

class NetworkManager;

enum class ModoVisualizacao {
    UmaPagina,     // 1 página por vez
    DuasPaginas,   // 2 páginas lado a lado (modo livro)
    Continuo       // Rolagem contínua vertical
};

/**
 * MainWindow
 * ──────────
 * Leitor e Tradutor Técnico de Documentos PDF.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    // ── Ações Principais ──────────────────────────────────────────────────
    void onAbrirPdf();
    void onFecharPdf();
    void onToggleModoCaptura(bool ativo);
    void onTraduzirDireto();

    // ── Módulo Opcional de Treino ─────────────────────────────────────────
    void onToggleModuloTreino(bool visivel);
    void onGravarInterpretacao();
    void onLimparOcr();

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

    // ── Respostas do NetworkManager ───────────────────────────────────────
    void onTraducaoDiretaResultado(const QString &textoIngles, const QString &traducaoPortugues);
    void onLimparOcrResultado(const QString &texto);
    void onTranscricaoResultado(const QString &texto);
    void onAvaliacaoResultado(int nota);
    void onServidorOnline(bool online);
    void onRequisicaoIniciada(const QString &endpoint);
    void onRequisicaoConcluida(const QString &endpoint);
    void onErroRequisicao(const QString &endpoint, const QString &mensagem);
    void onGravacaoIniciada();

private:
    // ── Setup ─────────────────────────────────────────────────────────────
    void setupMenuBar();
    void setupUi();
    QWidget* criarBarraVisualizacao();
    void setupStyleSheet();
    void connectSignals();

    void irParaPagina(int pagina);
    void setModoVisualizacao(ModoVisualizacao modo);
    void aplicarZoom(qreal fator);
    void zoomDelta(qreal delta);
    void atualizarInfoNavegacao();
    void atualizarLabelZoom();

    void capturarRegiaoRubberBand();

    // ── Utilitários de UI ─────────────────────────────────────────────────
    void appendLog(const QString &mensagem, const QString &tipo = "info");
    void setButtonBusy(QPushButton *btn, bool busy);

    // ── Estado interno ────────────────────────────────────────────────────
    bool             m_modoCaptura   = false;
    bool             m_gravando      = false;
    bool             m_panAtivo      = false;
    ModoVisualizacao m_modoVis       = ModoVisualizacao::UmaPagina;
    int              m_paginaAtual   = 0;

    QString          m_textoOcrAtual;
    QString          m_textoOriginalEn;
    QString          m_textoTranscrito;

    QPoint           m_rbOrigin;
    QPoint           m_panOrigin;

    // ── PDF ───────────────────────────────────────────────────────────────
    QPdfDocument    *m_pdfDoc         = nullptr;
    QPdfView        *m_pdfView        = nullptr;
    QPdfView        *m_pdfView2       = nullptr;
    QPdfView        *m_activePdfView  = nullptr;

    // ── Rubber band ───────────────────────────────────────────────────────
    QRubberBand     *m_rubberBand     = nullptr;

    // ── Rede ──────────────────────────────────────────────────────────────
    NetworkManager  *m_net            = nullptr;

    // ── Menus ─────────────────────────────────────────────────────────────
    QAction         *m_actModoTreino  = nullptr;

    // ── Barra Superior de PDF ─────────────────────────────────────────────
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

    // ── Painel Direito: Tradução Direta ───────────────────────────────────
    QPushButton     *m_btnAbrir          = nullptr;
    QPushButton     *m_btnModoCaptura    = nullptr;
    QPushButton     *m_btnTraduzir       = nullptr;
    QCheckBox       *m_chkTraducaoAuto   = nullptr;
    QTextEdit       *m_txtTraducao       = nullptr;

    // ── Seção Opcional: Treino & Pronúncia (Oculta por padrão) ─────────────
    QWidget         *m_containerTreino   = nullptr;
    QPushButton     *m_btnOuvir          = nullptr;
    QPushButton     *m_btnGravar         = nullptr;
    QPushButton     *m_btnLimparOcr      = nullptr;

    QTextEdit       *m_logBox            = nullptr;
    QLabel          *m_lblStatus         = nullptr;
};
