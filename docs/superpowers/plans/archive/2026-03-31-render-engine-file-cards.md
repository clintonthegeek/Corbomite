# Unified Render Engine & Canvas File Cards Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a unified markdown rendering interface used by all display contexts, then implement canvas file cards as the first consumer that proves the interface works.

**Architecture:** Abstract `MarkdownRenderEngine` interface in `libs/core` with `RegexRenderEngine` adapter wrapping existing `MarkdownRenderer`. Canvas `FileCardItem` receives pre-rendered `RenderedDocument` objects via dependency injection. `ConnectableItem` interface enables edges to connect to both text and file cards.

**Tech Stack:** C++20, Qt6 (QTextDocument, QGraphicsView), KDE Frameworks 6

---

### Task 1: RenderProfile and RenderOptions structs

**Files:**
- Create: `libs/core/include/corbomite/core/RenderProfile.h`
- Create: `libs/core/include/corbomite/core/RenderOptions.h`

- [ ] **Step 1: Create RenderProfile.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

struct RenderProfile {
    QString name;

    // Styling
    int baseFontSizePt = 14;
    int maxWidthPx = 0;           // 0 = no limit (fill container)
    int marginPx = 16;
    bool showFrontmatter = false;

    // Content
    bool renderImages = true;
    bool renderCodeHighlighting = true;

    static RenderProfile readingMode()
    {
        RenderProfile p;
        p.name = QStringLiteral("ReadingMode");
        p.baseFontSizePt = 16;
        p.maxWidthPx = 700;
        p.marginPx = 20;
        return p;
    }

    static RenderProfile canvasCard()
    {
        RenderProfile p;
        p.name = QStringLiteral("CanvasCard");
        p.baseFontSizePt = 11;
        p.maxWidthPx = 0;
        p.marginPx = 4;
        return p;
    }

    static RenderProfile hoverPreview()
    {
        RenderProfile p;
        p.name = QStringLiteral("HoverPreview");
        p.baseFontSizePt = 11;
        p.maxWidthPx = 0;
        p.marginPx = 8;
        p.renderImages = false;
        return p;
    }
};

} // namespace Corbomite
```

- [ ] **Step 2: Create RenderOptions.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <optional>

namespace Corbomite {

struct RenderOptions {
    // Subpath extraction: render only content under this heading/block
    // Empty = render full document
    // "#heading" = render from that heading to next same-level heading
    // "#^block-id" = render only the paragraph containing that block ID
    QString subpath;

    // Profile overrides (applied on top of the engine's default profile)
    std::optional<int> baseFontSizePt;
    std::optional<int> maxWidthPx;
    std::optional<int> marginPx;

    // Vault context for resolving links and embeds
    QString vaultRoot;
    QString notePath;
};

} // namespace Corbomite
```

- [ ] **Step 3: Verify build**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds (headers not yet used, but must parse)

- [ ] **Step 4: Commit**

```bash
git add libs/core/include/corbomite/core/RenderProfile.h libs/core/include/corbomite/core/RenderOptions.h
git commit -m "feat: add RenderProfile and RenderOptions structs"
```

---

### Task 2: RenderedDocument wrapper

**Files:**
- Create: `libs/core/include/corbomite/core/RenderedDocument.h`
- Create: `libs/core/src/RenderedDocument.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Create: `tests/core/tst_rendereddocument.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/core/tst_rendereddocument.cpp`:

```cpp
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
```

- [ ] **Step 2: Create RenderedDocument.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

class QTextDocument;
class QWidget;

namespace Corbomite {

class RenderedDocument {
public:
    ~RenderedDocument();

    QTextDocument *toQTextDocument() const;
    QWidget *createWidget(QWidget *parent = nullptr) const;

    static std::unique_ptr<RenderedDocument> fromQTextDocument(std::unique_ptr<QTextDocument> doc);

private:
    explicit RenderedDocument(std::unique_ptr<QTextDocument> doc);

    std::unique_ptr<QTextDocument> m_document;
};

} // namespace Corbomite
```

- [ ] **Step 3: Create RenderedDocument.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/RenderedDocument.h"

#include <QTextBrowser>
#include <QTextDocument>

