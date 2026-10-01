#include "NetworkManager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

// ═════════════════════════════════════════════════════════════════════════════
// Construtor
// ═════════════════════════════════════════════════════════════════════════════

NetworkManager::NetworkManager(const QString &baseUrl, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_baseUrl(baseUrl)
{
}

// ═════════════════════════════════════════════════════════════════════════════
// Auxiliares Internos
// ═════════════════════════════════════════════════════════════════════════════

QNetworkReply* NetworkManager::postJson(const QString &endpoint, const QByteArray &jsonBody)
{
    QNetworkRequest req(QUrl(m_baseUrl + endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    return m_nam->post(req, jsonBody);
}

QNetworkReply* NetworkManager::get(const QString &endpoint)
{
    QNetworkRequest req(QUrl(m_baseUrl + endpoint));
    req.setRawHeader("Accept", "application/json");
    return m_nam->get(req);
}

QString NetworkManager::extrairCampoString(const QByteArray &jsonData, const QString &campo)
{
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(jsonData, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    return doc.object().value(campo).toString();
}

int NetworkManager::extrairCampoInt(const QByteArray &jsonData, const QString &campo, int fallback)
{
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(jsonData, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return fallback;
    return doc.object().value(campo).toInt(fallback);
}

QStringList NetworkManager::extrairCampoListaString(const QByteArray &jsonData, const QString &campo)
{
    QStringList lista;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(jsonData, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return lista;
    const auto array = doc.object().value(campo).toArray();
    for (const auto &val : array) {
        lista.append(val.toString());
    }
    return lista;
}

// ═════════════════════════════════════════════════════════════════════════════
// Métodos Públicos
// ═════════════════════════════════════════════════════════════════════════════

void NetworkManager::verificarConexao()
{
    auto *reply = get("/");
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const bool ok = (reply->error() == QNetworkReply::NoError);
        emit servidorOnline(ok);
        reply->deleteLater();
    });
}

void NetworkManager::limparOcr(const QString &textoSujo, const QString &apiKey)
{
    const QString endpoint = "/limpar_ocr";
    emit requisicaoIniciada(endpoint);

    QJsonObject body;
    body["texto_sujo"] = textoSujo;
    if (!apiKey.isEmpty()) body["api_key"] = apiKey;
    const QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);

    auto *reply = postJson(endpoint, jsonBody);

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        emit requisicaoConcluida(endpoint);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit erroRequisicao(endpoint,
                QStringLiteral("Erro de rede: %1").arg(reply->errorString()));
            return;
        }

        const QString texto = extrairCampoString(reply->readAll(), "resultado");
        if (texto.isEmpty()) {
            emit erroRequisicao(endpoint, "Resposta do servidor veio vazia ou mal formada.");
            return;
        }
        emit limparOcrResultado(texto);
    });
}

void NetworkManager::processarImagemOcr(const QByteArray &imagemBase64, const QString &apiKey)
{
    const QString endpoint = "/limpar_ocr";
    emit requisicaoIniciada(endpoint);

    QJsonObject body;
    body["imagem_base64"] = QString::fromLatin1(imagemBase64);
    if (!apiKey.isEmpty()) body["api_key"] = apiKey;
    const QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);

    auto *reply = postJson(endpoint, jsonBody);

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        emit requisicaoConcluida(endpoint);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit erroRequisicao(endpoint,
                QStringLiteral("Erro ao processar imagem OCR: %1").arg(reply->errorString()));
            return;
        }

        const QString texto = extrairCampoString(reply->readAll(), "resultado");
        emit limparOcrResultado(texto.isEmpty() ? QStringLiteral("(Sem texto detectado)") : texto);
    });
}

void NetworkManager::gravarETranscrever()
{
    const QString endpoint = "/gravar_e_transcrever";
    emit requisicaoIniciada(endpoint);

    auto *reply = postJson(endpoint, QByteArrayLiteral("{}"));

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        emit requisicaoConcluida(endpoint);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit erroRequisicao(endpoint,
                QStringLiteral("Erro na gravação: %1").arg(reply->errorString()));
            return;
        }

        const QString texto = extrairCampoString(reply->readAll(), "texto_transcrito");
        if (texto.isEmpty()) {
            emit erroRequisicao(endpoint, "Whisper não detectou nenhuma fala.");
            return;
        }
        emit transcricaoResultado(texto);
    });
}

void NetworkManager::iniciarGravacao()
{
    const QString endpoint = "/iniciar_gravacao";
    emit requisicaoIniciada(endpoint);

    auto *reply = postJson(endpoint, QByteArrayLiteral("{}"));

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        emit requisicaoConcluida(endpoint);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit erroRequisicao(endpoint,
                QStringLiteral("Erro ao iniciar gravação: %1").arg(reply->errorString()));
            return;
        }
        emit gravacaoIniciada();
    });
}

void NetworkManager::pararGravacao(const QString &idioma)
{
    const QString endpoint = "/parar_gravacao?idioma=" + idioma;
    emit requisicaoIniciada(endpoint);

    auto *reply = postJson(endpoint, QByteArrayLiteral("{}"));

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        emit requisicaoConcluida(endpoint);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit erroRequisicao(endpoint,
                QStringLiteral("Erro ao parar gravação: %1").arg(reply->errorString()));
            return;
        }

        const QString texto = extrairCampoString(reply->readAll(), "texto_transcrito");
        if (texto.isEmpty()) {
            emit erroRequisicao(endpoint, "Whisper não detectou nenhuma fala.");
            return;
        }
        emit transcricaoResultado(texto);
    });
}

