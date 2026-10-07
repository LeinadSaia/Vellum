#include "ThemeManager.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QStyleHints>
#include <QGuiApplication>
#include <QDebug>

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
    // Escutar mudanças de tema do sistema operacional se necessário
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme colorScheme) {
        if (m_currentTheme == Theme::System) {
            applyTheme(Theme::System); // Reaplica o tema correto baseado no sistema
        }
    });
}

bool ThemeManager::applyTheme(Theme theme) {
    m_currentTheme = theme;
    Theme effectiveTheme = theme;

    if (effectiveTheme == Theme::System) {
        if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark) {
            effectiveTheme = Theme::Dark;
        } else {
            effectiveTheme = Theme::Light;
        }
    }

    QString qssPath = (effectiveTheme == Theme::Dark) 
                      ? ":/themes/dark.qss" 
                      : ":/themes/light.qss";

    if (loadQssFile(qssPath)) {
        emit themeChanged(theme);
        return true;
    }
    return false;
}

ThemeManager::Theme ThemeManager::currentTheme() const {
    return m_currentTheme;
}

bool ThemeManager::loadQssFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        qWarning() << "Falha ao abrir arquivo QSS:" << filePath;
        return false;
    }
    QTextStream stream(&file);
    qApp->setStyleSheet(stream.readAll());
    file.close();
    return true;
}
