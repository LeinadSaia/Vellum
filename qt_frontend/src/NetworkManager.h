#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QByteArray>
#include <QUrl>

/**
 * NetworkManager
 * ──────────────
 * Camada de abstração para toda comunicação HTTP com o backend FastAPI.
 * Todas as chamadas são 100% assíncronas via signals/slots do Qt.
 * A UI nunca congela — os sinais de resultado chegam quando a resposta estiver pronta.
 */
class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(const QString &baseUrl = "http://localhost:8000",
                            QObject *parent = nullptr);

    /**
     * POST /limpar_ocr
     * Envia texto sujo do Tesseract para o Ollama limpar via IA.
     * @param textoSujo  Texto extraído pelo Tesseract (pode ter erros de OCR)
     */
    void limparOcr(const QString &textoSujo);

    /**
     * POST /limpar_ocr  (overload para imagem Base64)
     * Para PDFs escaneados: envia a imagem capturada pelo QRubberBand.
     * Nota: requer que o backend FastAPI aceite "imagem_base64" neste endpoint.
     * @param imagemBase64  PNG capturado da tela, codificado em Base64
     */
    void processarImagemOcr(const QByteArray &imagemBase64);

    /**
     * POST /gravar_e_transcrever
     * Aciona a gravação do microfone no servidor Python.
     * O servidor usa Whisper para retornar o texto falado.
     */
    void gravarETranscrever();

    /**
     * POST /avaliar_traducao
     * Avalia a precisão técnica da tradução. Retorna nota 0-100.
     * @param textoIngles    Frase original em inglês
     * @param textoPortugues Tradução capturada do microfone
     */
    void avaliarTraducao(const QString &textoIngles, const QString &textoPortugues);

    /**
     * GET /
     * Verifica se o servidor Python está rodando.
     */
    void verificarConexao();

    /**
     * POST /iniciar_gravacao
     * Inicia gravação no backend. Retorna imediatamente.
     */
    void iniciarGravacao();

    /**
     * POST /parar_gravacao
     * Para a gravação e aguarda a transcrição do Whisper.
     */
    void pararGravacao();

    /**
     * POST /traduzir
     * Traduz diretamente texto em inglês ou imagem Base64 para português com o Ollama.
     */
    void traduzirDireto(const QString &textoIngles = QString(), const QByteArray &imagemBase64 = QByteArray());

signals:
    /** Emitido quando a tradução direta do Ollama finaliza */
    void traducaoDiretaResultado(const QString &textoIngles, const QString &traducaoPortugues);

    /** Emitido quando o OCR + limpeza da IA finalizam */
    void limparOcrResultado(const QString &textoPronto);

    /** Emitido quando o Whisper transcreve a fala */
    void transcricaoResultado(const QString &textoTranscrito);

    /** Emitido com a nota final (0-100) do Ollama */
    void avaliacaoResultado(int nota);

    /** Status de conexão com o backend */
    void servidorOnline(bool online);

    /** Confirmação de que /iniciar_gravacao foi aceito pelo servidor */
    void gravacaoIniciada();


    /** Notifica que uma requisição foi iniciada (para desabilitar botões na UI) */
    void requisicaoIniciada(const QString &endpoint);

    /** Notifica que uma requisição foi concluída (para reabilitar botões na UI) */
    void requisicaoConcluida(const QString &endpoint);

    /** Emitido em caso de erro de rede ou parsing */
    void erroRequisicao(const QString &endpoint, const QString &mensagem);

private:
    /**
     * Envia um POST com body JSON para o endpoint especificado.
     * Retorna o QNetworkReply para que o caller conecte o sinal finished.
     */
    QNetworkReply* postJson(const QString &endpoint, const QByteArray &jsonBody);

    /** Faz GET em um endpoint */
    QNetworkReply* get(const QString &endpoint);

    /** Extrai o campo texto de uma resposta JSON */
    static QString extrairCampoString(const QByteArray &jsonData, const QString &campo);

    /** Extrai um campo inteiro de uma resposta JSON */
    static int extrairCampoInt(const QByteArray &jsonData, const QString &campo, int fallback = 0);

    QNetworkAccessManager *m_nam;
    QString m_baseUrl;
};