namespace Corbomite {

RenderedDocument::RenderedDocument(std::unique_ptr<QTextDocument> doc)
    : m_document(std::move(doc))
{
}

RenderedDocument::~RenderedDocument() = default;

QTextDocument *RenderedDocument::toQTextDocument() const
{
    return m_document.get();
}

QWidget *RenderedDocument::createWidget(QWidget *parent) const
{
    auto *browser = new QTextBrowser(parent);
    browser->setOpenLinks(false);
    browser->setOpenExternalLinks(false);
    browser->setReadOnly(true);

    // Clone the document content into the browser
    browser->setHtml(m_document->toHtml());
    return browser;
}

std::unique_ptr<RenderedDocument> RenderedDocument::fromQTextDocument(std::unique_ptr<QTextDocument> doc)
{
    return std::unique_ptr<RenderedDocument>(new RenderedDocument(std::move(doc)));
}

} // namespace Corbomite
```

- [ ] **Step 4: Add to libs/core/CMakeLists.txt**

Add `src/RenderedDocument.cpp` to the `add_library` source list. The `target_link_libraries` line already has `Qt6::Gui` which provides `QTextDocument`. Add `Qt6::Widgets` for `QTextBrowser`:

In `libs/core/CMakeLists.txt`, change the `find_package` line:
```cmake
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)
```

Add the source to the library:
```cmake
add_library(corbomite-core STATIC
    src/NoteMeta.cpp
    src/NoteDocument.cpp
    include/corbomite/core/NoteDocument.h
    src/MarkdownRenderer.cpp
    src/RenderedDocument.cpp
)
```

Add `Qt6::Widgets` to the link line:
```cmake
target_link_libraries(corbomite-core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg KF6::SyntaxHighlighting jkqtmathtext mmdr)
```

- [ ] **Step 5: Add test to tests/core/CMakeLists.txt**

Append:
```cmake
add_executable(tst_rendereddocument tst_rendereddocument.cpp)
add_test(NAME tst_rendereddocument COMMAND tst_rendereddocument)
target_link_libraries(tst_rendereddocument PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_rendereddocument PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 6: Run test to verify it fails**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -10`
Expected: Build fails because `RenderedDocument.h` or `.cpp` may have issues — iterate until build succeeds, then:

Run: `cd build && ctest -R tst_rendereddocument --output-on-failure`
Expected: PASS (all 3 tests)

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/RenderedDocument.h libs/core/src/RenderedDocument.cpp libs/core/CMakeLists.txt tests/core/tst_rendereddocument.cpp tests/core/CMakeLists.txt
git commit -m "feat: add RenderedDocument wrapper for rendered markdown output"
```

---

### Task 3: MarkdownRenderEngine abstract interface and subpath extraction

**Files:**
- Create: `libs/core/include/corbomite/core/MarkdownRenderEngine.h`
- Create: `libs/core/src/MarkdownRenderEngine.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Create: `tests/core/tst_subpath.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the subpath extraction tests**

Create `tests/core/tst_subpath.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/MarkdownRenderEngine.h"

class TestSubpath : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testEmptySubpathReturnsFullDocument()
    {
        QString md = QStringLiteral("# Title\n\nSome content\n\n## Section\n\nMore content");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QString());
        QCOMPARE(result, md);
    }

    void testHeadingSubpath()
    {
        QString md = QStringLiteral("# Title\n\nIntro\n\n## Section A\n\nContent A\n\n## Section B\n\nContent B");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#Section A"));
        QVERIFY(result.contains(QStringLiteral("## Section A")));
        QVERIFY(result.contains(QStringLiteral("Content A")));
        QVERIFY(!result.contains(QStringLiteral("Section B")));
        QVERIFY(!result.contains(QStringLiteral("Intro")));
    }

    void testHeadingSubpathIncludesSubsections()
    {
        QString md = QStringLiteral("# Title\n\n## Parent\n\nParent content\n\n### Child\n\nChild content\n\n## Sibling\n\nSibling content");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#Parent"));
        QVERIFY(result.contains(QStringLiteral("## Parent")));
        QVERIFY(result.contains(QStringLiteral("### Child")));
        QVERIFY(result.contains(QStringLiteral("Child content")));
        QVERIFY(!result.contains(QStringLiteral("Sibling")));
    }

    void testHeadingSubpathAtEOF()
    {
        QString md = QStringLiteral("# Title\n\n## Last Section\n\nFinal content");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#Last Section"));
        QVERIFY(result.contains(QStringLiteral("## Last Section")));
        QVERIFY(result.contains(QStringLiteral("Final content")));
    }

    void testHeadingCaseInsensitive()
    {
        QString md = QStringLiteral("## My Heading\n\nContent here");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#my heading"));
        QVERIFY(result.contains(QStringLiteral("## My Heading")));
        QVERIFY(result.contains(QStringLiteral("Content here")));
    }

    void testNonexistentHeading()
    {
        QString md = QStringLiteral("# Title\n\nContent");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#Nonexistent"));
        QVERIFY(result.isEmpty());
    }

    void testBlockIdSubpath()
    {
        QString md = QStringLiteral("First paragraph.\n\nThis is the target paragraph with some\nimportant content. ^my-block\n\nThird paragraph.");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#^my-block"));
        QVERIFY(result.contains(QStringLiteral("target paragraph")));
        QVERIFY(result.contains(QStringLiteral("important content")));
        QVERIFY(!result.contains(QStringLiteral("^my-block")));
        QVERIFY(!result.contains(QStringLiteral("First paragraph")));
        QVERIFY(!result.contains(QStringLiteral("Third paragraph")));
    }

    void testNonexistentBlockId()
    {
        QString md = QStringLiteral("# Title\n\nContent");
        QString result = Corbomite::MarkdownRenderEngine::extractSubpath(md, QStringLiteral("#^nonexistent"));
        QVERIFY(result.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestSubpath)
#include "tst_subpath.moc"
```

- [ ] **Step 2: Create MarkdownRenderEngine.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>
#include <QString>

#include "RenderProfile.h"
#include "RenderOptions.h"
#include "RenderedDocument.h"

namespace Corbomite {

class MarkdownRenderEngine {
public:
    virtual ~MarkdownRenderEngine() = default;

    virtual std::unique_ptr<RenderedDocument> render(
        const QString &markdown,
        const RenderOptions &options = {}) const = 0;

    void setProfile(const RenderProfile &profile) { m_profile = profile; }
    RenderProfile profile() const { return m_profile; }

    // Shared utility: extract content for a subpath from raw markdown
    static QString extractSubpath(const QString &markdown, const QString &subpath);

protected:
    RenderProfile m_profile;
};

} // namespace Corbomite
```

- [ ] **Step 3: Create MarkdownRenderEngine.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MarkdownRenderEngine.h"

#include <QRegularExpression>
#include <QStringList>

namespace Corbomite {

QString MarkdownRenderEngine::extractSubpath(const QString &markdown, const QString &subpath)
{
    if (subpath.isEmpty())
        return markdown;

    // Block ID: #^block-id
    if (subpath.startsWith(QStringLiteral("#^"))) {
        const QString blockId = subpath.mid(2); // strip "#^"
        const QStringList lines = markdown.split(QLatin1Char('\n'));

        // Find the line containing ^block-id
        int targetLine = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].contains(QStringLiteral("^") + blockId)) {
                targetLine = i;
                break;
            }
        }

        if (targetLine < 0)
            return {};

        // Expand to surrounding paragraph (contiguous non-empty lines)
        int start = targetLine;
        while (start > 0 && !lines[start - 1].trimmed().isEmpty())
            --start;

        int end = targetLine;
        while (end < lines.size() - 1 && !lines[end + 1].trimmed().isEmpty())
            ++end;

        // Collect paragraph lines, stripping the block ID marker
        QStringList result;
        const QRegularExpression blockIdPattern(
            QStringLiteral(R"(\s*\^)") + QRegularExpression::escape(blockId));
        for (int i = start; i <= end; ++i) {
            QString line = lines[i];
            line.remove(blockIdPattern);
            result.append(line);
        }

        return result.join(QLatin1Char('\n')).trimmed();
    }

    // Heading: #heading-text
    if (subpath.startsWith(QLatin1Char('#'))) {
        const QString headingText = subpath.mid(1).trimmed(); // strip leading "#"
        const QStringList lines = markdown.split(QLatin1Char('\n'));

        static const QRegularExpression headingPattern(QStringLiteral(R"(^(#{1,6})\s+(.+)$)"));

        // Find the matching heading
        int startLine = -1;
        int headingLevel = 0;
        for (int i = 0; i < lines.size(); ++i) {
            auto match = headingPattern.match(lines[i]);
            if (match.hasMatch()) {
                if (match.captured(2).trimmed().compare(headingText, Qt::CaseInsensitive) == 0) {
                    startLine = i;
                    headingLevel = match.captured(1).length();
                    break;
                }
            }
        }

        if (startLine < 0)
            return {};

        // Find the end: next heading of equal or higher level, or EOF
        int endLine = lines.size(); // exclusive
        for (int i = startLine + 1; i < lines.size(); ++i) {
            auto match = headingPattern.match(lines[i]);
            if (match.hasMatch()) {
                int level = match.captured(1).length();
                if (level <= headingLevel) {
                    endLine = i;
                    break;
                }
            }
        }

        // Collect lines from startLine to endLine (exclusive)
        QStringList result;
        for (int i = startLine; i < endLine; ++i) {
            result.append(lines[i]);
        }

        // Trim trailing empty lines
        while (!result.isEmpty() && result.last().trimmed().isEmpty())
            result.removeLast();

        return result.join(QLatin1Char('\n'));
    }

    return markdown;
}

} // namespace Corbomite
```

- [ ] **Step 4: Add source to libs/core/CMakeLists.txt**

Add `src/MarkdownRenderEngine.cpp` to the `add_library` source list:

```cmake
add_library(corbomite-core STATIC
    src/NoteMeta.cpp
    src/NoteDocument.cpp
    include/corbomite/core/NoteDocument.h
    src/MarkdownRenderer.cpp
    src/RenderedDocument.cpp
    src/MarkdownRenderEngine.cpp
)
```

- [ ] **Step 5: Add test to tests/core/CMakeLists.txt**

Append:
```cmake
add_executable(tst_subpath tst_subpath.cpp)
add_test(NAME tst_subpath COMMAND tst_subpath)
target_link_libraries(tst_subpath PRIVATE Qt6::Test Corbomite::Core)
```

- [ ] **Step 6: Build and run tests**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

Run: `cd build && ctest -R tst_subpath --output-on-failure`
Expected: All 8 tests PASS

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/MarkdownRenderEngine.h libs/core/src/MarkdownRenderEngine.cpp libs/core/CMakeLists.txt tests/core/tst_subpath.cpp tests/core/CMakeLists.txt
git commit -m "feat: add MarkdownRenderEngine interface with subpath extraction"
```

---

### Task 4: RegexRenderEngine adapter

**Files:**
- Create: `libs/core/include/corbomite/core/RegexRenderEngine.h`
- Create: `libs/core/src/RegexRenderEngine.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Create: `tests/core/tst_renderengine.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/core/tst_renderengine.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTextDocument>
#include "corbomite/core/RegexRenderEngine.h"

class TestRenderEngine : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testRenderProducesDocument()
    {
        Corbomite::RegexRenderEngine engine;
        auto result = engine.render(QStringLiteral("# Hello\n\nWorld"));
        QVERIFY(result != nullptr);
        QVERIFY(result->toQTextDocument() != nullptr);
        QVERIFY(result->toQTextDocument()->toPlainText().contains(QStringLiteral("Hello")));
        QVERIFY(result->toQTextDocument()->toPlainText().contains(QStringLiteral("World")));
    }

    void testEmptyMarkdown()
    {
        Corbomite::RegexRenderEngine engine;
        auto result = engine.render(QString());
        QVERIFY(result != nullptr);
        QVERIFY(result->toQTextDocument() != nullptr);
    }

    void testProfileAffectsOutput()
    {
        Corbomite::RegexRenderEngine engine;

        engine.setProfile(Corbomite::RenderProfile::readingMode());
        auto reading = engine.render(QStringLiteral("# Test"));
        QString readingHtml = reading->toQTextDocument()->toHtml();

        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        auto canvas = engine.render(QStringLiteral("# Test"));
        QString canvasHtml = canvas->toQTextDocument()->toHtml();

        // They should produce different results (different font sizes at minimum)
        QVERIFY(readingHtml != canvasHtml);
    }

    void testRenderWithSubpath()
    {
        Corbomite::RegexRenderEngine engine;
        QString md = QStringLiteral("# Title\n\nIntro\n\n## Section\n\nSection content");
        Corbomite::RenderOptions opts;
        opts.subpath = QStringLiteral("#Section");
        auto result = engine.render(md, opts);
        QVERIFY(result != nullptr);
        QString text = result->toQTextDocument()->toPlainText();
        QVERIFY(text.contains(QStringLiteral("Section content")));
        QVERIFY(!text.contains(QStringLiteral("Intro")));
    }

    void testOptionsOverrideProfile()
    {
        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::readingMode()); // 16pt font

        Corbomite::RenderOptions opts;
        opts.baseFontSizePt = 20;

        auto result = engine.render(QStringLiteral("Hello"), opts);
        QString html = result->toQTextDocument()->toHtml();
        QVERIFY(html.contains(QStringLiteral("20")));
    }
};

QTEST_MAIN(TestRenderEngine)
#include "tst_renderengine.moc"
```

- [ ] **Step 2: Create RegexRenderEngine.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MarkdownRenderEngine.h"
#include "MarkdownRenderer.h"

namespace Corbomite {

class RegexRenderEngine : public MarkdownRenderEngine {
public:
    std::unique_ptr<RenderedDocument> render(
        const QString &markdown,
        const RenderOptions &options = {}) const override;

private:
    MarkdownRenderer m_legacyRenderer;

    QString buildStylesheet(const RenderOptions &options) const;
};

} // namespace Corbomite
```

- [ ] **Step 3: Create RegexRenderEngine.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/RegexRenderEngine.h"

#include <QTextDocument>

namespace Corbomite {

std::unique_ptr<RenderedDocument> RegexRenderEngine::render(
    const QString &markdown,
    const RenderOptions &options) const
{
    // 1. Extract subpath if requested
    QString md = markdown;
    if (!options.subpath.isEmpty()) {
        md = extractSubpath(markdown, options.subpath);
    }

    // 2. Render markdown to HTML via legacy renderer
    QString html = m_legacyRenderer.renderToHtml(md);

    // 3. Apply profile-specific CSS overrides
    QString styleOverrides = buildStylesheet(options);
    if (!styleOverrides.isEmpty()) {
        // Insert style overrides before </head>
        html.replace(QStringLiteral("</head>"),
                     QStringLiteral("<style>") + styleOverrides + QStringLiteral("</style></head>"));
    }

    // 4. Create QTextDocument from HTML
    auto doc = std::make_unique<QTextDocument>();
    doc->setHtml(html);

    return RenderedDocument::fromQTextDocument(std::move(doc));
}

QString RegexRenderEngine::buildStylesheet(const RenderOptions &options) const
{
    int fontSize = options.baseFontSizePt.value_or(m_profile.baseFontSizePt);
    int maxWidth = options.maxWidthPx.value_or(m_profile.maxWidthPx);
    int margin = options.marginPx.value_or(m_profile.marginPx);

    // Only emit overrides if they differ from the legacy renderer defaults
    // (legacy defaults: font-size 16px, max-width 700px, padding 20px)
    QString css;

    bool needsBody = (fontSize != 16 || maxWidth != 700 || margin != 20);
    if (needsBody) {
        css += QStringLiteral("body { ");
        css += QStringLiteral("font-size: %1px; ").arg(fontSize);
        if (maxWidth > 0) {
            css += QStringLiteral("max-width: %1px; ").arg(maxWidth);
        } else {
            css += QStringLiteral("max-width: none; ");
        }
        css += QStringLiteral("padding: %1px; ").arg(margin);
        css += QStringLiteral("} ");
    }

    return css;
}

} // namespace Corbomite
```

- [ ] **Step 4: Add source to libs/core/CMakeLists.txt**

Add `src/RegexRenderEngine.cpp` to the `add_library` source list:

```cmake
add_library(corbomite-core STATIC
    src/NoteMeta.cpp
    src/NoteDocument.cpp
    include/corbomite/core/NoteDocument.h
    src/MarkdownRenderer.cpp
    src/RenderedDocument.cpp
    src/MarkdownRenderEngine.cpp
    src/RegexRenderEngine.cpp
)
```

- [ ] **Step 5: Add test to tests/core/CMakeLists.txt**

Append:
```cmake
add_executable(tst_renderengine tst_renderengine.cpp)
add_test(NAME tst_renderengine COMMAND tst_renderengine)
target_link_libraries(tst_renderengine PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_renderengine PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 6: Build and run tests**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

Run: `cd build && ctest -R tst_renderengine --output-on-failure`
Expected: All 5 tests PASS

Also run existing renderer tests to verify no regression:
Run: `cd build && ctest -R tst_markdownrenderer --output-on-failure`
Expected: All tests PASS

- [ ] **Step 7: Commit**

```bash
git add libs/core/include/corbomite/core/RegexRenderEngine.h libs/core/src/RegexRenderEngine.cpp libs/core/CMakeLists.txt tests/core/tst_renderengine.cpp tests/core/CMakeLists.txt
git commit -m "feat: add RegexRenderEngine wrapping existing MarkdownRenderer"
```

---

### Task 5: Migrate NotePreviewWidget to use render engine

**Files:**
- Modify: `src/editor/NotePreviewWidget.h`
- Modify: `src/editor/NotePreviewWidget.cpp`

- [ ] **Step 1: Update NotePreviewWidget.h**

Replace the `MarkdownRenderer m_renderer;` member with `MarkdownRenderEngine *m_engine = nullptr;` and add a setter. Change the include from `MarkdownRenderer.h` to `MarkdownRenderEngine.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTextBrowser>

namespace Corbomite {

class MarkdownRenderEngine;
class NoteDocument;

class NotePreviewWidget : public QTextBrowser {
    Q_OBJECT

public:
    explicit NotePreviewWidget(QWidget *parent = nullptr);

    void setRenderEngine(MarkdownRenderEngine *engine);
    void renderDocument(NoteDocument *doc);

Q_SIGNALS:
    void internalLinkClicked(const QString &targetPath);

private:
    void onAnchorClicked(const QUrl &url);

    MarkdownRenderEngine *m_engine = nullptr;
};

} // namespace Corbomite
```

- [ ] **Step 2: Update NotePreviewWidget.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "NotePreviewWidget.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/MarkdownRenderEngine.h"

#include <QDesktopServices>
#include <QTextDocument>

namespace Corbomite {

NotePreviewWidget::NotePreviewWidget(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setReadOnly(true);

    connect(this, &QTextBrowser::anchorClicked, this, &NotePreviewWidget::onAnchorClicked);
}

void NotePreviewWidget::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_engine = engine;
}

void NotePreviewWidget::renderDocument(NoteDocument *doc)
{
    if (!doc || !m_engine) {
        clear();
        return;
    }

    auto rendered = m_engine->render(doc->markdown());
    setHtml(rendered->toQTextDocument()->toHtml());
}

void NotePreviewWidget::onAnchorClicked(const QUrl &url)
{
    QString scheme = url.scheme();
    QString path = url.path();

    if (scheme.isEmpty() || scheme == QStringLiteral("file")) {
        if (path.endsWith(QStringLiteral(".md"))) {
            Q_EMIT internalLinkClicked(path);
        } else {
            Q_EMIT internalLinkClicked(path + QStringLiteral(".md"));
        }
    } else {
        QDesktopServices::openUrl(url);
    }
}

} // namespace Corbomite
```

- [ ] **Step 3: Wire up the render engine in the app**

The engine needs to be created somewhere and injected into `NotePreviewWidget`. Find where `NotePreviewWidget` instances are created (in `EditorViewSpace`) and inject the engine there. The engine should be owned by a long-lived object — `EditorViewManager` or `MainWindow`.

Search for `NotePreviewWidget` construction in `EditorViewSpace.cpp` and add `setRenderEngine()` calls after construction. The engine instance should be created in `MainWindow` or `EditorViewManager` and passed down.

Read `EditorViewSpace.h` and `EditorViewManager.h` to find the right injection point, then:

- Create the `RegexRenderEngine` in `MainWindow` (or `EditorViewManager`) with `ReadingMode` profile
- Pass it to `EditorViewSpace` which passes it to `NotePreviewWidget`

The exact wiring depends on the current code — read and adapt.

- [ ] **Step 4: Build and run the app to verify reading mode still works**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

Run: `./build/Corbomite` — open a vault, open a note, press Ctrl+E to toggle reading mode
Expected: Reading mode renders correctly, same as before

- [ ] **Step 5: Run all existing tests**

Run: `cd build && ctest --output-on-failure`
Expected: All tests PASS

- [ ] **Step 6: Commit**

```bash
git add src/editor/NotePreviewWidget.h src/editor/NotePreviewWidget.cpp
git commit -m "refactor: migrate NotePreviewWidget to MarkdownRenderEngine"
```

Note: Include any other files modified for wiring (EditorViewSpace, EditorViewManager, MainWindow).

---

### Task 6: ConnectableItem interface for edge connections

**Files:**
- Create: `libs/canvas/include/canvas/ConnectableItem.h`
- Modify: `libs/canvas/include/canvas/TextCardItem.h`
- Modify: `libs/canvas/src/TextCardItem.cpp`
- Modify: `libs/canvas/include/canvas/EdgeItem.h`
- Modify: `libs/canvas/src/EdgeItem.cpp`

- [ ] **Step 1: Create ConnectableItem.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointF>
#include <QString>
#include "CanvasTypes.h"

class QGraphicsObject;

namespace Canvas {

class ConnectableItem {
public:
    virtual ~ConnectableItem() = default;
    virtual QPointF connectionPoint(Side side) const = 0;
    virtual QString nodeId() const = 0;
    virtual QGraphicsObject *asGraphicsObject() = 0;
};

} // namespace Canvas
```

- [ ] **Step 2: Update TextCardItem to implement ConnectableItem**

In `libs/canvas/include/canvas/TextCardItem.h`, add `#include "ConnectableItem.h"` and make `TextCardItem` inherit from `ConnectableItem`:

Change the class declaration from:
```cpp
class TextCardItem : public QGraphicsObject {
```
to:
```cpp
class TextCardItem : public QGraphicsObject, public ConnectableItem {
```

Add the `asGraphicsObject()` override in the public section:
```cpp
    QGraphicsObject *asGraphicsObject() override { return this; }
```

Mark `connectionPoint` and `nodeId` as `override`:
```cpp
    QPointF connectionPoint(Side side) const override;
    QString nodeId() const override;
```

- [ ] **Step 3: Update EdgeItem to use ConnectableItem**

In `libs/canvas/include/canvas/EdgeItem.h`:

Replace the forward declaration of `TextCardItem` with `ConnectableItem`:
```cpp
class ConnectableItem;
```

Change the constructor and accessor types:
```cpp
class EdgeItem : public QGraphicsPathItem {
public:
    EdgeItem(ConnectableItem *fromCard, ConnectableItem *toCard, const CanvasEdge &data, QGraphicsItem *parent = nullptr);

    void adjust();
    void setEdgeData(const CanvasEdge &data);
    CanvasEdge edgeData() const;
    QString edgeId() const;
    ConnectableItem *sourceCard() const;
    ConnectableItem *targetCard() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    void drawArrowHead(QPainterPath &path, const QPointF &tip, const QPointF &from) const;

    CanvasEdge m_data;
    ConnectableItem *m_source;
    ConnectableItem *m_target;
};
```

- [ ] **Step 4: Update EdgeItem.cpp**

In `libs/canvas/src/EdgeItem.cpp`:

Change the include from `TextCardItem.h` to `ConnectableItem.h`:
```cpp
#include "canvas/ConnectableItem.h"
```

Change the constructor parameter types:
```cpp
EdgeItem::EdgeItem(ConnectableItem *fromCard, ConnectableItem *toCard, const CanvasEdge &data, QGraphicsItem *parent)
```

Change the accessor return types:
```cpp
ConnectableItem *EdgeItem::sourceCard() const
{
    return m_source;
}

ConnectableItem *EdgeItem::targetCard() const
{
    return m_target;
}
```

- [ ] **Step 5: Update CanvasScene where EdgeItems are created**

In `libs/canvas/src/CanvasScene.cpp`, update `addEdgeItemToScene` signature and the `onEdgeAdded` to look up `ConnectableItem*` from both text cards and (later) file cards.

Change `addEdgeItemToScene` in the header (`CanvasScene.h`):
```cpp
EdgeItem *addEdgeItemToScene(ConnectableItem *from, ConnectableItem *to, const CanvasEdge &edge);
```

Update the implementation in `CanvasScene.cpp`. Also add a helper to find any connectable item by ID:

```cpp
ConnectableItem *CanvasScene::connectableItem(const QString &id) const
{
    if (auto *card = textCardItem(id))
        return card;
    if (auto *file = fileCardItem(id))
        return file;
    return nullptr;
}
```

Note: `fileCardItem()` doesn't exist yet — for now just check text cards. We'll add file card lookup in Task 8.

Temporarily (until Task 8), the helper is:
```cpp
ConnectableItem *CanvasScene::connectableItem(const QString &id) const
{
    if (auto *card = textCardItem(id))
        return card;
    return nullptr;
}
```

Add the declaration to `CanvasScene.h`:
```cpp
    ConnectableItem *connectableItem(const QString &id) const;
```

Update `onEdgeAdded`:
```cpp
void CanvasScene::onEdgeAdded(const QString &id)
{
    if (!m_document)
        return;
    if (m_edgeItems.contains(id))
        return;

    const CanvasEdge edge = m_document->edge(id);
    auto *from = connectableItem(edge.fromNode);
    auto *to = connectableItem(edge.toNode);
    if (from && to) {
        addEdgeItemToScene(from, to, edge);
    }
}
```

Update `populateFromDocument` similarly — change edge creation to use `connectableItem()`.

Also update the `positionChanged` connection in `addTextCardItem` — the edge's `adjust()` method still works because it calls `connectionPoint()` on `ConnectableItem*`.

- [ ] **Step 6: Build and run existing canvas tests**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -10`
Expected: Build succeeds

Run: `cd build && ctest -R tst_canvas --output-on-failure`
Expected: All canvas tests PASS (existing behavior unchanged)

- [ ] **Step 7: Commit**

```bash
git add libs/canvas/include/canvas/ConnectableItem.h libs/canvas/include/canvas/TextCardItem.h libs/canvas/src/TextCardItem.cpp libs/canvas/include/canvas/EdgeItem.h libs/canvas/src/EdgeItem.cpp libs/canvas/include/canvas/CanvasScene.h libs/canvas/src/CanvasScene.cpp
git commit -m "refactor: extract ConnectableItem interface for edge connections"
```

---

### Task 7: Link canvas library to Corbomite::Core

**Files:**
- Modify: `libs/canvas/CMakeLists.txt`

The canvas library needs to depend on `Corbomite::Core` for the render engine interface. Currently it only depends on `Qt6::Core Qt6::Widgets`.

- [ ] **Step 1: Add Corbomite::Core dependency**

In `libs/canvas/CMakeLists.txt`, change:
```cmake
target_link_libraries(canvas PUBLIC Qt6::Core Qt6::Widgets)
```
to:
```cmake
target_link_libraries(canvas PUBLIC Qt6::Core Qt6::Widgets Corbomite::Core)
```

- [ ] **Step 2: Build to verify**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 3: Commit**

```bash
git add libs/canvas/CMakeLists.txt
git commit -m "build: link canvas library to Corbomite::Core for render engine"
```

---

### Task 8: FileCardItem graphics item

**Files:**
- Create: `libs/canvas/include/canvas/FileCardItem.h`
- Create: `libs/canvas/src/FileCardItem.cpp`
- Modify: `libs/canvas/CMakeLists.txt`

- [ ] **Step 1: Create FileCardItem.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsObject>
#include <memory>
#include "CanvasTypes.h"
#include "ConnectableItem.h"

namespace Corbomite {
class RenderedDocument;
}

namespace Canvas {

class FileCardItem : public QGraphicsObject, public ConnectableItem {
    Q_OBJECT

public:
    FileCardItem(const CanvasNode &data, QGraphicsItem *parent = nullptr);

    void setNodeData(const CanvasNode &data);
    CanvasNode nodeData() const;
    QString nodeId() const override;

    void setRenderedDocument(std::unique_ptr<Corbomite::RenderedDocument> doc);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QPointF connectionPoint(Side side) const override;
    QGraphicsObject *asGraphicsObject() override { return this; }

    enum ResizeMode { NoResize = 0, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };
    ResizeMode resizeModeAtPos(const QPointF &localPos) const;

Q_SIGNALS:
    void positionChanged();
    void sizeChanged();
    void editRequested();
    void refreshRequested();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QString displayTitle() const;

    CanvasNode m_data;
    std::unique_ptr<Corbomite::RenderedDocument> m_renderedDoc;
};

} // namespace Canvas
```

- [ ] **Step 2: Create FileCardItem.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/FileCardItem.h"
#include "corbomite/core/RenderedDocument.h"

#include <QFileInfo>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTextDocument>

namespace Canvas {

static constexpr qreal kCornerRadius = 8.0;
static constexpr qreal kTitleBarHeight = 28.0;
static constexpr qreal kTextPadding = 8.0;
static constexpr qreal kHandleSize = 6.0;
static constexpr qreal kResizeZone = 8.0;

FileCardItem::FileCardItem(const CanvasNode &data, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_data(data)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(1);
    setPos(data.x, data.y);
}

void FileCardItem::setNodeData(const CanvasNode &data)
{
    prepareGeometryChange();
    m_data = data;
    setPos(data.x, data.y);
    update();
}

CanvasNode FileCardItem::nodeData() const
{
    return m_data;
}

QString FileCardItem::nodeId() const
{
    return m_data.id;
}

void FileCardItem::setRenderedDocument(std::unique_ptr<Corbomite::RenderedDocument> doc)
{
    m_renderedDoc = std::move(doc);
    update();
}

QString FileCardItem::displayTitle() const
{
    // Extract filename without extension
    QString title = QFileInfo(m_data.file).completeBaseName();
    if (title.isEmpty())
        title = m_data.file;

    // Append subpath if present
    if (!m_data.subpath.isEmpty()) {
        QString sub = m_data.subpath;
        if (sub.startsWith(QLatin1Char('#')))
            sub = sub.mid(1);
        if (sub.startsWith(QLatin1Char('^')))
            sub = sub.mid(1);
        title += QStringLiteral(" > ") + sub;
    }

    return title;
}

QRectF FileCardItem::boundingRect() const
{
    return QRectF(0, 0, m_data.width, m_data.height);
}

void FileCardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing);

    const QRectF rect = boundingRect();
    const bool selected = (option->state & QStyle::State_Selected);

    // 1. Rounded rect with white fill
    QPainterPath cardPath;
    cardPath.addRoundedRect(rect, kCornerRadius, kCornerRadius);
    painter->fillPath(cardPath, QColor(255, 255, 255));

    // 2. Title bar
    const QColor stripeColor = colorFromCanvasColor(m_data.color);
    const QColor titleBg = stripeColor.isValid() ? stripeColor : QColor(240, 240, 240);

    painter->save();
    painter->setClipPath(cardPath);
    painter->fillRect(QRectF(0, 0, rect.width(), kTitleBarHeight), titleBg);
    painter->restore();

    // File icon (small document icon indicator)
    QFont iconFont = painter->font();
    iconFont.setPointSize(9);
    painter->save();
    painter->setFont(iconFont);
    painter->setPen(stripeColor.isValid() ? QColor(255, 255, 255) : QColor(100, 100, 100));
    painter->drawText(QRectF(kTextPadding, 0, 16, kTitleBarHeight),
                      Qt::AlignCenter, QStringLiteral("\u{1F4C4}"));
    painter->restore();

    // Title text
    QFont titleFont = painter->font();
    titleFont.setPointSize(10);
    titleFont.setBold(true);
    painter->save();
    painter->setFont(titleFont);
    painter->setPen(stripeColor.isValid() ? QColor(255, 255, 255) : QColor(40, 40, 40));
    const qreal titleLeft = kTextPadding + 20; // After icon
    painter->drawText(QRectF(titleLeft, 0, rect.width() - titleLeft - kTextPadding, kTitleBarHeight),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      displayTitle());
    painter->restore();

    // 3. Render body content
    if (m_renderedDoc && m_renderedDoc->toQTextDocument()) {
        const qreal textTop = kTitleBarHeight + kTextPadding;
        const qreal textWidth = rect.width() - 2 * kTextPadding;
        const qreal availableHeight = rect.height() - textTop - kTextPadding;

        QTextDocument *doc = m_renderedDoc->toQTextDocument();
        doc->setTextWidth(textWidth);

        painter->save();
        painter->translate(kTextPadding, textTop);
        painter->setClipRect(QRectF(0, 0, textWidth, availableHeight));
        doc->drawContents(painter);
        painter->restore();
    } else {
        // No content — show placeholder
        painter->save();
        painter->setPen(QColor(160, 160, 160));
        QFont placeholderFont = painter->font();
        placeholderFont.setItalic(true);
        painter->setFont(placeholderFont);
        const qreal textTop = kTitleBarHeight + kTextPadding;
        painter->drawText(QRectF(kTextPadding, textTop, rect.width() - 2 * kTextPadding, 30),
                          Qt::AlignLeft | Qt::AlignTop,
                          QStringLiteral("File not found"));
        painter->restore();
    }

    // 4. Border
    QPen borderPen(selected ? QColor(58, 134, 255) : QColor(200, 200, 200));
    borderPen.setWidthF(selected ? 2.0 : 1.0);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(rect, kCornerRadius, kCornerRadius);

    // 5. Resize handles when selected
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(58, 134, 255));

        const qreal hs = kHandleSize;
        const qreal hh = hs / 2.0;
        const qreal w = rect.width();
        const qreal h = rect.height();

        painter->drawRect(QRectF(-hh, -hh, hs, hs));
        painter->drawRect(QRectF(w - hh, -hh, hs, hs));
        painter->drawRect(QRectF(w - hh, h - hh, hs, hs));
        painter->drawRect(QRectF(-hh, h - hh, hs, hs));
        painter->drawRect(QRectF(w / 2.0 - hh, -hh, hs, hs));
        painter->drawRect(QRectF(w - hh, h / 2.0 - hh, hs, hs));
        painter->drawRect(QRectF(w / 2.0 - hh, h - hh, hs, hs));
        painter->drawRect(QRectF(-hh, h / 2.0 - hh, hs, hs));
    }
}

