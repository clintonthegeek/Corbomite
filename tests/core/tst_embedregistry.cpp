// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Core::EmbedRegistry — extension-to-factory dispatch
// for ![[file.ext]] embeds. Cluster J Phase 1 Task 1.4.

#include <QTest>

#include "corbomite/core/EmbedRegistry.h"
#include "corbomite/core/MarkdownRenderChild.h"

class TstEmbedRegistry : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRegisterAndDispatch()
    {
        Corbomite::Core::EmbedRegistry reg;
        bool called = false;
        QString seenPath;
        reg.registerExtension(
            QStringLiteral("pdf"),
            [&](const Corbomite::Core::EmbedRequest &req)
                -> std::unique_ptr<Corbomite::Core::MarkdownRenderChild> {
                called = true;
                seenPath = req.targetPath;
                return std::make_unique<Corbomite::Core::MarkdownRenderChild>();
            });
        auto child = reg.dispatch({QStringLiteral("doc.pdf"), {}, nullptr, 0});
        QVERIFY(called);
        QCOMPARE(seenPath, QStringLiteral("doc.pdf"));
        QVERIFY(child != nullptr);
    }

    void testUnknownExtensionReturnsNullptr()
    {
        Corbomite::Core::EmbedRegistry reg;
        auto child = reg.dispatch({QStringLiteral("doc.xyz"), {}, nullptr, 0});
        QCOMPARE(child.get(), nullptr);
    }

    void testCaseInsensitiveExtension()
    {
        Corbomite::Core::EmbedRegistry reg;
        reg.registerExtension(
            QStringLiteral("pdf"),
            [](const Corbomite::Core::EmbedRequest &) {
                return std::make_unique<Corbomite::Core::MarkdownRenderChild>();
            });
        QVERIFY(reg.dispatch({QStringLiteral("doc.PDF"), {}, nullptr, 0}) != nullptr);
        QVERIFY(reg.dispatch({QStringLiteral("doc.Pdf"), {}, nullptr, 0}) != nullptr);
    }

    void testCaseInsensitiveRegistrationKey()
    {
        // Register with mixed case; registry should still dispatch on
        // lowercase filename suffix.
        Corbomite::Core::EmbedRegistry reg;
        reg.registerExtension(
            QStringLiteral("PDF"),
            [](const Corbomite::Core::EmbedRequest &) {
                return std::make_unique<Corbomite::Core::MarkdownRenderChild>();
            });
        QVERIFY(reg.dispatch({QStringLiteral("doc.pdf"), {}, nullptr, 0}) != nullptr);
    }

    void testHandleUnregister()
    {
        Corbomite::Core::EmbedRegistry reg;
        auto h = reg.registerExtension(
            QStringLiteral("md"),
            [](const Corbomite::Core::EmbedRequest &) {
                return std::make_unique<Corbomite::Core::MarkdownRenderChild>();
            });
        QVERIFY(reg.dispatch({QStringLiteral("a.md"), {}, nullptr, 0}) != nullptr);
        reg.unregister(h);
        QCOMPARE(reg.dispatch({QStringLiteral("a.md"), {}, nullptr, 0}).get(),
                 nullptr);
    }

    void testDispatchPassesSubpathAndDepth()
    {
        Corbomite::Core::EmbedRegistry reg;
        QString seenSub;
        int seenDepth = -1;
        reg.registerExtension(
            QStringLiteral("md"),
            [&](const Corbomite::Core::EmbedRequest &req) {
                seenSub = req.subpath;
                seenDepth = req.depth;
                return std::make_unique<Corbomite::Core::MarkdownRenderChild>();
            });
        (void)reg.dispatch({QStringLiteral("Note.md"),
                            QStringLiteral("#Heading"),
                            nullptr,
                            3});
        QCOMPARE(seenSub, QStringLiteral("#Heading"));
        QCOMPARE(seenDepth, 3);
    }
};

QTEST_APPLESS_MAIN(TstEmbedRegistry)
#include "tst_embedregistry.moc"
