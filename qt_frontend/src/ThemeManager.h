#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QString>
#include <QObject>

class ThemeManager : public QObject {
    Q_OBJECT
public:
    enum class Theme {
        Light,
        Dark,
        System
    };

    explicit ThemeManager(QObject* parent = nullptr);

    // Carrega o tema selecionado
    bool applyTheme(Theme theme);

    // Retorna o tema atual
    Theme currentTheme() const;

signals:
    void themeChanged(Theme newTheme);

private:
    Theme m_currentTheme = Theme::System;
    bool loadQssFile(const QString& filePath);
};

#endif // THEMEMANAGER_H