QPointF FileCardItem::connectionPoint(Side side) const
{
    const QRectF rect = boundingRect();
    QPointF local;
    switch (side) {
    case Side::Top:    local = QPointF(rect.width() / 2.0, 0); break;
    case Side::Right:  local = QPointF(rect.width(), rect.height() / 2.0); break;
    case Side::Bottom: local = QPointF(rect.width() / 2.0, rect.height()); break;
    case Side::Left:   local = QPointF(0, rect.height() / 2.0); break;
    }
    return mapToScene(local);
}

FileCardItem::ResizeMode FileCardItem::resizeModeAtPos(const QPointF &localPos) const
{
    const QRectF rect = boundingRect();
    const qreal x = localPos.x();
    const qreal y = localPos.y();
    const qreal w = rect.width();
    const qreal h = rect.height();

    const bool nearLeft   = x < kResizeZone;
    const bool nearRight  = x > w - kResizeZone;
    const bool nearTop    = y < kResizeZone;
    const bool nearBottom = y > h - kResizeZone;

    if (nearTop && nearLeft)     return TopLeft;
    if (nearTop && nearRight)    return TopRight;
    if (nearBottom && nearRight) return BottomRight;
    if (nearBottom && nearLeft)  return BottomLeft;
    if (nearTop)    return Top;
    if (nearRight)  return Right;
    if (nearBottom) return Bottom;
    if (nearLeft)   return Left;

    return NoResize;
}

