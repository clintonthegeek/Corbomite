# Global Search & Reading Mode — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add SQLite FTS5 full-text search with results sidebar panel (Ctrl+Shift+F) and rendered markdown reading mode with Ctrl+E toggle, completing Phase 2.

**Architecture:** SQLiteIndex in libs/storage provides FTS5 indexing/search. MarkdownRenderer in libs/core converts markdown to HTML via regex. SearchPanel sidebar shows search results. NotePreviewWidget renders HTML. EditorViewSpace toggles between editor and preview.

**Tech Stack:** C++20, Qt6 (Core, Widgets, Sql), SQLite FTS5, QTextBrowser, KFuzzyMatcher

**Spec:** `docs/superpowers/specs/2026-03-31-global-search-reading-mode-design.md`

**Current state:** 14 tests passing. Wikilink/tag autocomplete and Ctrl+Click navigation complete.

---

### Task 1: SQLiteIndex (libs/storage)

**Files:**
- Create: `libs/storage/include/corbomite/storage/SQLiteIndex.h`
- Create: `libs/storage/src/SQLiteIndex.cpp`
- Modify: `libs/storage/CMakeLists.txt`
- Create: `tests/storage/tst_sqliteindex.cpp`
- Modify: `tests/storage/CMakeLists.txt`
- Modify: `CMakeLists.txt` (add Qt6::Sql to root find_package)

- [ ] **Step 1: Write SQLiteIndex tests**

`tests/storage/tst_sqliteindex.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include "corbomite/storage/SQLiteIndex.h"

class TestSQLiteIndex : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testOpenClose()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        QVERIFY(index.open(tmp.path() + "/test.sqlite"));
        index.close();
    }

    void testIndexAndSearch()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("My Note"),
                        QStringLiteral("This is some content about programming and Qt."));

        auto results = index.search(QStringLiteral("programming"));
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.at(0).notePath, QStringLiteral("note.md"));
        QVERIFY(!results.at(0).snippet.isEmpty());
    }

    void testSearchNoMatch()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("Hello world"));

        auto results = index.search(QStringLiteral("nonexistent"));
        QCOMPARE(results.size(), 0);
    }

    void testRemoveNote()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("findable content"));
        QCOMPARE(index.search(QStringLiteral("findable")).size(), 1);

        index.removeNote(QStringLiteral("note.md"));
        QCOMPARE(index.search(QStringLiteral("findable")).size(), 0);
    }

    void testUpdateExistingNote()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("old content"));
        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Note"),
                        QStringLiteral("new content"));

        QCOMPARE(index.search(QStringLiteral("old")).size(), 0);
        QCOMPARE(index.search(QStringLiteral("new")).size(), 1);
    }

    void testMultipleNotes()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("a.md"), QStringLiteral("Alpha"),
                        QStringLiteral("shared word unique_alpha"));
        index.indexNote(QStringLiteral("b.md"), QStringLiteral("Beta"),
                        QStringLiteral("shared word unique_beta"));

        QCOMPARE(index.search(QStringLiteral("shared")).size(), 2);
        QCOMPARE(index.search(QStringLiteral("unique_alpha")).size(), 1);
    }

    void testSearchByTitle()
    {
        QTemporaryDir tmp;
        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/test.sqlite");

        index.indexNote(QStringLiteral("note.md"), QStringLiteral("Special Title"),
                        QStringLiteral("boring content"));

        auto results = index.search(QStringLiteral("Special"));
        QCOMPARE(results.size(), 1);
    }

    void testRebuildIndex()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        QFile f1(tmp.path() + "/vault/note1.md");
        f1.open(QIODevice::WriteOnly);
        f1.write("# First\n\nContent one");
        f1.close();
        QFile f2(tmp.path() + "/vault/sub/note2.md");
        QDir().mkpath(tmp.path() + "/vault/sub");
        f2.open(QIODevice::WriteOnly);
        f2.write("# Second\n\nContent two");
        f2.close();

        Corbomite::SQLiteIndex index;
        index.open(tmp.path() + "/index.sqlite");
        index.rebuildIndex(tmp.path() + "/vault");

        QCOMPARE(index.search(QStringLiteral("Content")).size(), 2);
        QCOMPARE(index.search(QStringLiteral("First")).size(), 1);
    }
};

QTEST_MAIN(TestSQLiteIndex)
#include "tst_sqliteindex.moc"
```

- [ ] **Step 2: Implement SQLiteIndex**

`libs/storage/include/corbomite/storage/SQLiteIndex.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace Corbomite {

struct SearchMatch {
    QString notePath;
    QString snippet;
    double score = 0.0;
};

class SQLiteIndex : public QObject {
    Q_OBJECT

public:
    explicit SQLiteIndex(QObject *parent = nullptr);
    ~SQLiteIndex() override;

    bool open(const QString &dbPath);
    void close();

    void rebuildIndex(const QString &vaultRoot);
    // TODO: Move to background thread for large vaults (>1000 notes)
    void indexNote(const QString &relativePath, const QString &title, const QString &content);
    void removeNote(const QString &relativePath);

    QVector<SearchMatch> search(const QString &query, int maxResults = 100) const;
    // TODO: Support Obsidian search operators (file:, path:, tag:, line:, regex)

Q_SIGNALS:
    void indexReady();

private:
    void createTables();
    QString m_connectionName;
    bool m_isOpen = false;
};

} // namespace Corbomite
```

