#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QUrl>

/**
 * NetworkManager
 * ──────────────
 * Camada de comunicação HTTP assíncrona com o backend FastAPI.
 * Trata OCR, Tradução, TTS Neural, Gravação e Avaliação do Tutor.
 */
class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(const QString &baseUrl = "http://localhost:8000",
                            QObject *parent = nullptr);

    /** POST /limpar_ocr */
    void limparOcr(const QString &textoSujo);

    /** POST /limpar_ocr (imagem base64) */
    void processarImagemOcr(const QByteArray &imagemBase64);

    /** POST /gravar_e_transcrever (legado) */
    void gravarETranscrever();

    /** POST /avaliar_traducao */
    void avaliarTraducao(const QString &textoIngles, const QString &textoPortugues);

    /** GET / (health check) */
    void verificarConexao();

    /** POST /iniciar_gravacao */
    void iniciarGravacao();

    /** POST /parar_gravacao */
    void pararGravacao(const QString &idioma = "en");

    /** POST /traduzir */
    void traduzirDireto(const QString &textoIngles = QString(), const QByteArray &imagemBase64 = QByteArray());

    /** POST /falar (TTS Neural com Edge TTS) */
    void falarTexto(const QString &texto, const QString &voz = "en-US-JennyNeural", const QString &velocidade = "+0%");

    /** POST /parar_audio */
    void pararAudio();

    /** POST /avaliar_pronuncia (Tutor de fala com nível) */
    void avaliarPronuncia(const QString &textoEsperado, const QString &textoFalado, const QString &nivel = "intermediario");

signals:
    void traducaoDiretaResultado(const QString &textoIngles, const QString &traducaoPortugues);
    void limparOcrResultado(const QString &textoPronto);
    void transcricaoResultado(const QString &textoTranscrito);
    void avaliacaoResultado(int nota);
    void servidorOnline(bool online);
    void gravacaoIniciada();

    /** TTS e Tutor */
    void falaIniciada(const QString &voz);
    void avaliacaoPronunciaResultado(int nota, const QString &feedback, const QString &textoFalado, const QStringList &palavrasAusentes);

    void requisicaoIniciada(const QString &endpoint);
    void requisicaoConcluida(const QString &endpoint);
    void erroRequisicao(const QString &endpoint, const QString &mensagem);

private:
    QNetworkReply* postJson(const QString &endpoint, const QByteArray &jsonBody);
    QNetworkReply* get(const QString &endpoint);

    static QString extrairCampoString(const QByteArray &jsonData, const QString &campo);
    static int extrairCampoInt(const QByteArray &jsonData, const QString &campo, int fallback = 0);
    static QStringList extrairCampoListaString(const QByteArray &jsonData, const QString &campo);

    QNetworkAccessManager *m_nam;
    QString m_baseUrl;
};