QVariant FileCardItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        Q_EMIT positionChanged();
    }
    return QGraphicsObject::itemChange(change, value);
}

void FileCardItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    Q_EMIT editRequested();
}

} // namespace Canvas
```

- [ ] **Step 3: Add to canvas CMakeLists.txt**

Add `src/FileCardItem.cpp` and `include/canvas/FileCardItem.h` to the source list:

```cmake
add_library(canvas STATIC
    src/CanvasDocument.cpp
    src/CanvasView.cpp
    src/CanvasScene.cpp
    src/CanvasTool.cpp
    src/CanvasCommands.cpp
    src/TextCardItem.cpp
    src/GroupItem.cpp
    src/EdgeItem.cpp
    src/FileCardItem.cpp
    include/canvas/CanvasDocument.h
    include/canvas/CanvasView.h
    include/canvas/CanvasScene.h
    include/canvas/CanvasTool.h
    include/canvas/CanvasCommands.h
    include/canvas/TextCardItem.h
    include/canvas/GroupItem.h
    include/canvas/EdgeItem.h
    include/canvas/FileCardItem.h
)
```

- [ ] **Step 4: Build**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

- [ ] **Step 5: Commit**

```bash
git add libs/canvas/include/canvas/FileCardItem.h libs/canvas/src/FileCardItem.cpp libs/canvas/CMakeLists.txt
git commit -m "feat: add FileCardItem graphics item for canvas file cards"
```

---

### Task 9: Integrate FileCardItem into CanvasScene

**Files:**
- Modify: `libs/canvas/include/canvas/CanvasScene.h`
- Modify: `libs/canvas/src/CanvasScene.cpp`
- Modify: `libs/canvas/tests/tst_canvasscene.cpp`

- [ ] **Step 1: Write the failing test**

Add to `libs/canvas/tests/tst_canvasscene.cpp`:

```cpp
    void testAddFileCard()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        // Set up a file resolver that returns markdown
        scene.setFileResolver([](const QString &path) -> QString {
            if (path == QStringLiteral("note.md"))
                return QStringLiteral("# Hello\n\nContent here");
            return {};
        });

        // Create a RegexRenderEngine for the scene
        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        scene.setRenderEngine(&engine);

        Canvas::CanvasNode node;
        node.id = QStringLiteral("file1");
        node.type = Canvas::NodeType::File;
        node.file = QStringLiteral("note.md");
        node.x = 100; node.y = 100;
        node.width = 250; node.height = 200;
        doc.addNode(node);

        auto *item = scene.fileCardItem(QStringLiteral("file1"));
        QVERIFY(item != nullptr);
        QCOMPARE(item->nodeId(), QStringLiteral("file1"));
    }

    void testFileCardWithSubpath()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setFileResolver([](const QString &) -> QString {
            return QStringLiteral("# Title\n\nIntro\n\n## Section\n\nSection content");
        });

        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        scene.setRenderEngine(&engine);

        Canvas::CanvasNode node;
        node.id = QStringLiteral("sub1");
        node.type = Canvas::NodeType::File;
        node.file = QStringLiteral("note.md");
        node.subpath = QStringLiteral("#Section");
        node.x = 0; node.y = 0;
        node.width = 250; node.height = 200;
        doc.addNode(node);

        auto *item = scene.fileCardItem(QStringLiteral("sub1"));
        QVERIFY(item != nullptr);
    }

    void testFileResolverReturnsEmpty()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setFileResolver([](const QString &) -> QString { return {}; });

        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        scene.setRenderEngine(&engine);

        Canvas::CanvasNode node;
        node.id = QStringLiteral("missing");
        node.type = Canvas::NodeType::File;
        node.file = QStringLiteral("nonexistent.md");
        node.x = 0; node.y = 0;
        node.width = 250; node.height = 200;
        doc.addNode(node);

        // Card should still be created (will show placeholder)
        auto *item = scene.fileCardItem(QStringLiteral("missing"));
        QVERIFY(item != nullptr);
    }

    void testEdgeBetweenTextAndFileCard()
    {
        Canvas::CanvasDocument doc;
        Canvas::CanvasScene scene;
        scene.setDocument(&doc);

        scene.setFileResolver([](const QString &) -> QString {
            return QStringLiteral("# Note\n\nContent");
        });
        Corbomite::RegexRenderEngine engine;
        engine.setProfile(Corbomite::RenderProfile::canvasCard());
        scene.setRenderEngine(&engine);

        // Add a text card
        Canvas::CanvasNode textNode;
        textNode.id = QStringLiteral("t1");
        textNode.type = Canvas::NodeType::Text;
        textNode.x = 0; textNode.y = 0; textNode.width = 200; textNode.height = 80;
        doc.addNode(textNode);

        // Add a file card
        Canvas::CanvasNode fileNode;
        fileNode.id = QStringLiteral("f1");
        fileNode.type = Canvas::NodeType::File;
        fileNode.file = QStringLiteral("note.md");
        fileNode.x = 400; fileNode.y = 0; fileNode.width = 250; fileNode.height = 200;
        doc.addNode(fileNode);

        // Add edge between them
        Canvas::CanvasEdge edge;
        edge.id = QStringLiteral("e1");
        edge.fromNode = QStringLiteral("t1");
        edge.toNode = QStringLiteral("f1");
        doc.addEdge(edge);

        auto *edgeItem = scene.edgeItem(QStringLiteral("e1"));
        QVERIFY(edgeItem != nullptr);
        QVERIFY(!edgeItem->path().isEmpty());
    }
