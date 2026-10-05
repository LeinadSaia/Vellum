#ifndef DEPENDENCYMANAGER_H
#define DEPENDENCYMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QProgressDialog>
#include <QStringList>

class DependencyManager : public QObject
{
    Q_OBJECT
public:
    explicit DependencyManager(QWidget *parentWidget = nullptr);

    bool checkDependencies() const;
    void startDownload();

signals:
    void finished();
    void error(const QString &msg);

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    void downloadNext();
    void installExternalDependencies();

    QWidget *m_parentWidget;
    QNetworkAccessManager m_net;
    QNetworkReply *m_currentReply = nullptr;
    QFile *m_currentFile = nullptr;
    QProgressDialog *m_progress = nullptr;

    struct DownloadItem {
        QString url;
        QString destPath;
        QString name;
    };
    QList<DownloadItem> m_queue;
    int m_currentIndex = 0;
};

#endif // DEPENDENCYMANAGER_H