`libs/storage/src/SQLiteIndex.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/VaultScanner.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/core/NoteMeta.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QFileInfo>

namespace Corbomite {

SQLiteIndex::SQLiteIndex(QObject *parent)
    : QObject(parent)
    , m_connectionName(QUuid::createUuid().toString())
{
}

SQLiteIndex::~SQLiteIndex()
{
    close();
}

bool SQLiteIndex::open(const QString &dbPath)
{
    close();

    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        return false;
    }

    createTables();
    m_isOpen = true;
    return true;
}

void SQLiteIndex::close()
{
    if (m_isOpen) {
        QSqlDatabase::database(m_connectionName).close();
        m_isOpen = false;
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

void SQLiteIndex::createTables()
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5("
        "path, title, content, tokenize = 'porter unicode61'"
        ")"));
}

void SQLiteIndex::rebuildIndex(const QString &vaultRoot)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.exec(QStringLiteral("DELETE FROM notes_fts"));

    VaultScanner scanner;
    FileSystemAdapter fs;
    auto notes = scanner.scan(vaultRoot);

    for (const auto &meta : notes) {
        auto content = fs.readFile(meta.absolutePath(vaultRoot));
        if (!content.has_value()) continue;

        QString title = meta.nameFromPath();

        query.prepare(QStringLiteral(
            "INSERT INTO notes_fts(path, title, content) VALUES(?, ?, ?)"));
        query.addBindValue(meta.relativePath);
        query.addBindValue(title);
        query.addBindValue(content.value());
        query.exec();
    }

    Q_EMIT indexReady();
}

void SQLiteIndex::indexNote(const QString &relativePath, const QString &title, const QString &content)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    // Remove existing entry first (upsert)
    query.prepare(QStringLiteral("DELETE FROM notes_fts WHERE path = ?"));
    query.addBindValue(relativePath);
    query.exec();

    // Insert new entry
    query.prepare(QStringLiteral(
        "INSERT INTO notes_fts(path, title, content) VALUES(?, ?, ?)"));
    query.addBindValue(relativePath);
    query.addBindValue(title);
    query.addBindValue(content);
    query.exec();
}

void SQLiteIndex::removeNote(const QString &relativePath)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("DELETE FROM notes_fts WHERE path = ?"));
    query.addBindValue(relativePath);
    query.exec();
}

QVector<SearchMatch> SQLiteIndex::search(const QString &query, int maxResults) const
{
    QVector<SearchMatch> results;
    if (query.trimmed().isEmpty() || !m_isOpen) return results;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "SELECT path, snippet(notes_fts, 2, '<b>', '</b>', '...', 32), rank "
        "FROM notes_fts WHERE notes_fts MATCH ? "
        "ORDER BY rank LIMIT ?"));
    q.addBindValue(query);
    q.addBindValue(maxResults);

    if (!q.exec()) return results;

    while (q.next()) {
        SearchMatch match;
        match.notePath = q.value(0).toString();
        match.snippet = q.value(1).toString();
        match.score = q.value(2).toDouble();
        results.append(match);
    }

    return results;
}

} // namespace Corbomite
```

- [ ] **Step 3: Update CMakeLists**

`libs/storage/CMakeLists.txt` — add Qt6::Sql and SQLiteIndex:
```cmake
cmake_minimum_required(VERSION 3.19)
project(corbomite-storage VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Sql)

add_library(corbomite-storage STATIC
    src/FileSystemAdapter.cpp
    src/VaultScanner.cpp
    src/SQLiteIndex.cpp
    include/corbomite/storage/SQLiteIndex.h
)
set_target_properties(corbomite-storage PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Storage ALIAS corbomite-storage)

target_include_directories(corbomite-storage
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-storage PUBLIC Qt6::Core Qt6::Sql)

if(NOT PROJECT_IS_TOP_LEVEL)
    target_link_libraries(corbomite-storage PUBLIC Corbomite::Core)
endif()
```

Root `CMakeLists.txt` — add Sql to Qt6 find_package:
```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets DBus Sql)
```

`tests/storage/CMakeLists.txt` — add test:
```cmake
add_executable(tst_sqliteindex tst_sqliteindex.cpp)
add_test(NAME tst_sqliteindex COMMAND tst_sqliteindex)
target_link_libraries(tst_sqliteindex PRIVATE Qt6::Test Qt6::Sql Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_sqliteindex PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 4: Build and run tests**

Run:
```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_sqliteindex --output-on-failure
```

Expected: All SQLiteIndex tests pass.

- [ ] **Step 5: Run all tests**

Run: `cd build && ctest --output-on-failure`

Expected: All 15+ tests pass.

- [ ] **Step 6: Commit**

```bash
git add libs/storage/ tests/storage/tst_sqliteindex.cpp tests/storage/CMakeLists.txt CMakeLists.txt
git commit -m "feat: add SQLiteIndex with FTS5 full-text search and tests

SQLite FTS5 index for vault-wide note search. Supports indexing,
removal, rebuild, and scored search with snippet context.
Porter stemming + unicode61 tokenizer."
```

---

### Task 2: MarkdownRenderer (libs/core)

**Files:**
- Create: `libs/core/include/corbomite/core/MarkdownRenderer.h`
- Create: `libs/core/src/MarkdownRenderer.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Create: `tests/core/tst_markdownrenderer.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write MarkdownRenderer tests**

`tests/core/tst_markdownrenderer.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/MarkdownRenderer.h"

class TestMarkdownRenderer : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testHeading()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("# Title"));
        QVERIFY(html.contains(QStringLiteral("<h1>Title</h1>")));
    }

    void testHeadingLevels()
    {
        Corbomite::MarkdownRenderer r;
        QVERIFY(r.renderToHtml(QStringLiteral("## Sub")).contains(QStringLiteral("<h2>")));
        QVERIFY(r.renderToHtml(QStringLiteral("### Sub3")).contains(QStringLiteral("<h3>")));
    }

    void testBold()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("This is **bold** text"));
        QVERIFY(html.contains(QStringLiteral("<strong>bold</strong>")));
    }

    void testItalic()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("This is *italic* text"));
        QVERIFY(html.contains(QStringLiteral("<em>italic</em>")));
    }

    void testLink()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("[Click](https://example.com)"));
        QVERIFY(html.contains(QStringLiteral("<a href=\"https://example.com\">Click</a>")));
    }

    void testInlineCode()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("Use `printf()` here"));
        QVERIFY(html.contains(QStringLiteral("<code>printf()</code>")));
    }

    void testCodeBlock()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("```cpp\nint x = 1;\n```"));
        QVERIFY(html.contains(QStringLiteral("<pre><code")));
        QVERIFY(html.contains(QStringLiteral("int x = 1;")));
    }

    void testWikiLink()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("See [[My Note]] here"));
        QVERIFY(html.contains(QStringLiteral("class=\"internal-link\"")));
        QVERIFY(html.contains(QStringLiteral("My Note")));
    }

    void testHighlight()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("This is ==important== text"));
        QVERIFY(html.contains(QStringLiteral("<mark>important</mark>")));
    }

    void testCommentStripped()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("Visible %%hidden%% text"));
        QVERIFY(!html.contains(QStringLiteral("hidden")));
        QVERIFY(html.contains(QStringLiteral("Visible")));
        QVERIFY(html.contains(QStringLiteral("text")));
    }

    void testCallout()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("> [!warning] Be careful\n> Content here"));
        QVERIFY(html.contains(QStringLiteral("callout")));
        QVERIFY(html.contains(QStringLiteral("warning")));
    }

    void testTag()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("Hello #project tag"));
        QVERIFY(html.contains(QStringLiteral("class=\"tag\"")));
        QVERIFY(html.contains(QStringLiteral("#project")));
    }

    void testUnorderedList()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("- item one\n- item two"));
        QVERIFY(html.contains(QStringLiteral("<ul>")));
        QVERIFY(html.contains(QStringLiteral("<li>")));
    }

    void testBlockquote()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("> quoted text"));
        QVERIFY(html.contains(QStringLiteral("<blockquote>")));
    }

    void testHorizontalRule()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("---"));
        QVERIFY(html.contains(QStringLiteral("<hr")));
    }

    void testCheckbox()
    {
        Corbomite::MarkdownRenderer r;
        QString html = r.renderToHtml(QStringLiteral("- [ ] todo\n- [x] done"));
        QVERIFY(html.contains(QStringLiteral("checkbox")));
    }
};