```

Add the necessary includes at the top of the test file:
```cpp
#include "canvas/FileCardItem.h"
#include "corbomite/core/RegexRenderEngine.h"
```

- [ ] **Step 2: Update CanvasScene.h**

Add file card members, render engine injection, and file resolver:

```cpp
// Add includes at top:
#include <functional>
namespace Corbomite {
class MarkdownRenderEngine;
}

// In the CanvasScene class, add to public section:
    // Render engine for file/text card rendering
    void setRenderEngine(Corbomite::MarkdownRenderEngine *engine);
    Corbomite::MarkdownRenderEngine *renderEngine() const;

    // File content resolver
    using FileResolver = std::function<QString(const QString &filePath)>;
    void setFileResolver(FileResolver resolver);

    // File card item management
    FileCardItem *addFileCardItem(const CanvasNode &node);
    void removeFileCardItem(const QString &id);
    FileCardItem *fileCardItem(const QString &id) const;

    // Lookup any connectable item by node ID
    ConnectableItem *connectableItem(const QString &id) const;

// In private section, add:
    QHash<QString, FileCardItem *> m_fileCardItems;
    Corbomite::MarkdownRenderEngine *m_renderEngine = nullptr;
    FileResolver m_fileResolver;
```

Add forward declaration for `FileCardItem` and `ConnectableItem` next to the existing forward declarations.

- [ ] **Step 3: Update CanvasScene.cpp**

Add the includes:
```cpp
#include "canvas/FileCardItem.h"
#include "canvas/ConnectableItem.h"
#include "corbomite/core/MarkdownRenderEngine.h"
#include "corbomite/core/RenderOptions.h"
```

Add the new methods:
```cpp
void CanvasScene::setRenderEngine(Corbomite::MarkdownRenderEngine *engine)
{
    m_renderEngine = engine;
}

