// SPDX-License-Identifier: GPL-3.0-or-later
// A1 coverage: trigger detection + names mode + replaceEnd + ambiguity.
// (A2 adds alias/heading slots; A3 adds blocks — same file.)
#include "WikiLinkSuggest.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/storage/FileSystemAdapter.h"

#include <QObject>
#include <QTemporaryDir>
#include <QTest>

using namespace Corbomite;

class WikiLinkSuggestTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    std::unique_ptr<FileSystemAdapter> m_adapter;
    std::unique_ptr<Vault> m_vault;
    LinkResolver m_resolver;

    void writeNote(const QString &rel, const QByteArray &body)
    {
        const QString abs = m_dir.path() + QLatin1Char('/') + rel;
        QDir().mkpath(QFileInfo(abs).absolutePath());
        QFile f(abs);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(body);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        writeNote(QStringLiteral("Alpha.md"), "# Alpha\n");
        writeNote(QStringLiteral("Beta.md"), "# Beta\n");
        writeNote(QStringLiteral("sub/Beta.md"), "# Beta in sub\n");
        m_adapter = std::make_unique<FileSystemAdapter>();
        m_vault = std::make_unique<Vault>(m_adapter.get(), nullptr);
        m_vault->load(m_dir.path());
        QStringList paths;
        for (auto *tf : m_vault->getMarkdownFiles()) paths << tf->path;
        m_resolver.setVaultPaths(paths);
    }

    void trigger_afterDoubleBracket()
    {
        WikiLinkSuggest s(m_vault.get());
        auto info = s.onTrigger(6, QStringLiteral("x [[Al"), nullptr);
        QVERIFY(info.has_value());
        QCOMPARE(info->start, 4);
        QCOMPARE(info->end, 6);
        QCOMPARE(info->query, QStringLiteral("Al"));
        QCOMPARE(info->replaceEnd, -1);                   // nothing to consume
        QVERIFY(!s.onTrigger(1, QStringLiteral("no link"), nullptr).has_value());
    }

    void trigger_bailsOnClosedLink()
    {
        WikiLinkSuggest s(m_vault.get());
        QVERIFY(!s.onTrigger(9, QStringLiteral("[[done]] x"), nullptr).has_value());
    }

    void trigger_consumesTrailingBrackets()
    {
        WikiLinkSuggest s(m_vault.get());
        auto info = s.onTrigger(4, QStringLiteral("[[Al]]"), nullptr);
        QVERIFY(info.has_value());
        QCOMPARE(info->replaceEnd, 6);                    // consume the "]]"
    }

    void names_universeAndInsertText()
    {
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        auto info = s.onTrigger(2, QStringLiteral("[["), nullptr);
        QVERIFY(info.has_value());
        const auto set = s.getSuggestions(*info);
        QCOMPARE(set.filter, QString());
        // At least the 3 A1 notes: Alpha, Beta (x2 — ambiguous basename).
        QVERIFY(set.items.size() >= 3);
        QString alphaInsert, betaInserts;
        for (const auto &it : set.items) {
            if (it.display == QStringLiteral("Alpha")) alphaInsert = it.insertText;
            if (it.display == QStringLiteral("Beta"))  betaInserts += it.insertText + QLatin1Char(';');
        }
        QCOMPARE(alphaInsert, QStringLiteral("Alpha]]"));               // unique → basename
        QVERIFY2(betaInserts.contains(QStringLiteral("Beta]]"))
                     && betaInserts.contains(QStringLiteral("sub/Beta]]")),
                 qPrintable(QStringLiteral("ambiguous basenames must insert paths: %1").arg(betaInserts)));
    }
};

QTEST_MAIN(WikiLinkSuggestTest)
#include "tst_wikilink_suggest.moc"
