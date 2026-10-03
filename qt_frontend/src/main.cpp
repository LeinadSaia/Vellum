#include <QApplication>
#include <QIcon>
#include <QFile>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Vellum");
    app.setApplicationDisplayName("Vellum");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Vellum");
    app.setDesktopFileName("vellum");
    app.setWindowIcon(QIcon(":/app_icon.png"));

    MainWindow w;
    w.setWindowIcon(QIcon(":/app_icon.png"));

    if (argc > 1) {
        const QString arg1 = QString::fromLocal8Bit(argv[1]);
        if (QFile::exists(arg1) && arg1.endsWith(".pdf", Qt::CaseInsensitive)) {
            w.carregarArquivoPdf(arg1);
        }
    }

    w.show();

    return app.exec();
}
