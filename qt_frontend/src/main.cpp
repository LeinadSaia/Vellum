#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QSplashScreen>
#include <QTimer>
#include "MainWindow.h"
#include "DependencyManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Vellum");
    app.setApplicationDisplayName("Vellum");
    app.setApplicationVersion("1.0.1");
    app.setOrganizationName("Vellum");
    app.setDesktopFileName("vellum");
    app.setWindowIcon(QIcon(":/app_icon.png"));

    QSplashScreen splash(QPixmap(":/app_icon.png").scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    splash.show();
    splash.showMessage("Iniciando Vellum...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    app.processEvents();

    auto startApp = [argc, argv, &splash]() {
        MainWindow *w = new MainWindow();
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->setWindowIcon(QIcon(":/app_icon.png"));

        if (argc > 1) {
            const QString arg1 = QString::fromLocal8Bit(argv[1]);
            if (QFile::exists(arg1) && arg1.endsWith(".pdf", Qt::CaseInsensitive)) {
                w->carregarArquivoPdf(arg1);
            }
        }

        splash.showMessage("Iniciando Motor de IA...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);

        QObject::connect(w, &MainWindow::backendPronto, [w, &splash]() {
            if (splash.isVisible()) {
                splash.finish(w);
                w->show();
            }
        });

        QTimer::singleShot(15000, [w, &splash]() {
            if (splash.isVisible()) {
                splash.showMessage("Demorando mais que o esperado...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
                splash.finish(w);
                w->show();
            }
        });
    };

    DependencyManager *depManager = new DependencyManager();
    if (!depManager->checkDependencies()) {
        splash.showMessage("Aguardando download de dependências...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
        QObject::connect(depManager, &DependencyManager::finished, [depManager, startApp]() {
            depManager->deleteLater();
            startApp();
        });
        QObject::connect(depManager, &DependencyManager::error, [depManager, startApp](const QString &msg) {
            qWarning() << "Erro no download:" << msg;
            depManager->deleteLater();
            startApp(); // Tenta subir mesmo com erro, o Vellum lidará com funcionalidades faltantes
        });
        depManager->startDownload();
    } else {
        depManager->deleteLater();
        startApp();
    }

    return app.exec();
}
