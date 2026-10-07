#include <QtTest>
#include <QApplication>
#include <QSignalSpy>
#include "../src/PromptInputWidget.h"

class TestPromptInputWidget : public QObject {
    Q_OBJECT

private slots:
    void testInitialState() {
        PromptInputWidget widget;
        QCOMPARE(widget.text(), QString(""));
        QCOMPARE(widget.hasImage(), false);
    }

    void testSetAndClearText() {
        PromptInputWidget widget;
        // Simulando a escrita
        widget.findChild<QTextEdit*>()->setPlainText("Hello AI!");
        QCOMPARE(widget.text(), QString("Hello AI!"));

        widget.clearInput();
        QCOMPARE(widget.text(), QString(""));
    }

    void testSendSignal() {
        PromptInputWidget widget;
        QSignalSpy spy(&widget, &PromptInputWidget::sendRequested);

        widget.findChild<QTextEdit*>()->setPlainText("Make it beautiful");
        
        // Simular o clique no botão enviar
        auto* sendBtn = widget.findChild<QPushButton*>(); // Pega o primeiro ou achar por objectName (recomendável no futuro)
        // Como não definimos ObjectName, vamos forçar o envio direto para testar o signal
        // O ideal é buscar o botão pelo nome, mas por enquanto, apenas verificamos o funcionamento
        
        // Vamos testar o Shift+Enter já que implementamos o eventFilter
        QKeyEvent keyEvent(QEvent::KeyPress, Qt::Key_Enter, Qt::ShiftModifier);
        QApplication::sendEvent(widget.findChild<QTextEdit*>(), &keyEvent);

        QCOMPARE(spy.count(), 1);
        QList<QVariant> arguments = spy.takeFirst();
        QCOMPARE(arguments.at(0).toString(), QString("Make it beautiful"));
    }
};

QTEST_MAIN(TestPromptInputWidget)
#include "tst_PromptInputWidget.moc"
