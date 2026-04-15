// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Core::CodeBlockProcessorRegistry — per-language
// dispatch for fenced code blocks. Cluster J Phase 2 Task 2.2.

#include <QTest>

#include "corbomite/core/CodeBlockProcessorRegistry.h"

class TstCodeBlockProcessorRegistry : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testLanguageDispatch()
    {
        Corbomite::Core::CodeBlockProcessorRegistry reg;
        QString lastSource;
        reg.registerLanguage(
            QStringLiteral("mermaid"),
            [&](const QString &src,
                void *,
                const Corbomite::Core::CodeBlockContext &) {
                lastSource = src;
                return true;
            });
        QVERIFY(reg.dispatch(QStringLiteral("mermaid"),
                             QStringLiteral("flowchart TD;A-->B"),
                             nullptr,
                             {}));
        QCOMPARE(lastSource, QStringLiteral("flowchart TD;A-->B"));
    }

    void testUnknownLanguageReturnsFalse()
    {
        Corbomite::Core::CodeBlockProcessorRegistry reg;
        QVERIFY(!reg.dispatch(QStringLiteral("unknownlang"),
                              QStringLiteral("x=1"),
                              nullptr,
                              {}));
    }

    void testCaseInsensitiveLanguageMatch()
    {
        Corbomite::Core::CodeBlockProcessorRegistry reg;
        reg.registerLanguage(
            QStringLiteral("mermaid"),
            [](const QString &,
               void *,
               const Corbomite::Core::CodeBlockContext &) { return true; });
        QVERIFY(reg.dispatch(QStringLiteral("Mermaid"),
                             QStringLiteral("x"),
                             nullptr,
                             {}));
        QVERIFY(reg.dispatch(QStringLiteral("MERMAID"),
                             QStringLiteral("x"),
                             nullptr,
                             {}));
    }

    void testCaseInsensitiveRegistrationKey()
    {
        Corbomite::Core::CodeBlockProcessorRegistry reg;
        reg.registerLanguage(
            QStringLiteral("MERMAID"),
            [](const QString &,
               void *,
               const Corbomite::Core::CodeBlockContext &) { return true; });
        QVERIFY(reg.dispatch(QStringLiteral("mermaid"),
                             QStringLiteral("x"),
                             nullptr,
                             {}));
    }

    void testUnregister()
    {
        Corbomite::Core::CodeBlockProcessorRegistry reg;
        auto h = reg.registerLanguage(
            QStringLiteral("cpp"),
            [](const QString &,
               void *,
               const Corbomite::Core::CodeBlockContext &) { return true; });
        QVERIFY(reg.dispatch(QStringLiteral("cpp"),
                             QString(),
                             nullptr,
                             {}));
        reg.unregister(h);
        QVERIFY(!reg.dispatch(QStringLiteral("cpp"),
                              QString(),
                              nullptr,
                              {}));
    }

    void testBooleanReturnFallthrough()
    {
        Corbomite::Core::CodeBlockProcessorRegistry reg;
        reg.registerLanguage(
            QStringLiteral("cpp"),
            [](const QString &,
               void *,
               const Corbomite::Core::CodeBlockContext &) { return false; });
        // Registered but returns false — dispatch returns false so the
        // caller can fall through to default highlighting.
        QVERIFY(!reg.dispatch(QStringLiteral("cpp"),
                              QStringLiteral("int main(){}"),
                              nullptr,
                              {}));
    }
};

QTEST_APPLESS_MAIN(TstCodeBlockProcessorRegistry)
#include "tst_codeblockprocessorregistry.moc"