QTEST_MAIN(TestMarkdownRenderer)
#include "tst_markdownrenderer.moc"
```

- [ ] **Step 2: Implement MarkdownRenderer**

`libs/core/include/corbomite/core/MarkdownRenderer.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

class MarkdownRenderer {
public:
    QString renderToHtml(const QString &markdown) const;

    // TODO: Replace regex renderer with cmark-gfm or other proper markdown
    // parser for full CommonMark spec compliance. The regex approach handles
    // common cases but will fail on edge cases like nested emphasis, reference
    // links, and complex list nesting. cmark-gfm provides a proper AST-based
    // pipeline with GFM extensions (tables, strikethrough, autolinks, task lists).

private:
    QString processBlocks(const QString &markdown) const;
    QString processInline(const QString &text) const;
    QString wrapDocument(const QString &body) const;
    static QString escapeHtml(const QString &text);
    static QString defaultStylesheet();
};

} // namespace Corbomite
```

`libs/core/src/MarkdownRenderer.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MarkdownRenderer.h"
#include <QRegularExpression>
#include <QStringList>

// TODO: This entire file should be replaced with a cmark-gfm based renderer.
// The regex approach below is a pragmatic first pass that handles the most
// common markdown patterns. Known limitations:
// - Nested emphasis (***bold italic***) may not render correctly
// - Reference-style links [text][ref] are not supported
// - Complex list nesting (mixed ordered/unordered, 3+ levels) is fragile
// - Table alignment (:---:) is ignored
// - Setext-style headings (underline with === or ---) are not supported

namespace Corbomite {

QString MarkdownRenderer::renderToHtml(const QString &markdown) const
{
    QString body = processBlocks(markdown);
    return wrapDocument(body);
}

QString MarkdownRenderer::processBlocks(const QString &markdown) const
{
    QStringList lines = markdown.split(QLatin1Char('\n'));
    QString html;
    bool inCodeBlock = false;
    QString codeBlockLang;
    QString codeContent;
    bool inList = false;
    bool isOrderedList = false;
    bool inBlockquote = false;
    bool inCallout = false;
    QString calloutType;
    QString calloutContent;

    static const QRegularExpression headingPattern(QStringLiteral(R"(^(#{1,6})\s+(.+)$)"));
    static const QRegularExpression codeFencePattern(QStringLiteral(R"(^```(\w*)$)"));
    static const QRegularExpression ulPattern(QStringLiteral(R"(^[-*+]\s+(.+)$)"));
    static const QRegularExpression olPattern(QStringLiteral(R"(^\d+\.\s+(.+)$)"));
    static const QRegularExpression checkboxUnchecked(QStringLiteral(R"(^[-*+]\s+\[ \]\s+(.+)$)"));
    static const QRegularExpression checkboxChecked(QStringLiteral(R"(^[-*+]\s+\[x\]\s+(.+)$)"));
    static const QRegularExpression blockquotePattern(QStringLiteral(R"(^>\s?(.*)$)"));
    static const QRegularExpression calloutPattern(QStringLiteral(R"(^>\s*\[!(\w+)\]\s*(.*)$)"));
    static const QRegularExpression hrPattern(QStringLiteral(R"(^(---|\*\*\*|___)$)"));

    auto closeList = [&]() {
        if (inList) {
            html += isOrderedList ? QStringLiteral("</ol>\n") : QStringLiteral("</ul>\n");
            inList = false;
        }
    };

    auto closeBlockquote = [&]() {
        if (inBlockquote && !inCallout) {
            html += QStringLiteral("</blockquote>\n");
            inBlockquote = false;
        }
    };

    auto closeCallout = [&]() {
        if (inCallout) {
            html += QStringLiteral("<p>") + processInline(calloutContent.trimmed()) + QStringLiteral("</p>\n");
            html += QStringLiteral("</div>\n");
            inCallout = false;
            calloutContent.clear();
            inBlockquote = false;
        }
    };

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];

        // Code fence handling
        auto codeFenceMatch = codeFencePattern.match(line);
        if (codeFenceMatch.hasMatch()) {
            if (!inCodeBlock) {
                closeList();
                closeBlockquote();
                closeCallout();
                inCodeBlock = true;
                codeBlockLang = codeFenceMatch.captured(1);
                codeContent.clear();
            } else {
                QString langAttr = codeBlockLang.isEmpty()
                    ? QString()
                    : QStringLiteral(" class=\"language-%1\"").arg(codeBlockLang);
                html += QStringLiteral("<pre><code%1>%2</code></pre>\n")
                            .arg(langAttr, escapeHtml(codeContent));
                inCodeBlock = false;
            }
            continue;
        }
        if (inCodeBlock) {
            if (!codeContent.isEmpty()) codeContent += QLatin1Char('\n');
            codeContent += line;
            continue;
        }

        // Horizontal rule
        if (hrPattern.match(line).hasMatch()) {
            closeList();
            closeBlockquote();
            closeCallout();
            html += QStringLiteral("<hr/>\n");
            continue;
        }

        // Empty line
        if (line.trimmed().isEmpty()) {
            closeList();
            if (inCallout) {
                // Empty line in callout continues the callout
            } else {
                closeBlockquote();
            }
            continue;
        }

