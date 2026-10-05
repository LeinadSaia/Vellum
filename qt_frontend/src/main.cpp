#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QSplashScreen>
#include <QTimer>
#include "MainWindow.h"

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
    splash.showMessage("Iniciando Motor de IA...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    app.processEvents();

    MainWindow w;
    w.setWindowIcon(QIcon(":/app_icon.png"));

    if (argc > 1) {
        const QString arg1 = QString::fromLocal8Bit(argv[1]);
        if (QFile::exists(arg1) && arg1.endsWith(".pdf", Qt::CaseInsensitive)) {
            w.carregarArquivoPdf(arg1);
        }
    }

    // Fecha a splash e abre a janela quando o backend estiver pronto
    QObject::connect(&w, &MainWindow::backendPronto, [&w, &splash]() {
        if (splash.isVisible()) {
            splash.finish(&w);
            w.show();
        }
    });

    // Fallback de segurança: se o backend demorar mais de 10s, mostra a interface mesmo assim
    QTimer::singleShot(10000, [&w, &splash]() {
        if (splash.isVisible()) {
            splash.showMessage("Demorando mais que o esperado...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);
            splash.finish(&w);
            w.show();
        }
    });

    return app.exec();
}
