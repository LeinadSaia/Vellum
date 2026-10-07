#ifndef PROMPTINPUTWIDGET_H
#define PROMPTINPUTWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>

/**
 * @brief Widget reutilizável para a área de input de texto e anexos (Chat/Prompt).
 * Isola a lógica visual e os controles da MainWindow.
 */
class PromptInputWidget : public QWidget {
    Q_OBJECT
public:
    explicit PromptInputWidget(QWidget* parent = nullptr);

    // Retorna o texto atual e opcionalmente limpa o campo
    QString text() const;
    QString selectedText() const;
    void setText(const QString& text);
    void setHtml(const QString& html);
    void clearInput();
    void setPlaceholderText(const QString& text);
    void setReadOnly(bool readOnly);

    // Gerenciamento de preview de imagem/anexo
    void showImagePreview(const QString& base64, const QString& fileName = "Imagem anexada");
    void hideImagePreview();
    bool hasImage() const;

    // Controle de estado dos botões
    void setSendButtonBusy(bool busy);
    void setButtonsVisible(bool showCapture, bool showAttach);
    void setToolbarVisible(bool visible);
    void setSendButtonText(const QString& text);

signals:
    void sendRequested(const QString& text);
    void attachImageRequested();
    void removeImageRequested();
    void captureCircuitRequested();
    void pasteSnippetRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();

    QTextEdit* m_textInput;
    QPushButton* m_btnSend;
    QPushButton* m_btnAttachImage;
    QPushButton* m_btnCaptureCircuit;
    QPushButton* m_btnPasteSnippet;
    QWidget* m_toolbarWidget;

    // Área de preview de imagem
    QWidget* m_previewContainer;
    QLabel* m_lblThumbImage;
    QLabel* m_lblThumbText;
    QPushButton* m_btnRemoveThumb;

    bool m_hasImage = false;
};

#endif // PROMPTINPUTWIDGET_H