Corbomite::MarkdownRenderEngine *CanvasScene::renderEngine() const
{
    return m_renderEngine;
}

void CanvasScene::setFileResolver(FileResolver resolver)
{
    m_fileResolver = std::move(resolver);
}

FileCardItem *CanvasScene::addFileCardItem(const CanvasNode &node)
{
    auto *item = new FileCardItem(node);
    addItem(item);
    m_fileCardItems.insert(node.id, item);

    // Connect editRequested signal
    connect(item, &FileCardItem::editRequested, this, [this, item]() {
        Q_EMIT cardDoubleClicked(item->nodeId());
    });

    // Forward position changes to edge adjustment
    connect(item, &FileCardItem::positionChanged, this, [this, item]() {
        if (!m_document)
            return;
        const auto edges = m_document->edgesForNode(item->nodeId());
        for (const auto &edge : edges) {
            if (auto *edgeItem = this->edgeItem(edge.id)) {
                edgeItem->adjust();
            }
        }
    });

    // Render the file content
    renderFileCard(item);

    return item;
}

void CanvasScene::removeFileCardItem(const QString &id)
{
    if (auto *item = m_fileCardItems.take(id)) {
        removeItem(item);
        delete item;
    }
}

FileCardItem *CanvasScene::fileCardItem(const QString &id) const
{
    return m_fileCardItems.value(id, nullptr);
}

ConnectableItem *CanvasScene::connectableItem(const QString &id) const
{
    if (auto *card = textCardItem(id))
        return card;
    if (auto *file = fileCardItem(id))
        return file;
    return nullptr;
}
```

Add a private helper `renderFileCard`:

In the header, add to private section:
```cpp
    void renderFileCard(FileCardItem *item);
```

Implementation:
```cpp
void CanvasScene::renderFileCard(FileCardItem *item)
{
    if (!item || !m_renderEngine)
        return;

    QString markdown;
    if (m_fileResolver) {
        markdown = m_fileResolver(item->nodeData().file);
    }

    if (markdown.isEmpty()) {
        item->setRenderedDocument(nullptr);
        return;
    }

    Corbomite::RenderOptions opts;
    opts.subpath = item->nodeData().subpath;

    auto rendered = m_renderEngine->render(markdown, opts);
    item->setRenderedDocument(std::move(rendered));
}
```

Update `onNodeAdded` to handle `NodeType::File`:
```cpp
void CanvasScene::onNodeAdded(const QString &id)
{
    if (!m_document)
        return;

    if (m_textCardItems.contains(id) || m_groupItems.contains(id) || m_fileCardItems.contains(id))
        return;

    const CanvasNode node = m_document->node(id);
    switch (node.type) {
    case NodeType::Text:
        addTextCardItem(node);
        break;
    case NodeType::File:
        addFileCardItem(node);
        break;
    case NodeType::Group:
        addGroupItemToScene(node);
        break;
    case NodeType::Link:
        break;
    }
}
```

Update `onNodeRemoved` to also remove file cards:
```cpp
void CanvasScene::onNodeRemoved(const QString &id)
{
    removeTextCardItem(id);
    removeFileCardItem(id);
    removeGroupItem(id);
}
```

Update `onNodeChanged` to handle file cards:
```cpp
void CanvasScene::onNodeChanged(const QString &id)
{
    if (!m_document)
        return;

    const CanvasNode node = m_document->node(id);
    if (auto *card = textCardItem(id)) {
        card->setNodeData(node);
    } else if (auto *file = fileCardItem(id)) {
        file->setNodeData(node);
        renderFileCard(file);
    } else if (auto *group = groupItem(id)) {
        group->setNodeData(node);
    }
}
```

Update `populateFromDocument` to handle file nodes:
```cpp
void CanvasScene::populateFromDocument()
{
    if (!m_document)
        return;

    const auto nodes = m_document->nodes();
    for (const auto &node : nodes) {
        if (node.type == NodeType::Group) {
            addGroupItemToScene(node);
        } else if (node.type == NodeType::Text) {
            addTextCardItem(node);
        } else if (node.type == NodeType::File) {
            addFileCardItem(node);
        }
    }

    const auto edges = m_document->edges();
    for (const auto &edge : edges) {
        auto *from = connectableItem(edge.fromNode);
        auto *to = connectableItem(edge.toNode);
        if (from && to) {
            addEdgeItemToScene(from, to, edge);
        }
    }
}
```

Update `clearAllItems` to clear file cards:
```cpp
void CanvasScene::clearAllItems()
{
    finishInlineEdit();
    finishGroupLabelEdit();

    m_textCardItems.clear();
    m_fileCardItems.clear();
    m_groupItems.clear();
    m_edgeItems.clear();
    clear();
}
```

Update the context menu to handle right-click on file cards — add `FileCardItem` detection in `contextMenuEvent`:
```cpp
    FileCardItem *fileItem = nullptr;
    // In the while(hitItem) loop, add:
    if (!fileItem) fileItem = dynamic_cast<FileCardItem *>(hitItem);
    // Update break condition:
    if (cardItem || grpItem || edgItem || fileItem)
        break;
