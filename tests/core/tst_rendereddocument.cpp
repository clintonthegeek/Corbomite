// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTextDocument>
#include "corbomite/core/RenderedDocument.h"

class TestRenderedDocument : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testFromQTextDocument()
    {
        auto qtDoc = std::make_unique<QTextDocument>();
        qtDoc->setHtml(QStringLiteral("<p>Hello</p>"));

        auto rendered = Corbomite::RenderedDocument::fromQTextDocument(std::move(qtDoc));
        QVERIFY(rendered != nullptr);
        QVERIFY(rendered->toQTextDocument() != nullptr);
        QVERIFY(rendered->toQTextDocument()->toPlainText().contains(QStringLiteral("Hello")));
    }

    void testCreateWidget()
    {
        auto qtDoc = std::make_unique<QTextDocument>();
        qtDoc->setHtml(QStringLiteral("<p>Widget test</p>"));

        auto rendered = Corbomite::RenderedDocument::fromQTextDocument(std::move(qtDoc));
        QWidget *widget = rendered->createWidget(nullptr);
        QVERIFY(widget != nullptr);
        delete widget;
    }

    void testEmptyDocument()
    {
        auto qtDoc = std::make_unique<QTextDocument>();
        auto rendered = Corbomite::RenderedDocument::fromQTextDocument(std::move(qtDoc));
        QVERIFY(rendered != nullptr);
        QVERIFY(rendered->toQTextDocument() != nullptr);
    }
};

QTEST_MAIN(TestRenderedDocument)
#include "tst_rendereddocument.moc"
