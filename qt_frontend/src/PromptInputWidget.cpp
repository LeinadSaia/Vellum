#include "PromptInputWidget.h"
#include <QKeyEvent>
#include <QPixmap>
#include <QByteArray>

PromptInputWidget::PromptInputWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void PromptInputWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    // 1. Container de Preview de Imagem (oculto por padrão)
    m_previewContainer = new QWidget(this);
    m_previewContainer->setVisible(false);
    auto* previewLayout = new QHBoxLayout(m_previewContainer);
    previewLayout->setContentsMargins(8, 4, 8, 4);
    
    m_lblThumbImage = new QLabel(this);
    m_lblThumbImage->setFixedSize(40, 40);
    m_lblThumbImage->setScaledContents(true);
    
    m_lblThumbText = new QLabel(this);
    m_lblThumbText->setStyleSheet("color: #888; font-size: 12px;");
    
    m_btnRemoveThumb = new QPushButton("✖", this);
    m_btnRemoveThumb->setFixedSize(24, 24);
    m_btnRemoveThumb->setToolTip("Remover anexo");
    m_btnRemoveThumb->setCursor(Qt::PointingHandCursor);
    m_btnRemoveThumb->setStyleSheet("QPushButton { border: none; border-radius: 12px; background: #e81123; color: white; }"
                                    "QPushButton:hover { background: #f1707a; }");
    
    previewLayout->addWidget(m_lblThumbImage);
    previewLayout->addWidget(m_lblThumbText);
    previewLayout->addStretch();
    previewLayout->addWidget(m_btnRemoveThumb);

    // 2. Caixa de Texto
    m_textInput = new QTextEdit(this);
    m_textInput->setPlaceholderText("Digite seu comando aqui... (Shift+Enter para enviar)");
    m_textInput->setMinimumHeight(60);
    m_textInput->installEventFilter(this); // Para capturar Shift+Enter

    // 3. Barra de Ferramentas (Botões)
    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    
    m_btnAttachImage = new QPushButton("📷 Anexar", this);
    m_btnCaptureCircuit = new QPushButton("✂ Capturar Área", this);
    m_btnPasteSnippet = new QPushButton("📋 Colar Trecho", this);
    m_btnSend = new QPushButton("🚀 Enviar", this);
    
    // Estilos temporários inline (idealmente virão do ThemeManager no futuro)
    m_btnSend->setStyleSheet("background-color: #0078d4; color: white; font-weight: bold; border-radius: 4px; padding: 6px 12px;");
    m_btnSend->setCursor(Qt::PointingHandCursor);

    toolbarLayout->addWidget(m_btnAttachImage);
    toolbarLayout->addWidget(m_btnCaptureCircuit);
    toolbarLayout->addWidget(m_btnPasteSnippet);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_btnSend);

    m_toolbarWidget = new QWidget(this);
    m_toolbarWidget->setLayout(toolbarLayout);

    mainLayout->addWidget(m_previewContainer);
    mainLayout->addWidget(m_textInput);
    mainLayout->addWidget(m_toolbarWidget);

    // Conexões
    connect(m_btnSend, &QPushButton::clicked, this, [this]() {
        if (!m_textInput->toPlainText().trimmed().isEmpty() || m_hasImage) {
            emit sendRequested(m_textInput->toPlainText());
        }
    });
    connect(m_btnAttachImage, &QPushButton::clicked, this, &PromptInputWidget::attachImageRequested);
    connect(m_btnCaptureCircuit, &QPushButton::clicked, this, &PromptInputWidget::captureCircuitRequested);
    connect(m_btnPasteSnippet, &QPushButton::clicked, this, &PromptInputWidget::pasteSnippetRequested);
    connect(m_btnRemoveThumb, &QPushButton::clicked, this, &PromptInputWidget::removeImageRequested);
}

QString PromptInputWidget::text() const {
    return m_textInput->toPlainText();
}

QString PromptInputWidget::selectedText() const {
    return m_textInput->textCursor().selectedText();
}

void PromptInputWidget::setText(const QString& text) {
    m_textInput->setPlainText(text);
}

void PromptInputWidget::setHtml(const QString& html) {
    m_textInput->setHtml(html);
}

void PromptInputWidget::setReadOnly(bool readOnly) {
    m_textInput->setReadOnly(readOnly);
}

void PromptInputWidget::setPlaceholderText(const QString& text) {
    m_textInput->setPlaceholderText(text);
}

void PromptInputWidget::clearInput() {
    m_textInput->clear();
}

void PromptInputWidget::showImagePreview(const QString& base64, const QString& fileName) {
    QByteArray imgData = QByteArray::fromBase64(base64.toUtf8());
    QPixmap pix;
    pix.loadFromData(imgData);
    
    m_lblThumbImage->setPixmap(pix);
    m_lblThumbText->setText(fileName);
    m_previewContainer->setVisible(true);
    m_hasImage = true;
}

void PromptInputWidget::hideImagePreview() {
    m_lblThumbImage->clear();
    m_lblThumbText->clear();
    m_previewContainer->setVisible(false);
    m_hasImage = false;
}

bool PromptInputWidget::hasImage() const {
    return m_hasImage;
}

void PromptInputWidget::setSendButtonBusy(bool busy) {
    if (busy) {
        m_btnSend->setText("⏳ Carregando...");
        m_btnSend->setEnabled(false);
    } else {
        m_btnSend->setText("🚀 Enviar");
        m_btnSend->setEnabled(true);
    }
}

void PromptInputWidget::setButtonsVisible(bool showCapture, bool showAttach) {
    m_btnCaptureCircuit->setVisible(showCapture);
    m_btnAttachImage->setVisible(showAttach);
}

void PromptInputWidget::setToolbarVisible(bool visible) {
    m_toolbarWidget->setVisible(visible);
}

void PromptInputWidget::setSendButtonText(const QString& text) {
    m_btnSend->setText(text);
}

bool PromptInputWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_textInput && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                // Shift+Enter envia a mensagem
                m_btnSend->click();
                return true; // Consome o evento
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