void NetworkManager::avaliarTraducao(const QString &textoIngles, const QString &textoPortugues)
{
    const QString endpoint = "/avaliar_traducao";
    emit requisicaoIniciada(endpoint);

    QJsonObject body;
    body["texto_ingles"]    = textoIngles;
    body["texto_portugues"] = textoPortugues;
    const QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);

    auto *reply = postJson(endpoint, jsonBody);

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        emit requisicaoConcluida(endpoint);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit erroRequisicao(endpoint,
                QStringLiteral("Erro na avaliação: %1").arg(reply->errorString()));
            return;
        }

        const int nota = extrairCampoInt(reply->readAll(), "nota", -1);
        emit avaliacaoResultado(nota);
    });
}

void NetworkManager::traduzirDireto(const QString &textoIngles, const QByteArray &imagemBase64, const QString &apiKey)
{
    const QString endpoint = "/traduzir";
    emit requisicaoIniciada(endpoint);

    QJsonObject body;
    if (!textoIngles.isEmpty())
        body["texto_ingles"] = textoIngles;
    if (!imagemBase64.isEmpty())
        body["imagem_base64"] = QString::fromLatin1(imagemBase64);
    if (!apiKey.isEmpty())
        body["api_key"] = apiKey;

    const QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto *reply = postJson(endpoint, jsonBody);

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        emit requisicaoConcluida(endpoint);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit erroRequisicao(endpoint,
                QStringLiteral("Erro ao traduzir: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray respData = reply->readAll();
        const QString textoEn = extrairCampoString(respData, "texto_ingles");
        const QString textoPt = extrairCampoString(respData, "traducao_portugues");

        emit traducaoDiretaResultado(textoEn, textoPt);
    });
}

void NetworkManager::falarTexto(const QString &texto, const QString &voz, const QString &velocidade)
{
    const QString endpoint = "/falar";
    emit requisicaoIniciada(endpoint);

    QJsonObject body;
    body["texto"] = texto;
    body["voz"] = voz;
    body["velocidade"] = velocidade;
    const QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);

    auto *reply = postJson(endpoint, jsonBody);

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint, voz]() {
        emit requisicaoConcluida(endpoint);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit erroRequisicao(endpoint,
                QStringLiteral("Erro na síntese de voz: %1").arg(reply->errorString()));
            return;
        }

        emit falaIniciada(voz);
    });
}

void NetworkManager::pararAudio()
{
    auto *reply = postJson("/parar_audio", QByteArrayLiteral("{}"));
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void NetworkManager::avaliarPronuncia(const QString &textoEsperado, const QString &textoFalado, const QString &nivel)
{
    const QString endpoint = "/avaliar_pronuncia";
    emit requisicaoIniciada(endpoint);

    QJsonObject body;
    body["texto_esperado"] = textoEsperado;
    body["texto_falado"]   = textoFalado;
    body["nivel"]          = nivel;
    const QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);

    auto *reply = postJson(endpoint, jsonBody);

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        emit requisicaoConcluida(endpoint);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit erroRequisicao(endpoint,
                QStringLiteral("Erro na avaliação de pronúncia: %1").arg(reply->errorString()));
            return;
        }

        const QByteArray resp = reply->readAll();
        const int nota = extrairCampoInt(resp, "nota", 0);
        const QString feedback = extrairCampoString(resp, "feedback");
        const QString falado = extrairCampoString(resp, "texto_falado");
        const QStringList ausentes = extrairCampoListaString(resp, "palavras_ausentes");

        emit avaliacaoPronunciaResultado(nota, feedback, falado, ausentes);
    });
}

void NetworkManager::enviarMensagemChat(const QString &mensagem, 
                                        const QString &imagemBase64, 
                                        const QJsonArray &historico, 
                                        const QString &provedor, 
                                        const QString &apiKey, 
                                        const QString &modelo)
{
    const QString endpoint = "/chat_ia";
    emit requisicaoIniciada(endpoint);

    QJsonObject body;
    body["mensagem"] = mensagem;
    if (!imagemBase64.isEmpty()) {
        body["imagem_base64"] = imagemBase64;
    }
    if (!historico.isEmpty()) {
        body["historico"] = historico;
    }
    body["provedor"] = provedor;
    if (!apiKey.isEmpty()) {
        body["api_key"] = apiKey;
    }
    if (!modelo.isEmpty()) {
        body["modelo"] = modelo;
    }

    const QByteArray jsonBody = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto *reply = postJson(endpoint, jsonBody);

    connect(reply, &QNetworkReply::finished, this, [this, reply, endpoint]() {
        emit requisicaoConcluida(endpoint);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            const QByteArray errResp = reply->readAll();
            QString detalhe = extrairCampoString(errResp, "detail");
            if (detalhe.isEmpty()) {
                detalhe = reply->errorString();
            }
            emit erroRequisicao(endpoint, QStringLiteral("Erro no Assistente IA: %1").arg(detalhe));
            return;
        }

        const QByteArray resp = reply->readAll();
        const QString resposta = extrairCampoString(resp, "resposta");
        const QString prov     = extrairCampoString(resp, "provedor_usado");
        const QString mod      = extrairCampoString(resp, "modelo_usado");

        emit chatRespostaResultado(resposta, prov, mod);
    });
}

