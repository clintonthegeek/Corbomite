// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Core::MarkdownRenderChild — lifecycle-tied widget
// subtree produced by post-processors, code-block processors, and embed
// renderers. Cluster J Phase 1 Task 1.2.
// Audit reference: docs/obsidian-audit/domains/editor-markdown.md §10.

#include <QTest>
#include <QWidget>

#include "corbomite/core/Component.h"
#include "corbomite/core/MarkdownRenderChild.h"

class TstMarkdownRenderChild : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testInheritsComponent()
    {
        Corbomite::Core::MarkdownRenderChild child;
        Corbomite::Component *asComponent = &child;
        QVERIFY(asComponent != nullptr);
        QVERIFY(!asComponent->isLoaded());
    }

    void testLoadUnloadLifecycle()
    {
        Corbomite::Core::MarkdownRenderChild child;
        QVERIFY(!child.isLoaded());
        child.load();
        QVERIFY(child.isLoaded());
        child.unload();
        QVERIFY(!child.isLoaded());
        child.unload(); // idempotent
        QVERIFY(!child.isLoaded());
    }

    void testChildRegistration()
    {
        Corbomite::Core::MarkdownRenderChild parent;
        auto *child = new Corbomite::Core::MarkdownRenderChild();
        parent.addChild(child);
        parent.load();
        QVERIFY(child->isLoaded());
        parent.unload();
        QVERIFY(!child->isLoaded());
    }

    void testRenderedTextAccessor()
    {
        Corbomite::Core::MarkdownRenderChild child;
        QVERIFY(child.renderedText().isEmpty());
        child.setRenderedText(QStringLiteral("Hello **world**"));
        QCOMPARE(child.renderedText(), QStringLiteral("Hello **world**"));
    }

    void testMountInto()
    {
        Corbomite::Core::MarkdownRenderChild child;
        QWidget host;
        child.mountInto(&host);
        QCOMPARE(child.hostWidget(), &host);
    }

    void testMountIntoNullptrIsSafe()
    {
        Corbomite::Core::MarkdownRenderChild child;
        child.mountInto(nullptr);
        QCOMPARE(child.hostWidget(), nullptr);
    }
};

QTEST_MAIN(TstMarkdownRenderChild)
#include "tst_markdownrenderchild.moc"
