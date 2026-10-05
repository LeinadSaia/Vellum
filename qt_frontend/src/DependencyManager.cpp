#include "DependencyManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

DependencyManager::DependencyManager(QWidget *parentWidget)
    : QObject(parentWidget), m_parentWidget(parentWidget)
{
}

bool DependencyManager::checkDependencies() const
{
    QString whisperDir = QDir::homePath() + "/.cache/whisper";
    bool tinyExists = QFile::exists(whisperDir + "/tiny.en.pt");
    bool baseExists = QFile::exists(whisperDir + "/base.en.pt");

#ifdef Q_OS_WIN
    QString tesseractDir = QCoreApplication::applicationDirPath() + "/Tesseract-OCR";
    bool tesseractExists = QFile::exists(tesseractDir + "/tesseract.exe") && QFile::exists(tesseractDir + "/tessdata/por.traineddata");
    return tinyExists && baseExists && tesseractExists;
#else
    return tinyExists && baseExists;
#endif
}

void DependencyManager::startDownload()
{
    QString whisperDir = QDir::homePath() + "/.cache/whisper";
    QDir().mkpath(whisperDir);

    m_queue.clear();

    if (!QFile::exists(whisperDir + "/tiny.en.pt")) {
        m_queue.append({
            "https://openaipublic.azureedge.net/main/whisper/models/d3dd57d32accea0b295c96e26691aa14d8822fac7d9d27d5dc00ba0adc22e8dd/tiny.en.pt",
            whisperDir + "/tiny.en.pt",
            "Modelo Whisper (tiny.en)"
        });
    }
    if (!QFile::exists(whisperDir + "/base.en.pt")) {
        m_queue.append({
            "https://openaipublic.azureedge.net/main/whisper/models/256150255c601dbb170ef89248a8458ad4cf37081fa97e5b2eb5de50d1da676e/base.en.pt",
            whisperDir + "/base.en.pt",
            "Modelo Whisper (base.en)"
        });
    }

#ifdef Q_OS_WIN
    QString tesseractDir = QCoreApplication::applicationDirPath() + "/Tesseract-OCR";
    if (!QFile::exists(tesseractDir + "/tesseract.exe")) {
        m_queue.append({
            "https://digi.bib.uni-mannheim.de/tesseract/tesseract-ocr-w64-setup-5.5.0.20241111.exe",
            QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/vellum-tesseract-setup.exe",
            "Instalador do Tesseract OCR"
        });
    }
    if (!QFile::exists(tesseractDir + "/tessdata/por.traineddata")) {
        QDir().mkpath(tesseractDir + "/tessdata");
        m_queue.append({
            "https://github.com/tesseract-ocr/tessdata_fast/raw/main/por.traineddata",
            tesseractDir + "/tessdata/por.traineddata",
            "Idioma OCR (Português)"
        });
    }
#endif

    if (m_queue.isEmpty()) {
        emit finished();
        return;
    }

    m_progress = new QProgressDialog("Preparando download...", "Cancelar", 0, 100, m_parentWidget);
    m_progress->setWindowTitle("Dependências Iniciais");
    m_progress->setWindowModality(Qt::ApplicationModal);
    m_progress->setAutoClose(false);
    m_progress->setAutoReset(false);
    m_progress->show();

    connect(m_progress, &QProgressDialog::canceled, this, [this]() {
        if (m_currentReply) m_currentReply->abort();
        emit error("Download cancelado pelo usuário. O Vellum não pode iniciar sem as dependências.");
    });

    m_currentIndex = 0;
    downloadNext();
}

void DependencyManager::downloadNext()
{
    if (m_currentIndex >= m_queue.size()) {
        if (m_progress) {
            m_progress->close();
            m_progress->deleteLater();
            m_progress = nullptr;
        }

#ifdef Q_OS_WIN
        // Após todos os downloads, instalar o tesseract se o instalador foi baixado
        installTesseractWindows();
#else
        emit finished();
#endif
        return;
    }

    DownloadItem &item = m_queue[m_currentIndex];
    m_progress->setLabelText(QString("Baixando: %1\n(%2 de %3)")
                             .arg(item.name)
                             .arg(m_currentIndex + 1)
                             .arg(m_queue.size()));
    m_progress->setValue(0);

    m_currentFile = new QFile(item.destPath);
    if (!m_currentFile->open(QIODevice::WriteOnly)) {
        emit error("Erro ao criar arquivo para download: " + item.destPath);
        return;
    }

    QNetworkRequest request((QUrl(item.url)));
    // Follow redirects
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_currentReply = m_net.get(request);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &DependencyManager::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &DependencyManager::onDownloadFinished);
}

void DependencyManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0 && m_progress) {
        int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progress->setValue(percent);
    }
}

void DependencyManager::onDownloadFinished()
{
    if (!m_currentReply || !m_currentFile) return;

    if (m_currentReply->error() != QNetworkReply::NoError) {
        if (m_currentReply->error() != QNetworkReply::OperationCanceledError) {
            emit error("Falha no download: " + m_currentReply->errorString());
        }
        m_currentFile->close();
        m_currentFile->remove();
        m_currentFile->deleteLater();
        m_currentFile = nullptr;
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    m_currentFile->write(m_currentReply->readAll());
    m_currentFile->close();
    m_currentFile->deleteLater();
    m_currentFile = nullptr;

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    m_currentIndex++;
    downloadNext();
}

void DependencyManager::installTesseractWindows()
{
#ifdef Q_OS_WIN
    QString setupPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/vellum-tesseract-setup.exe";
    QString destDir = QCoreApplication::applicationDirPath() + "/Tesseract-OCR";

    if (QFile::exists(setupPath)) {
        if (m_progress) {
            m_progress->setLabelText("Instalando Tesseract OCR (Requer permissão de Administrador)...");
            m_progress->setRange(0, 0); // Indeterminate
            m_progress->show();
        }

        // The setup is an NSIS installer, so /S is silent, /D=Dir sets the directory
        QString args = QString("/S /D=%1").arg(QDir::toNativeSeparators(destDir));
        
        SHELLEXECUTEINFOW shExecInfo = {0};
        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
        shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        shExecInfo.hwnd = NULL;
        shExecInfo.lpVerb = L"runas";
        shExecInfo.lpFile = (LPCWSTR)setupPath.utf16();
        shExecInfo.lpParameters = (LPCWSTR)args.utf16();
        shExecInfo.lpDirectory = NULL;
        shExecInfo.nShow = SW_HIDE;
        shExecInfo.hInstApp = NULL;

        if (ShellExecuteExW(&shExecInfo)) {
            WaitForSingleObject(shExecInfo.hProcess, INFINITE);
            CloseHandle(shExecInfo.hProcess);
        } else {
            // failed to elevate or run
        }

        QFile::remove(setupPath);
    }
#endif
    
    emit finished();
}