```

Add a context menu section for file cards (after the `cardItem` block, before `grpItem`):
```cpp
    } else if (fileItem) {
        // Color submenu (same as text cards)
        auto *colorMenu = menu.addMenu(QStringLiteral("Color"));
        const struct { QString name; QString code; } colors[] = {
            { QStringLiteral("Red"),    QStringLiteral("1") },
            { QStringLiteral("Orange"), QStringLiteral("2") },
            { QStringLiteral("Yellow"), QStringLiteral("3") },
            { QStringLiteral("Green"),  QStringLiteral("4") },
            { QStringLiteral("Cyan"),   QStringLiteral("5") },
            { QStringLiteral("Purple"), QStringLiteral("6") },
        };
        for (const auto &c : colors) {
            colorMenu->addAction(c.name, [this, fileItem, code = c.code]() {
                if (!m_document) return;
                const QString oldColor = fileItem->nodeData().color;
                m_undoStack->push(
                    new CmdChangeColor(m_document, fileItem->nodeId(), oldColor, code));
            });
        }
        colorMenu->addSeparator();
        colorMenu->addAction(QStringLiteral("Remove Color"), [this, fileItem]() {
            if (!m_document) return;
            const QString oldColor = fileItem->nodeData().color;
            m_undoStack->push(
                new CmdChangeColor(m_document, fileItem->nodeId(), oldColor, QString()));
        });

        menu.addSeparator();
        menu.addAction(QStringLiteral("Delete"), [this, fileItem]() {
            if (!m_document) return;
            m_undoStack->push(
                new CmdRemoveCard(m_document, fileItem->nodeId()));
        });
```

- [ ] **Step 4: Update canvas test CMakeLists.txt to link Corbomite::Core**

In `libs/canvas/tests/CMakeLists.txt`, update the `tst_canvasscene` link line:
```cmake
target_link_libraries(tst_canvasscene PRIVATE Qt6::Test Qt6::Widgets canvas Corbomite::Core)
```

- [ ] **Step 5: Build and run tests**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -10`
Expected: Build succeeds

Run: `cd build && ctest -R tst_canvasscene --output-on-failure`
Expected: All tests PASS (old and new)

- [ ] **Step 6: Run all tests**

Run: `cd build && ctest --output-on-failure`
Expected: All tests PASS

- [ ] **Step 7: Commit**

```bash
git add libs/canvas/include/canvas/CanvasScene.h libs/canvas/src/CanvasScene.cpp libs/canvas/tests/tst_canvasscene.cpp libs/canvas/tests/CMakeLists.txt
git commit -m "feat: integrate FileCardItem into CanvasScene with render engine"
```

---

### Task 10: Wire render engine into CanvasViewTab

**Files:**
- Modify: `src/canvas/CanvasViewTab.h`
- Modify: `src/canvas/CanvasViewTab.cpp`

- [ ] **Step 1: Update CanvasViewTab.h**

Add render engine and file resolver wiring:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Canvas {
class CanvasDocument;
class CanvasView;
}

namespace Corbomite {

class MarkdownRenderEngine;

class CanvasViewTab : public QWidget {
    Q_OBJECT

public:
    explicit CanvasViewTab(const QString &filePath, QWidget *parent = nullptr);
    ~CanvasViewTab() override;

    void setRenderEngine(MarkdownRenderEngine *engine);

    QString filePath() const;
    bool save();
    bool isModified() const;

Q_SIGNALS:
    void modificationChanged(bool modified);

private:
    Canvas::CanvasDocument *m_document;
    Canvas::CanvasView *m_view;
    QString m_filePath;
};

} // namespace Corbomite
```

- [ ] **Step 2: Update CanvasViewTab.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasViewTab.h"

#include <canvas/CanvasDocument.h>
#include <canvas/CanvasScene.h>
#include <canvas/CanvasView.h>
#include <corbomite/core/MarkdownRenderEngine.h>

#include <QFileInfo>
#include <QVBoxLayout>

namespace Corbomite {

CanvasViewTab::CanvasViewTab(const QString &filePath, QWidget *parent)
    : QWidget(parent)
    , m_filePath(filePath)
{
    m_document = new Canvas::CanvasDocument(this);
    m_view = new Canvas::CanvasView(this);

    m_document->loadFromFile(filePath);
    m_view->setDocument(m_document);

    // Set up file resolver: resolve paths relative to the canvas file's directory
    QString canvasDir = QFileInfo(filePath).absolutePath();
    m_view->scene()->setFileResolver([canvasDir](const QString &path) -> QString {
        QString fullPath = canvasDir + QLatin1Char('/') + path;
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(file.readAll());
    });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    connect(m_document, &Canvas::CanvasDocument::modificationChanged,
            this, &CanvasViewTab::modificationChanged);
}

CanvasViewTab::~CanvasViewTab() = default;

void CanvasViewTab::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_view->scene()->setRenderEngine(engine);
}

QString CanvasViewTab::filePath() const
{
    return m_filePath;
}

bool CanvasViewTab::save()
{
    return m_document->saveToFile(m_filePath);
}

bool CanvasViewTab::isModified() const
{
    return m_document->isModified();
}

} // namespace Corbomite
```

Note: This requires `CanvasView` to expose its scene. Check if `CanvasView::scene()` exists — it inherits `QGraphicsView::scene()` which returns `QGraphicsScene*`. We need the typed `CanvasScene*`. Add a `canvasScene()` accessor to `CanvasView` if needed:

In `libs/canvas/include/canvas/CanvasView.h`, add:
```cpp
    CanvasScene *canvasScene() const { return m_scene; }
```

Then in `CanvasViewTab.cpp`, use `m_view->canvasScene()` instead of `m_view->scene()`.

- [ ] **Step 3: Wire the render engine from MainWindow**

Find where `CanvasViewTab` instances are created in the app (likely `EditorViewSpace` or `MainWindow`). Add `setRenderEngine()` calls after construction, passing the app's shared `RegexRenderEngine` instance.

The render engine should be created once in `MainWindow` (or wherever the vault is managed) with `CanvasCard` profile and shared across all canvas tabs.

Read the relevant files to find the exact construction site and wire it up.

- [ ] **Step 4: Build and test manually**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

Run: `./build/Corbomite` — open a vault that has `.canvas` files with file nodes
Expected: File cards appear with rendered content. Edges connect to them.

- [ ] **Step 5: Run all tests**

Run: `cd build && ctest --output-on-failure`
Expected: All tests PASS

- [ ] **Step 6: Commit**

```bash
git add src/canvas/CanvasViewTab.h src/canvas/CanvasViewTab.cpp libs/canvas/include/canvas/CanvasView.h
git commit -m "feat: wire render engine into CanvasViewTab for file card rendering"
```

Note: Include any other files modified during wiring (MainWindow, EditorViewSpace, etc.).

---

### Task 11: Inline editing for file cards

**Files:**
- Modify: `libs/canvas/include/canvas/CanvasScene.h`
- Modify: `libs/canvas/src/CanvasScene.cpp`

- [ ] **Step 1: Add FileSaver callback to CanvasScene.h**

In the public section:
```cpp
    using FileSaver = std::function<void(const QString &filePath, const QString &content)>;
    void setFileSaver(FileSaver saver);
```

In the private section:
```cpp
    FileSaver m_fileSaver;
    void beginFileCardEdit(FileCardItem *card);
    void finishFileCardEdit();
    QString m_editingFileCardId;
```

- [ ] **Step 2: Implement in CanvasScene.cpp**

```cpp
void CanvasScene::setFileSaver(FileSaver saver)
{
    m_fileSaver = std::move(saver);
}

void CanvasScene::beginFileCardEdit(FileCardItem *card)
{
    if (!card)
        return;

    finishInlineEdit();
    finishFileCardEdit();

    m_editingFileCardId = card->nodeId();

    // Load file content
    QString content;
    if (m_fileResolver) {
        content = m_fileResolver(card->nodeData().file);
    }

    m_editWidget = new QTextEdit;
    m_editWidget->setPlainText(content);
    m_editWidget->setFixedSize(static_cast<int>(card->boundingRect().width()),
                               static_cast<int>(card->boundingRect().height()));
    m_editWidget->setFrameShape(QFrame::NoFrame);

    m_editProxy = addWidget(m_editWidget);
    m_editProxy->setPos(card->pos());
    m_editProxy->setZValue(100);

    m_editWidget->setFocus();

    connect(m_editWidget, &QTextEdit::destroyed, this, [this]() {
        m_editProxy = nullptr;
        m_editWidget = nullptr;
        m_editingFileCardId.clear();
    });

    disconnect(m_focusConnection);
    m_focusConnection = connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (m_editWidget && now != m_editWidget) {
            finishFileCardEdit();
        }
    });
}

void CanvasScene::finishFileCardEdit()
{
    if (!m_editProxy || !m_editWidget || m_editingFileCardId.isEmpty())
        return;

    const QString newContent = m_editWidget->toPlainText();
    const QString nodeId = m_editingFileCardId;

    removeItem(m_editProxy);
    delete m_editProxy;
    m_editProxy = nullptr;
    m_editWidget = nullptr;
    m_editingFileCardId.clear();
    disconnect(m_focusConnection);

    // Save to file via callback
    if (auto *card = fileCardItem(nodeId)) {
        if (m_fileSaver) {
            m_fileSaver(card->nodeData().file, newContent);
        }
        // Re-render the card
        renderFileCard(card);
    }
}
```

Update `addFileCardItem` to connect the edit signal to `beginFileCardEdit`:
```cpp
    connect(item, &FileCardItem::editRequested, this, [this, item]() {
        beginFileCardEdit(item);
    });
```

(Replace the existing `cardDoubleClicked` signal emission.)

- [ ] **Step 3: Wire FileSaver in CanvasViewTab.cpp**

Add after the file resolver setup in the constructor:
```cpp
    m_view->canvasScene()->setFileSaver([canvasDir](const QString &path, const QString &content) {
        QString fullPath = canvasDir + QLatin1Char('/') + path;
        QFile file(fullPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(content.toUtf8());
        }
    });
```

- [ ] **Step 4: Build and test**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

Run: `cd build && ctest --output-on-failure`
Expected: All tests PASS

Manual test: Open a canvas with file cards, double-click one, edit, click away. Verify the file is saved.

- [ ] **Step 5: Commit**

```bash
git add libs/canvas/include/canvas/CanvasScene.h libs/canvas/src/CanvasScene.cpp src/canvas/CanvasViewTab.cpp
git commit -m "feat: add inline editing for canvas file cards"
```

---

### Task 12: Migrate TextCardItem rendering to use render engine

**Files:**
- Modify: `libs/canvas/src/CanvasScene.cpp`
- Modify: `libs/canvas/include/canvas/TextCardItem.h`
- Modify: `libs/canvas/src/TextCardItem.cpp`

This unifies text and file card rendering through the same engine. TextCardItem will receive a `RenderedDocument` for its body content, just like FileCardItem.

- [ ] **Step 1: Add setRenderedDocument to TextCardItem**

In `libs/canvas/include/canvas/TextCardItem.h`, add:
```cpp
#include <memory>
namespace Corbomite { class RenderedDocument; }
```

Add in the public section:
```cpp
    void setRenderedDocument(std::unique_ptr<Corbomite::RenderedDocument> doc);
```

Add in private:
```cpp
    std::unique_ptr<Corbomite::RenderedDocument> m_renderedDoc;
```

- [ ] **Step 2: Update TextCardItem::paint() to use RenderedDocument for body**

In `TextCardItem.cpp`, replace the body text rendering section (lines 167-215, the section after "4. Render body text via QTextDocument") with code that uses `m_renderedDoc` if available, falling back to the inline regex approach:

```cpp
    // 4. Render body text
    if (!cardText.isEmpty()) {
        const qreal textTop = headerBarHeight > 0 ? headerBarHeight + kTextPadding : kTextPadding;
        const qreal textWidth = rect.width() - 2 * kTextPadding;
        const qreal availableHeight = rect.height() - textTop - kTextPadding;

        QTextDocument *doc = nullptr;
        QTextDocument localDoc;

        if (m_renderedDoc && m_renderedDoc->toQTextDocument()) {
            doc = m_renderedDoc->toQTextDocument();
        } else {
            // Fallback: inline regex conversion (legacy path)
            QString html = cardText;
            html.replace(QRegularExpression(QStringLiteral(R"(\*\*(.+?)\*\*)")),
                          QStringLiteral("<b>\\1</b>"));
            html.replace(QRegularExpression(QStringLiteral(R"((?<!\*)\*([^*]+?)\*(?!\*))")),
                          QStringLiteral("<i>\\1</i>"));
            html.replace(QRegularExpression(QStringLiteral(R"(\[\[([^\]|]+)\|([^\]]+)\]\])")),
                          QStringLiteral("<a style='color:#7b6cd9;text-decoration:underline'>\\2</a>"));
            html.replace(QRegularExpression(QStringLiteral(R"(\[\[([^\]]+)\]\])")),
                          QStringLiteral("<a style='color:#7b6cd9;text-decoration:underline'>\\1</a>"));
            html.replace(QRegularExpression(QStringLiteral(R"(\[([^\]]+)\]\(([^)]+)\))")),
                          QStringLiteral("<a style='color:#7b6cd9;text-decoration:underline'>\\1</a>"));
            html.replace(QRegularExpression(QStringLiteral(R"(`([^`]+)`)")),
                          QStringLiteral("<code style='background:#f0f0f0;padding:1px 3px'>\\1</code>"));
            html.replace(QRegularExpression(QStringLiteral(R"(^- (.+)$)"), QRegularExpression::MultilineOption),
                          QStringLiteral("&bull; \\1"));
            html.replace(QStringLiteral("\n"), QStringLiteral("<br>"));
            localDoc.setHtml(html);
            doc = &localDoc;
        }

        doc->setTextWidth(textWidth);
        doc->setDocumentMargin(0);

        painter->save();
        painter->translate(kTextPadding, textTop);
        painter->setClipRect(QRectF(0, 0, textWidth, availableHeight));
        doc->drawContents(painter);
        painter->restore();
    }