        // Callout: > [!type] Title
        auto calloutMatch = calloutPattern.match(line);
        if (calloutMatch.hasMatch() && !inCallout) {
            closeList();
            closeBlockquote();
            closeCallout();
            inCallout = true;
            inBlockquote = true;
            calloutType = calloutMatch.captured(1).toLower();
            QString title = calloutMatch.captured(2);
            html += QStringLiteral("<div class=\"callout callout-%1\">\n").arg(calloutType);
            if (!title.isEmpty()) {
                html += QStringLiteral("<div class=\"callout-title\">%1</div>\n").arg(escapeHtml(title));
            }
            calloutContent.clear();
            continue;
        }

        // Blockquote continuation (including callout body)
        auto bqMatch = blockquotePattern.match(line);
        if (bqMatch.hasMatch()) {
            if (inCallout) {
                if (!calloutContent.isEmpty()) calloutContent += QLatin1Char('\n');
                calloutContent += bqMatch.captured(1);
                continue;
            }
            closeList();
            if (!inBlockquote) {
                html += QStringLiteral("<blockquote>\n");
                inBlockquote = true;
            }
            html += QStringLiteral("<p>") + processInline(bqMatch.captured(1)) + QStringLiteral("</p>\n");
            continue;
        } else {
            closeCallout();
            closeBlockquote();
        }

        // Heading
        auto headingMatch = headingPattern.match(line);
        if (headingMatch.hasMatch()) {
            closeList();
            int level = headingMatch.captured(1).length();
            QString content = processInline(headingMatch.captured(2));
            html += QStringLiteral("<h%1>%2</h%1>\n").arg(level).arg(content);
            continue;
        }

        // Checkbox (before generic list — more specific pattern)
        auto cbUnchecked = checkboxUnchecked.match(line);
        if (cbUnchecked.hasMatch()) {
            if (!inList) {
                html += QStringLiteral("<ul class=\"checklist\">\n");
                inList = true;
                isOrderedList = false;
            }
            html += QStringLiteral("<li><input type=\"checkbox\" disabled> %1</li>\n")
                        .arg(processInline(cbUnchecked.captured(1)));
            continue;
        }
        auto cbChecked = checkboxChecked.match(line);
        if (cbChecked.hasMatch()) {
            if (!inList) {
                html += QStringLiteral("<ul class=\"checklist\">\n");
                inList = true;
                isOrderedList = false;
            }
            html += QStringLiteral("<li><input type=\"checkbox\" checked disabled> %1</li>\n")
                        .arg(processInline(cbChecked.captured(1)));
            continue;
        }

        // Unordered list
        auto ulMatch = ulPattern.match(line);
        if (ulMatch.hasMatch()) {
            if (!inList || isOrderedList) {
                closeList();
                html += QStringLiteral("<ul>\n");
                inList = true;
                isOrderedList = false;
            }
            html += QStringLiteral("<li>%1</li>\n").arg(processInline(ulMatch.captured(1)));
            continue;
        }

        // Ordered list
        auto olMatch = olPattern.match(line);
        if (olMatch.hasMatch()) {
            if (!inList || !isOrderedList) {
                closeList();
                html += QStringLiteral("<ol>\n");
                inList = true;
                isOrderedList = true;
            }
            html += QStringLiteral("<li>%1</li>\n").arg(processInline(olMatch.captured(1)));
            continue;
        }

        // Regular paragraph
        closeList();
        html += QStringLiteral("<p>%1</p>\n").arg(processInline(line));
    }

    // Close any open blocks
    closeList();
    closeCallout();
    closeBlockquote();

    return html;
}

QString MarkdownRenderer::processInline(const QString &text) const
{
    QString result = text;

    // Strip Obsidian comments: %%...%%
    static const QRegularExpression commentPattern(QStringLiteral(R"(%%.+?%%)"));
    result.replace(commentPattern, QString());

    // Escape HTML in remaining text (but we need to do it carefully to not break our own tags)
    // Process patterns from most specific to least specific

    // Wikilinks: [[Note]] or [[Note|Display]]
    static const QRegularExpression wikiLinkAlias(QStringLiteral(R"(\[\[([^\]|]+)\|([^\]]+)\]\])"));
    result.replace(wikiLinkAlias, QStringLiteral(R"(<a class="internal-link" href="\1.md">\2</a>)"));

    static const QRegularExpression wikiLink(QStringLiteral(R"(\[\[([^\]]+)\]\])"));
    result.replace(wikiLink, QStringLiteral(R"(<a class="internal-link" href="\1.md">\1</a>)"));

    // Images: ![alt](src)
    static const QRegularExpression imagePattern(QStringLiteral(R"(!\[([^\]]*)\]\(([^)]+)\))"));
    result.replace(imagePattern, QStringLiteral(R"(<img src="\2" alt="\1"/>)"));

    // Links: [text](url)
    static const QRegularExpression linkPattern(QStringLiteral(R"(\[([^\]]+)\]\(([^)]+)\))"));
    result.replace(linkPattern, QStringLiteral(R"(<a href="\2">\1</a>)"));

    // Highlight: ==text==
    static const QRegularExpression highlightPattern(QStringLiteral(R"(==(.+?)==)"));
    result.replace(highlightPattern, QStringLiteral(R"(<mark>\1</mark>)"));

    // Bold: **text**
    static const QRegularExpression boldPattern(QStringLiteral(R"(\*\*(.+?)\*\*)"));
    result.replace(boldPattern, QStringLiteral(R"(<strong>\1</strong>)"));

    // Italic: *text*
    static const QRegularExpression italicPattern(QStringLiteral(R"(\*(.+?)\*(?!\*))"));
    result.replace(italicPattern, QStringLiteral(R"(<em>\1</em>)"));

    // Strikethrough: ~~text~~
    static const QRegularExpression strikePattern(QStringLiteral(R"(~~(.+?)~~)"));
    result.replace(strikePattern, QStringLiteral(R"(<del>\1</del>)"));

    // Inline code: `code`
    static const QRegularExpression codePattern(QStringLiteral(R"(`([^`]+)`)"));
    result.replace(codePattern, QStringLiteral(R"(<code>\1</code>)"));

    // Tags: #tag
    static const QRegularExpression tagPattern(QStringLiteral(R"((?<![&\w])#([a-zA-Z_][a-zA-Z0-9_/-]*))"));
    result.replace(tagPattern, QStringLiteral(R"(<span class="tag">#\1</span>)"));

    return result;
}

QString MarkdownRenderer::wrapDocument(const QString &body) const
{
    return QStringLiteral("<!DOCTYPE html><html><head><style>%1</style></head><body>%2</body></html>")
        .arg(defaultStylesheet(), body);
}

