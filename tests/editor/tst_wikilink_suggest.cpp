// SPDX-License-Identifier: GPL-3.0-or-later
// A1 coverage: trigger detection + names mode + replaceEnd + ambiguity.
// (A2 adds alias/heading slots; A3 adds blocks — same file.)
#include "WikiLinkSuggest.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
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
    std::unique_ptr<MetadataCache> m_cache;

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

        // A2 alias fixture: write notes with frontmatter aliases, reload the
        // vault so they are visible, then feed the metadata cache.
        writeNote(QStringLiteral("Aliased.md"),
                  "---\naliases: [Nickname, Other Name]\n---\n# Aliased\n## Section One\n## Section Two\nText ^blockid1\n");
        writeNote(QStringLiteral("Solo.md"), "---\nalias: TheOne\n---\nx\n");
        m_vault->unload();
        m_vault->load(m_dir.path());
        QStringList paths2;
        for (auto *tf : m_vault->getMarkdownFiles()) paths2 << tf->path;
        m_resolver.setVaultPaths(paths2);
        m_cache = std::make_unique<MetadataCache>(m_resolver, nullptr);
        QString aliasedPath, soloPath;
        for (auto *tf : m_vault->getMarkdownFiles()) {
            m_cache->onFileChanged(tf->path, m_vault->read(tf), 1);
            if (tf->basename == QStringLiteral("Aliased")) aliasedPath = tf->path;
            if (tf->basename == QStringLiteral("Solo")) soloPath = tf->path;
        }
        QVERIFY(!aliasedPath.isEmpty());
        QVERIFY(!soloPath.isEmpty());
        // Parsing runs on a worker thread; results arrive via queued signals.
        // Spin the event loop until both alias-bearing notes are cached.
        QTRY_VERIFY(m_cache->getFileCache(aliasedPath).has_value()
                    && m_cache->getFileCache(aliasedPath)->frontmatter.has_value());
        QTRY_VERIFY(m_cache->getFileCache(soloPath).has_value()
                    && m_cache->getFileCache(soloPath)->frontmatter.has_value());
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

    void aliases_appearWithTargetDetail()
    {
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        s.setMetadataCache(m_cache.get());
        auto info = s.onTrigger(2, QStringLiteral("[["), nullptr);
        const auto set = s.getSuggestions(*info);
        bool found = false;
        for (const auto &it : set.items) {
            if (it.display == QStringLiteral("Nickname")) {
                QCOMPARE(it.insertText, QStringLiteral("Aliased|Nickname]]"));
                found = true;
            }
        }
        QVERIFY2(found, "alias candidate missing");
    }

    void aliases_stringFormAccepted()
    {
        // "alias: TheOne" (string, not array) must also surface.
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        s.setMetadataCache(m_cache.get());
        auto info = s.onTrigger(2, QStringLiteral("[["), nullptr);
        const auto set = s.getSuggestions(*info);
        bool found = false;
        for (const auto &it : set.items)
            if (it.display == QStringLiteral("TheOne")) found = true;
        QVERIFY(found);
    }

    void headings_listedForResolvedTarget()
    {
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        s.setMetadataCache(m_cache.get());
        auto info = s.onTrigger(11, QStringLiteral("[[Aliased#S"), nullptr);
        QVERIFY(info.has_value());
        const auto set = s.getSuggestions(*info);
        QCOMPARE(set.filter, QStringLiteral("S"));
        QStringList displays;
        for (const auto &it : set.items) displays << it.display;
        QVERIFY(displays.contains(QStringLiteral("Section One")));
        QVERIFY(displays.contains(QStringLiteral("Section Two")));
        for (const auto &it : set.items)
            if (it.display == QStringLiteral("Section One"))
                QCOMPARE(it.insertText, QStringLiteral("Aliased#Section One]]"));
    }

    void headings_unresolvedTarget_emptyUniverse()
    {
        WikiLinkSuggest s(m_vault.get());
        s.setLinkResolver(&m_resolver);
        s.setMetadataCache(m_cache.get());
        // "[[Nope#x" is 8 chars; cursorPos must equal the string length
        // (onTrigger returns nullopt for cursorPos > length).
        auto info = s.onTrigger(8, QStringLiteral("[[Nope#x"), nullptr);
        QVERIFY(info.has_value());
        QVERIFY(s.getSuggestions(*info).items.isEmpty());
    }
};

QTEST_MAIN(WikiLinkSuggestTest)
#include "tst_wikilink_suggest.moc"
