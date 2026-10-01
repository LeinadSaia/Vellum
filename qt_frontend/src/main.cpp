#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Ensinador de Inglês");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("IA Local");

    MainWindow w;
    w.show();

    return app.exec();
}