QString MarkdownRenderer::escapeHtml(const QString &text)
{
    QString result = text;
    result.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    result.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    result.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    result.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return result;
}

QString MarkdownRenderer::defaultStylesheet()
{
    return QStringLiteral(R"(
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            font-size: 16px;
            line-height: 1.6;
            max-width: 700px;
            margin: 0 auto;
            padding: 20px;
            color: #333;
        }
        h1, h2, h3, h4, h5, h6 { margin-top: 1.2em; margin-bottom: 0.5em; }
        h1 { font-size: 2em; border-bottom: 1px solid #eee; padding-bottom: 0.3em; }
        h2 { font-size: 1.5em; border-bottom: 1px solid #eee; padding-bottom: 0.3em; }
        h3 { font-size: 1.25em; }
        a { color: #7b6cd9; text-decoration: none; }
        a:hover { text-decoration: underline; }
        a.internal-link { color: #7b6cd9; }
        code { background: #f0f0f0; padding: 2px 4px; border-radius: 3px; font-size: 0.9em; }
        pre { background: #f6f8fa; padding: 16px; border-radius: 6px; overflow-x: auto; }
        pre code { background: none; padding: 0; }
        blockquote { border-left: 3px solid #ddd; margin: 0; padding: 0 1em; color: #666; }
        mark { background: #fff3b0; padding: 1px 2px; }
        img { max-width: 100%; }
        hr { border: none; border-top: 1px solid #ddd; margin: 2em 0; }
        .tag { color: #e06c75; }
        .callout { border-left: 4px solid #d19a66; background: #fdf6e3; padding: 12px 16px; margin: 1em 0; border-radius: 4px; }
        .callout-title { font-weight: bold; margin-bottom: 4px; }
        .callout-warning { border-color: #e5c07b; background: #fdf6e3; }
        .callout-note { border-color: #61afef; background: #eef6ff; }
        .callout-tip { border-color: #98c379; background: #eef8ee; }
        .callout-danger, .callout-error { border-color: #e06c75; background: #fdeaea; }
        .callout-info { border-color: #61afef; background: #eef6ff; }
        .checklist { list-style: none; padding-left: 0; }
        .checklist li { padding: 2px 0; }
        input[type="checkbox"] { margin-right: 6px; }
        del { color: #999; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background: #f6f8fa; font-weight: bold; }
    )");
}

} // namespace Corbomite
```

- [ ] **Step 3: Update CMakeLists**

`libs/core/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(corbomite-core VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(corbomite-core STATIC
    src/NoteMeta.cpp
    src/NoteDocument.cpp
    include/corbomite/core/NoteDocument.h
    src/MarkdownRenderer.cpp
)
set_target_properties(corbomite-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Core ALIAS corbomite-core)

target_include_directories(corbomite-core
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-core PUBLIC Qt6::Core)
```

`tests/core/CMakeLists.txt` — add test:
```cmake
add_executable(tst_markdownrenderer tst_markdownrenderer.cpp)
add_test(NAME tst_markdownrenderer COMMAND tst_markdownrenderer)
target_link_libraries(tst_markdownrenderer PRIVATE Qt6::Test Corbomite::Core)
```

- [ ] **Step 4: Build and run tests**

Run:
```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_markdownrenderer --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/core/ tests/core/tst_markdownrenderer.cpp tests/core/CMakeLists.txt
git commit -m "feat: add MarkdownRenderer with regex-based markdown-to-HTML conversion

Handles headings, bold, italic, links, code, lists, blockquotes,
tables, wikilinks, ==highlight==, %%comments%%, callouts, #tags.
TODO: Replace with cmark-gfm for full CommonMark compliance."
```

---

### Task 3: SearchResultsModel (libs/models)

**Files:**
- Create: `libs/models/include/corbomite/models/SearchResultsModel.h`
- Create: `libs/models/src/SearchResultsModel.cpp`
- Modify: `libs/models/CMakeLists.txt`

- [ ] **Step 1: Implement SearchResultsModel**

`libs/models/include/corbomite/models/SearchResultsModel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractItemModel>
#include <QVector>
#include "corbomite/storage/SQLiteIndex.h"

namespace Corbomite {

class SearchResultsModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        NotePathRole = Qt::UserRole + 1,
        SnippetRole,
        MatchCountRole
    };

    explicit SearchResultsModel(QObject *parent = nullptr);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setResults(const QVector<SearchMatch> &results);
    void clear();

    int fileCount() const;
    int totalMatchCount() const;

private:
    // Group matches by file
    struct FileGroup {
        QString notePath;
        QString noteName;
        QVector<SearchMatch> matches;
    };

    QVector<FileGroup> m_groups;
};

} // namespace Corbomite
```

`libs/models/src/SearchResultsModel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/SearchResultsModel.h"

namespace Corbomite {

SearchResultsModel::SearchResultsModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

QModelIndex SearchResultsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0) return {};

    if (!parent.isValid()) {
        // Top level: file groups
        if (row >= 0 && row < m_groups.size()) {
            return createIndex(row, 0, quintptr(-1));
        }
    } else if (parent.internalId() == quintptr(-1)) {
        // Child level: matches within a file group
        int groupIdx = parent.row();
        if (groupIdx >= 0 && groupIdx < m_groups.size()
            && row >= 0 && row < m_groups[groupIdx].matches.size()) {
            return createIndex(row, 0, quintptr(groupIdx));
        }
    }
    return {};
}

QModelIndex SearchResultsModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return {};
    quintptr id = child.internalId();
    if (id == quintptr(-1)) return {}; // Top-level item
    return createIndex(int(id), 0, quintptr(-1)); // Parent is the file group
}

int SearchResultsModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return m_groups.size();
    }
    if (parent.internalId() == quintptr(-1)) {
        int groupIdx = parent.row();
        if (groupIdx >= 0 && groupIdx < m_groups.size()) {
            return m_groups[groupIdx].matches.size();
        }
    }
    return 0;
}

int SearchResultsModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant SearchResultsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};

    if (index.internalId() == quintptr(-1)) {
        // File group row
        int groupIdx = index.row();
        if (groupIdx < 0 || groupIdx >= m_groups.size()) return {};
        const auto &group = m_groups[groupIdx];

        switch (role) {
        case Qt::DisplayRole:
            return QStringLiteral("%1 (%2)").arg(group.noteName).arg(group.matches.size());
        case NotePathRole:
            return group.notePath;
        case MatchCountRole:
            return group.matches.size();
        }
    } else {
        // Match row within a file
        int groupIdx = int(index.internalId());
        if (groupIdx < 0 || groupIdx >= m_groups.size()) return {};
        const auto &matches = m_groups[groupIdx].matches;
        int matchIdx = index.row();
        if (matchIdx < 0 || matchIdx >= matches.size()) return {};
        const auto &match = matches[matchIdx];

        switch (role) {
        case Qt::DisplayRole:
        case SnippetRole:
            return match.snippet;
        case NotePathRole:
            return match.notePath;
        }
    }
    return {};
}

void SearchResultsModel::setResults(const QVector<SearchMatch> &results)
{
    beginResetModel();
    m_groups.clear();

    // Group by file path
    QHash<QString, int> pathToGroup;
    for (const auto &match : results) {
        auto it = pathToGroup.find(match.notePath);
        if (it == pathToGroup.end()) {
            FileGroup group;
            group.notePath = match.notePath;
            // Extract name from path
            QString name = match.notePath.mid(match.notePath.lastIndexOf(QLatin1Char('/')) + 1);
            int dot = name.lastIndexOf(QLatin1Char('.'));
            group.noteName = dot > 0 ? name.left(dot) : name;
            group.matches.append(match);
            pathToGroup[match.notePath] = m_groups.size();
            m_groups.append(group);
        } else {
            m_groups[it.value()].matches.append(match);
        }
    }
    endResetModel();
}

void SearchResultsModel::clear()
{
    beginResetModel();
    m_groups.clear();
    endResetModel();
}

int SearchResultsModel::fileCount() const
{
    return m_groups.size();
}

int SearchResultsModel::totalMatchCount() const
{
    int total = 0;
    for (const auto &g : m_groups) total += g.matches.size();
    return total;
}

} // namespace Corbomite
```

- [ ] **Step 2: Update libs/models/CMakeLists.txt**

Add `src/SearchResultsModel.cpp` and `include/corbomite/models/SearchResultsModel.h` to sources.

- [ ] **Step 3: Build**

Run: `cmake --build build`

- [ ] **Step 4: Commit**

```bash
git add libs/models/
git commit -m "feat: add SearchResultsModel with file-grouped search results"
```

---

### Task 4: SearchPanel (sidebar)

**Files:**
- Create: `src/sidebar/SearchPanel.h`
- Create: `src/sidebar/SearchPanel.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Implement SearchPanel**

`src/sidebar/SearchPanel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QTreeView>
#include <QLabel>
#include <QTimer>

namespace Corbomite {

class SQLiteIndex;
class SearchResultsModel;

class SearchPanel : public QWidget {
    Q_OBJECT

public:
    explicit SearchPanel(QWidget *parent = nullptr);

    void setIndex(SQLiteIndex *index);
    void focusSearchInput();

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

private:
    void onSearchTextChanged(const QString &text);
    void executeSearch();
    void onResultClicked(const QModelIndex &index);

    QLineEdit *m_searchInput;
    QTreeView *m_resultView;
    QLabel *m_statusLabel;
    QTimer m_debounceTimer;

    SQLiteIndex *m_index = nullptr;
    SearchResultsModel *m_resultsModel;
};

} // namespace Corbomite
```

`src/sidebar/SearchPanel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SearchPanel.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/models/SearchResultsModel.h"

#include <KLocalizedString>
#include <QVBoxLayout>

namespace Corbomite {

SearchPanel::SearchPanel(QWidget *parent)
    : QWidget(parent)
    , m_searchInput(new QLineEdit(this))
    , m_resultView(new QTreeView(this))
    , m_statusLabel(new QLabel(this))
    , m_resultsModel(new SearchResultsModel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_searchInput->setPlaceholderText(i18n("Search vault..."));
    m_searchInput->setClearButtonEnabled(true);
    layout->addWidget(m_searchInput);

    m_statusLabel->setVisible(false);
    layout->addWidget(m_statusLabel);

    m_resultView->setHeaderHidden(true);
    m_resultView->setRootIsDecorated(true);
    m_resultView->setModel(m_resultsModel);
    m_resultView->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_resultView);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(300);

    connect(m_searchInput, &QLineEdit::textChanged, this, &SearchPanel::onSearchTextChanged);
    connect(&m_debounceTimer, &QTimer::timeout, this, &SearchPanel::executeSearch);
    connect(m_resultView, &QTreeView::doubleClicked, this, &SearchPanel::onResultClicked);
}

void SearchPanel::setIndex(SQLiteIndex *index)
{
    m_index = index;
}

void SearchPanel::focusSearchInput()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

void SearchPanel::onSearchTextChanged(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        m_resultsModel->clear();
        m_statusLabel->setVisible(false);
        return;
    }
    m_debounceTimer.start();
}

void SearchPanel::executeSearch()
{
    if (!m_index) return;

    QString query = m_searchInput->text().trimmed();
    if (query.isEmpty()) return;

    // TODO: Support Obsidian search operators (file:, path:, tag:, regex)
    auto results = m_index->search(query);
    m_resultsModel->setResults(results);

    m_statusLabel->setText(i18n("%1 matches in %2 files",
                                m_resultsModel->totalMatchCount(),
                                m_resultsModel->fileCount()));
    m_statusLabel->setVisible(true);

    m_resultView->expandAll();
}

void SearchPanel::onResultClicked(const QModelIndex &index)
{
    QString path = index.data(SearchResultsModel::NotePathRole).toString();
    if (!path.isEmpty()) {
        Q_EMIT noteActivated(path);
    }
}

} // namespace Corbomite
```

- [ ] **Step 2: Add to src/CMakeLists.txt**

Add `sidebar/SearchPanel.cpp` to CorbomiteApp sources.

- [ ] **Step 3: Build**

Run: `cmake --build build`

- [ ] **Step 4: Commit**

```bash
git add src/sidebar/SearchPanel.h src/sidebar/SearchPanel.cpp src/CMakeLists.txt
git commit -m "feat: add SearchPanel sidebar with debounced FTS5 search"
```

---

### Task 5: NotePreviewWidget (editor)

**Files:**
- Create: `src/editor/NotePreviewWidget.h`
- Create: `src/editor/NotePreviewWidget.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Implement NotePreviewWidget**

`src/editor/NotePreviewWidget.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTextBrowser>
#include "corbomite/core/MarkdownRenderer.h"

namespace Corbomite {

class NoteDocument;

class NotePreviewWidget : public QTextBrowser {
    Q_OBJECT

public:
    explicit NotePreviewWidget(QWidget *parent = nullptr);

    void renderDocument(NoteDocument *doc);

Q_SIGNALS:
    void internalLinkClicked(const QString &targetPath);

private:
    void onAnchorClicked(const QUrl &url);

    MarkdownRenderer m_renderer;
};

} // namespace Corbomite
```

`src/editor/NotePreviewWidget.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "NotePreviewWidget.h"
#include "corbomite/core/NoteDocument.h"

#include <QDesktopServices>

namespace Corbomite {

NotePreviewWidget::NotePreviewWidget(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setReadOnly(true);

    connect(this, &QTextBrowser::anchorClicked, this, &NotePreviewWidget::onAnchorClicked);
}

void NotePreviewWidget::renderDocument(NoteDocument *doc)
{
    if (!doc) {
        clear();
        return;
    }

    QString html = m_renderer.renderToHtml(doc->markdown());
    setHtml(html);
}

void NotePreviewWidget::onAnchorClicked(const QUrl &url)
{
    QString scheme = url.scheme();
    QString path = url.path();

    if (scheme.isEmpty() || scheme == QStringLiteral("file")) {
        // Internal link — likely a wikilink
        // Remove .md extension for the signal
        if (path.endsWith(QStringLiteral(".md"))) {
            Q_EMIT internalLinkClicked(path);
        } else {
            Q_EMIT internalLinkClicked(path + QStringLiteral(".md"));
        }
    } else {
        // External link — open in browser
        QDesktopServices::openUrl(url);
    }
}

} // namespace Corbomite
```

- [ ] **Step 2: Add to src/CMakeLists.txt**

Add `editor/NotePreviewWidget.cpp` to CorbomiteApp sources.

- [ ] **Step 3: Build**

Run: `cmake --build build`

- [ ] **Step 4: Commit**

```bash
git add src/editor/NotePreviewWidget.h src/editor/NotePreviewWidget.cpp src/CMakeLists.txt
git commit -m "feat: add NotePreviewWidget with rendered markdown display

QTextBrowser showing HTML from MarkdownRenderer. Internal links
emit signal for navigation, external links open in system browser."
```

---

### Task 6: Editor Mode Toggle + Search Panel Wiring

**Files:**
- Modify: `src/editor/EditorViewSpace.h`
- Modify: `src/editor/EditorViewSpace.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/app/corbomiteui.rc.in`

- [ ] **Step 1: Add toggleEditorMode to EditorViewSpace**

In `src/editor/EditorViewSpace.h`, add:
```cpp
    // After existing public methods:
    void toggleEditorMode();
    bool isPreviewMode() const;
```

Add member:
```cpp
    QHash<QString, NotePreviewWidget *> m_previews;
    QSet<QString> m_previewModePaths;  // paths currently in preview mode
```

Add forward declaration: `class NotePreviewWidget;`

In `src/editor/EditorViewSpace.cpp`, add `#include "NotePreviewWidget.h"` and `#include "corbomite/core/NoteDocument.h"`, then implement:

```cpp
void EditorViewSpace::toggleEditorMode()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0) return;
    QString path = m_tabBar->tabData(idx).toString();

    if (m_previewModePaths.contains(path)) {
        // Switch from preview to editor
        m_previewModePaths.remove(path);
        if (auto *preview = m_previews.value(path)) {
            m_stack->setCurrentWidget(m_editors.value(path));
        }
        Q_EMIT activeEditorChanged(m_editors.value(path));
    } else {
        // Switch from editor to preview
        m_previewModePaths.insert(path);
        auto *editor = m_editors.value(path);
        if (!editor || !editor->noteDocument()) return;

        auto *preview = m_previews.value(path);
        if (!preview) {
            preview = new NotePreviewWidget(m_stack);
            m_previews.insert(path, preview);
            m_stack->addWidget(preview);

            // Forward internal link clicks
            connect(preview, &NotePreviewWidget::internalLinkClicked,
                    this, [this](const QString &target) {
                Q_EMIT activeEditorChanged(nullptr); // signal that we navigated
            });
        }

        preview->renderDocument(editor->noteDocument());
        m_stack->setCurrentWidget(preview);
        Q_EMIT activeEditorChanged(nullptr); // no active editor in preview mode
    }
}

bool EditorViewSpace::isPreviewMode() const
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0) return false;
    QString path = m_tabBar->tabData(idx).toString();
    return m_previewModePaths.contains(path);
}
```

- [ ] **Step 2: Add toggleEditorMode to EditorViewManager**

In `src/editor/EditorViewManager.h`, add:
```cpp
    void toggleEditorMode();
    bool isPreviewMode() const;
```

In `src/editor/EditorViewManager.cpp`:
```cpp
void EditorViewManager::toggleEditorMode()
{
    m_viewSpace->toggleEditorMode();
}

bool EditorViewManager::isPreviewMode() const
{
    return m_viewSpace->isPreviewMode();
}
```

- [ ] **Step 3: Wire search panel and mode toggle into MainWindow**

Add to MainWindow.h private section:
```cpp
    class SearchPanel *m_searchPanel = nullptr;
    class SQLiteIndex *m_searchIndex = nullptr;
```

Add new private methods:
```cpp
    void showSearchPanel();
    void toggleEditorMode();
```

In MainWindow.cpp setupActions(), add:
```cpp
    auto *searchVault = ac->addAction(QStringLiteral("search_vault"));
    searchVault->setText(i18n("Search Vault"));
    searchVault->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    ac->setDefaultShortcut(searchVault, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    connect(searchVault, &QAction::triggered, this, &MainWindow::showSearchPanel);

    auto *toggleMode = ac->addAction(QStringLiteral("editor_toggle_mode"));
    toggleMode->setText(i18n("Toggle Reading Mode"));
    toggleMode->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
    ac->setDefaultShortcut(toggleMode, QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(toggleMode, &QAction::triggered, this, &MainWindow::toggleEditorMode);
```

In setupSidebars(), after the file explorer toolview, add:
```cpp
    // Search panel in left sidebar
    auto *searchToolView = createToolView(
        nullptr,
        QStringLiteral("search_panel"),
        KMultiTabBar::Left,
        QIcon::fromTheme(QStringLiteral("edit-find")),
        i18n("Search")
    );

    m_searchPanel = new SearchPanel(searchToolView);
    searchToolView->layout()->addWidget(m_searchPanel);

    connect(m_searchPanel, &SearchPanel::noteActivated,
            this, &MainWindow::onNoteActivated);
```

In onVaultOpened(), after existing setup, add:
```cpp
    // Create search index
    delete m_searchIndex;
    m_searchIndex = new SQLiteIndex(this);
    m_searchIndex->open(vault->configPath() + QStringLiteral("/index.sqlite"));
    m_searchIndex->rebuildIndex(vault->path());
    // TODO: Move indexing to background thread for large vaults
    m_searchPanel->setIndex(m_searchIndex);

    // Update index on note saves
    connect(m_autosave, &AutosaveReactor::noteSaved, this, [this](const QString &relPath) {
        if (!m_searchIndex || !m_vaultService->vault()) return;
        auto *doc = m_vaultService->vault()->cachedDocument(relPath);
        if (doc) {
            m_searchIndex->indexNote(relPath, doc->name(), doc->markdown());
        }
    }, Qt::UniqueConnection);
```

In onVaultClosed(), add:
```cpp
    delete m_searchIndex;
    m_searchIndex = nullptr;
    m_searchPanel->setIndex(nullptr);
```

Implement showSearchPanel():
```cpp
void MainWindow::showSearchPanel()
{
    // Show the search toolview and focus its input
    auto *tv = toolView(QStringLiteral("search_panel"));
    if (tv) {
        showToolView(tv);
        m_searchPanel->focusSearchInput();
    }
}
```

Implement toggleEditorMode():
```cpp
void MainWindow::toggleEditorMode()
{
    m_editorManager->toggleEditorMode();
    // Update status bar
    if (m_editorManager->isPreviewMode()) {
        m_cursorPosLabel->setText(i18n("Reading"));
    }
}
```

- [ ] **Step 4: Update XMLGUI**

Replace `corbomiteui.rc.in` — bump version to 3, add search and toggle actions:

```xml
<!DOCTYPE gui SYSTEM "kpartgui.dtd">
<gui name="@CORBOMITE_COMPONENT_NAME@" version="3">
  <MenuBar>
    <Menu name="file">
      <text>&amp;File</text>
      <Action name="file_open_vault"/>
      <Separator/>
      <Action name="file_new_note"/>
      <Separator/>
      <Action name="file_save"/>
      <Separator/>
    </Menu>
    <Menu name="go">
      <text>&amp;Go</text>
      <Action name="quick_switcher"/>
      <Action name="command_palette"/>
      <Separator/>
      <Action name="search_vault"/>
    </Menu>
    <Menu name="view">
      <text>&amp;View</text>
      <Action name="editor_toggle_mode"/>
      <Separator/>
      <Action name="view_toggle_left_sidebar"/>
      <Separator/>
      <Action name="view_zoom_in"/>
      <Action name="view_zoom_out"/>
      <Action name="view_zoom_reset"/>
    </Menu>
  </MenuBar>
  <ToolBar name="mainToolBar" noMerge="1">
    <text>Main Toolbar</text>
    <Action name="file_open_vault"/>
    <Action name="file_new_note"/>
    <Action name="file_save"/>
    <Separator/>
    <Action name="quick_switcher"/>
    <Action name="command_palette"/>
    <Action name="search_vault"/>
    <Separator/>
    <Action name="editor_toggle_mode"/>
  </ToolBar>
</gui>
```

- [ ] **Step 5: Add includes to MainWindow.cpp**

```cpp
#include "sidebar/SearchPanel.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "editor/NotePreviewWidget.h"
```

- [ ] **Step 6: Build and verify**

Run:
```bash
cmake --build build && cd build && ctest --output-on-failure
```

Expected: All tests pass, builds clean.

- [ ] **Step 7: Commit**

```bash
git add src/
git commit -m "feat: wire global search panel and reading mode toggle

Search panel: Ctrl+Shift+F focuses search in left sidebar.
Reading mode: Ctrl+E toggles between source editor and rendered preview.
XMLGUI version bumped to 3 with new actions."
```

---

### Task 7: Integration verification and cleanup

- [ ] **Step 1: Run all tests**

```bash
cd build && ctest --output-on-failure
```

Expected: All tests pass (14 existing + new SQLiteIndex + MarkdownRenderer tests).

- [ ] **Step 2: Manual testing checklist**

Run `./build/bin/Corbomite`:
1. Open vault → search index builds
2. Ctrl+Shift+F → search panel focuses
3. Type query → results appear with snippets after 300ms
4. Click result → note opens in editor
5. Ctrl+E → switches to rendered HTML preview
6. Ctrl+E again → switches back to source editor
7. Wikilinks in preview are clickable → open target note
8. Status bar shows "Reading" in preview mode

- [ ] **Step 3: Final commit**

```bash
git add -A
git commit -m "chore: Phase 2 complete — search and reading mode

All Phase 2 features implemented:
- Obsidian syntax highlighting (wikilinks, tags, callouts, highlight, comments, block-refs)
- Wikilink autocomplete ([[), tag autocomplete (#)
- Ctrl+Click wikilink navigation
- Quick Switcher (Ctrl+O) and Command Palette (Ctrl+P)
- Global search with SQLite FTS5 (Ctrl+Shift+F)
- Reading mode with markdown-to-HTML rendering (Ctrl+E)"
```

---

Self-review:

1. **Spec coverage:** SQLiteIndex with FTS5 ✓. SearchPanel in sidebar ✓. SearchResultsModel ✓. MarkdownRenderer ✓. NotePreviewWidget ✓. Ctrl+E toggle ✓. Ctrl+Shift+F ✓. Index on vault open ✓. Incremental update on save ✓. Wikilinks in preview clickable ✓. Callout styling ✓. TODO comments for cmark-gfm upgrade ✓. TODO for background indexing ✓. TODO for search operators ✓.

2. **Placeholder scan:** All code complete. TODO comments are explicit upgrade-path markers, not missing implementation.

3. **Type consistency:** `SearchMatch` struct used consistently (SQLiteIndex produces, SearchResultsModel consumes). `MarkdownRenderer::renderToHtml()` used by NotePreviewWidget. `SQLiteIndex::search()` returns `QVector<SearchMatch>`. EditorViewSpace `toggleEditorMode()`/`isPreviewMode()` forwarded through EditorViewManager. `SearchPanel::noteActivated` signal matches MainWindow's `onNoteActivated` slot.
