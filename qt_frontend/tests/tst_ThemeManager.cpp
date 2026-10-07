#include <QtTest>
#include <QApplication>
#include "../src/ThemeManager.h"

class TestThemeManager : public QObject {
    Q_OBJECT

private slots:
    void testInitialTheme() {
        ThemeManager manager;
        // O padrão deve ser Theme::System
        QCOMPARE(manager.currentTheme(), ThemeManager::Theme::System);
    }

    void testApplyDarkTheme() {
        ThemeManager manager;
        QSignalSpy spy(&manager, &ThemeManager::themeChanged);

        bool success = manager.applyTheme(ThemeManager::Theme::Dark);
        QVERIFY2(success, "Deveria carregar o dark.qss com sucesso");
        QCOMPARE(manager.currentTheme(), ThemeManager::Theme::Dark);
        QCOMPARE(spy.count(), 1);
    }

    void testApplyLightTheme() {
        ThemeManager manager;
        QSignalSpy spy(&manager, &ThemeManager::themeChanged);

        bool success = manager.applyTheme(ThemeManager::Theme::Light);
        QVERIFY2(success, "Deveria carregar o light.qss com sucesso");
        QCOMPARE(manager.currentTheme(), ThemeManager::Theme::Light);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestThemeManager)
#include "tst_ThemeManager.moc"