```

Add `#include "corbomite/core/RenderedDocument.h"` at the top.

- [ ] **Step 3: Render text cards through the engine in CanvasScene**

In `CanvasScene::addTextCardItem()`, after creating the item, render its content if engine is available:

```cpp
    // Render text card content via engine if available
    if (m_renderEngine && !node.text.isEmpty()) {
        auto rendered = m_renderEngine->render(node.text);
        item->setRenderedDocument(std::move(rendered));
    }
```

Also update `onNodeChanged` for text cards:
```cpp
    if (auto *card = textCardItem(id)) {
        card->setNodeData(node);
        if (m_renderEngine && !node.text.isEmpty()) {
            auto rendered = m_renderEngine->render(node.text);
            card->setRenderedDocument(std::move(rendered));
        }
    }
```

And after `finishInlineEdit` commits text, re-render:
```cpp
    if (auto *card = textCardItem(nodeId)) {
        const QString oldText = card->nodeData().text;
        if (m_document && oldText != newText) {
            m_undoStack->push(new CmdEditText(m_document, nodeId, oldText, newText));
            // Re-render if engine is available
            if (m_renderEngine) {
                auto rendered = m_renderEngine->render(newText);
                card->setRenderedDocument(std::move(rendered));
            }
        }
    }
```

- [ ] **Step 4: Build and run tests**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -5`
Expected: Build succeeds

Run: `cd build && ctest --output-on-failure`
Expected: All tests PASS

Manual test: Canvas text cards should still look the same (or better, with full markdown rendering).

- [ ] **Step 5: Commit**

```bash
git add libs/canvas/include/canvas/TextCardItem.h libs/canvas/src/TextCardItem.cpp libs/canvas/src/CanvasScene.cpp
git commit -m "feat: migrate TextCardItem body rendering to use render engine"
```
