# Obsidian Syntax Highlighting — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Obsidian-flavored markdown syntax highlighting (wikilinks, tags, callouts, ==highlight==, %%comments%%, ^block-refs) to the forked qmarkdowntextedit MarkdownHighlighter.

**Architecture:** Extend the existing `MarkdownHighlighter` with new `HighlighterState` enum values, format configurations, and highlighting methods. All changes in two files within the `libs/qmarkdowntextedit/` submodule. Tests in the main project.

**Tech Stack:** C++20, Qt6, QSyntaxHighlighter, QRegularExpression

**Spec:** `docs/superpowers/specs/2026-03-31-obsidian-syntax-highlighting-design.md`

**Key files to modify:**
- `libs/qmarkdowntextedit/markdownhighlighter.h` — enum values, method declarations
- `libs/qmarkdowntextedit/markdownhighlighter.cpp` — formats, highlighting methods, dispatch

**Key files to create:**
- `tests/editor/tst_obsidian_highlighting.cpp` — unit tests
- `tests/editor/CMakeLists.txt` — test build

**Important context for implementers:**
- The highlighter is in a **git submodule** (`libs/qmarkdowntextedit/`). Changes must be committed BOTH in the submodule AND the parent project (submodule pointer update).
- The submodule's origin is `git@github.com:clintonthegeek/qmarkdowntextedit.git` (our fork).
- Existing enum values: `WikiLink = 32`, `WikiLinkBroken = 33`, `LinkInternal = 34`. New values start at 35.
- The `_formats` hash is static and keyed by `HighlighterState`.
- `setFormat(startIndex, length, format)` applies highlighting to the current block.
- `isPosInACodeSpan(blockNumber, position)` checks if a position is inside inline code.
- `MaskedSyntax` is the existing format for dimmed syntax characters (brackets, `**`, etc.).

---

### Task 1: Add Enum Values and Format Configuration

**Files:**
- Modify: `libs/qmarkdowntextedit/markdownhighlighter.h`
- Modify: `libs/qmarkdowntextedit/markdownhighlighter.cpp`

- [ ] **Step 1: Add new HighlighterState enum values**

In `libs/qmarkdowntextedit/markdownhighlighter.h`, after `LinkInternal,` (line 149), add:

```cpp
        LinkInternal,

        // Obsidian-flavored markdown extensions
        ObsidianTag = 35,
        ObsidianCallout,
        ObsidianHighlight,
        ObsidianComment,
        ObsidianBlockRef,
        ObsidianEmbed,
```

- [ ] **Step 2: Add format configurations in initTextFormats()**

In `libs/qmarkdowntextedit/markdownhighlighter.cpp`, at the end of `initTextFormats()` (before the `Formats for syntax highlighting` comment block around line 300), add:

```cpp
    /****************************************
     * Formats for Obsidian-flavored markdown
     ***************************************/

    // Wikilinks: [[Note Name]]
    format = QTextCharFormat();
    format.setForeground(QColor(123, 108, 217));  // Purple/accent
    format.setFontUnderline(true);
    _formats[WikiLink] = std::move(format);

    // Broken wikilinks
    format = QTextCharFormat();
    format.setForeground(QColor(136, 136, 136));  // Gray
    format.setFontUnderline(true);
    _formats[WikiLinkBroken] = std::move(format);

    // Embeds: ![[image.png]]
    format = QTextCharFormat();
    format.setForeground(QColor(123, 108, 217));  // Purple/accent
    format.setFontUnderline(true);
    _formats[ObsidianEmbed] = std::move(format);

    // Tags: #project/active
    format = QTextCharFormat();
    format.setForeground(QColor(224, 108, 117));  // Red/rose
    _formats[ObsidianTag] = std::move(format);

    // Callout type: > [!warning]
    format = QTextCharFormat();
    format.setForeground(QColor(209, 154, 102));  // Orange
    format.setFontWeight(QFont::Bold);
    _formats[ObsidianCallout] = std::move(format);

    // Highlight: ==text==
    format = QTextCharFormat();
    format.setBackground(QColor(255, 243, 176));  // Yellow
    _formats[ObsidianHighlight] = std::move(format);

    // Comments: %%hidden%%
    format = QTextCharFormat();
    format.setForeground(QColor(92, 99, 112));    // Gray
    format.setFontItalic(true);
    _formats[ObsidianComment] = std::move(format);

    // Block references: ^block-id
    format = QTextCharFormat();
    format.setForeground(QColor(92, 99, 112));    // Gray
    _formats[ObsidianBlockRef] = std::move(format);
```

- [ ] **Step 3: Declare new highlighting methods in the header**

In `libs/qmarkdowntextedit/markdownhighlighter.h`, in the `protected:` section (around line 267, near other `highlight*` method declarations), add:

```cpp
    // Obsidian-flavored markdown highlighting
    int highlightObsidianWikiLink(const QString &text, int startPos);
    void highlightObsidianTags(const QString &text);
    void highlightObsidianCallouts(const QString &text);
    int highlightObsidianHighlight(const QString &text, int startPos);
    void highlightObsidianComments(const QString &text);
    void highlightObsidianBlockRef(const QString &text);
```

- [ ] **Step 4: Build to verify enum/format changes compile**

Run:
```bash
cmake --build build 2>&1 | tail -3
```

Expected: Builds cleanly (methods declared but not yet defined — that's OK since they're not called yet).

Wait — the methods are declared but not defined. This will cause a linker error if anything references them. Since nothing calls them yet, the build should succeed. But to be safe, let's also add empty stubs.

Add to `libs/qmarkdowntextedit/markdownhighlighter.cpp` at the end (before the closing of the file, before `#include "moc_markdownhighlighter.cpp"` if present):

```cpp
// Obsidian-flavored markdown highlighting — stubs (implemented in Task 2-5)

int MarkdownHighlighter::highlightObsidianWikiLink(const QString &text, int startPos) {
    Q_UNUSED(text) Q_UNUSED(startPos)
    return startPos;
}

void MarkdownHighlighter::highlightObsidianTags(const QString &text) {
    Q_UNUSED(text)
}

void MarkdownHighlighter::highlightObsidianCallouts(const QString &text) {
    Q_UNUSED(text)
}

int MarkdownHighlighter::highlightObsidianHighlight(const QString &text, int startPos) {
    Q_UNUSED(text) Q_UNUSED(startPos)
    return startPos;
}

void MarkdownHighlighter::highlightObsidianComments(const QString &text) {
    Q_UNUSED(text)
}

void MarkdownHighlighter::highlightObsidianBlockRef(const QString &text) {
    Q_UNUSED(text)
}
```

- [ ] **Step 5: Build and verify**

Run: `cmake --build build 2>&1 | tail -3`

Expected: Builds cleanly.

- [ ] **Step 6: Commit in submodule AND parent**

```bash
cd libs/qmarkdowntextedit
git add markdownhighlighter.h markdownhighlighter.cpp
git commit -m "feat: add Obsidian syntax highlighting enum values, formats, and method stubs

New HighlighterState values: ObsidianTag, ObsidianCallout,
ObsidianHighlight, ObsidianComment, ObsidianBlockRef, ObsidianEmbed.
Format colors configured. Method stubs for incremental implementation."

cd ../..
git add libs/qmarkdowntextedit
git commit -m "chore: update qmarkdowntextedit submodule with Obsidian highlighting stubs"
```

---

### Task 2: Wikilink and Embed Highlighting

**Files:**
- Modify: `libs/qmarkdowntextedit/markdownhighlighter.cpp`

- [ ] **Step 1: Implement highlightObsidianWikiLink()**

Replace the stub `highlightObsidianWikiLink` in `libs/qmarkdowntextedit/markdownhighlighter.cpp` with:

```cpp
int MarkdownHighlighter::highlightObsidianWikiLink(const QString &text, int startPos) {
    // Detect [[ or ![[ at startPos
    bool isEmbed = false;
    int bracketStart = startPos;

    if (text.at(startPos) == QLatin1Char('!') && startPos + 2 < text.length()
        && text.at(startPos + 1) == QLatin1Char('[') && text.at(startPos + 2) == QLatin1Char('[')) {
        isEmbed = true;
        bracketStart = startPos;
    } else if (text.at(startPos) == QLatin1Char('[') && startPos + 1 < text.length()
               && text.at(startPos + 1) == QLatin1Char('[')) {
        bracketStart = startPos;
    } else {
        return startPos;
    }

    int contentStart = isEmbed ? bracketStart + 3 : bracketStart + 2;

    // Find closing ]]
    int closingPos = text.indexOf(QStringLiteral("]]"), contentStart);
    if (closingPos == -1) return startPos;

    // Check if inside code span
    if (isPosInACodeSpan(currentBlock().blockNumber(), bracketStart)) {
        return startPos;
    }

    int closingEnd = closingPos + 2;
    const auto linkFormat = isEmbed ? _formats[ObsidianEmbed] : _formats[WikiLink];
    const auto maskedFormat = _formats[MaskedSyntax];

    // Check for alias: [[target|display]]
    int pipePos = text.indexOf(QLatin1Char('|'), contentStart);
    if (pipePos != -1 && pipePos < closingPos) {
        // Format: !?[[target|display]]
        // Opening brackets
        setFormat(bracketStart, contentStart - bracketStart, maskedFormat);
        // Target path
        setFormat(contentStart, pipePos - contentStart, linkFormat);
        // Pipe separator
        setFormat(pipePos, 1, maskedFormat);
        // Display text
        setFormat(pipePos + 1, closingPos - pipePos - 1, linkFormat);
        // Closing brackets
        setFormat(closingPos, 2, maskedFormat);
    } else {
        // Format: !?[[target]]
        // Opening brackets
        setFormat(bracketStart, contentStart - bracketStart, maskedFormat);
        // Content
        setFormat(contentStart, closingPos - contentStart, linkFormat);
        // Closing brackets
        setFormat(closingPos, 2, maskedFormat);
    }

    // Store range for future interactive features (Batch B: Ctrl+Click, hover preview)
    _ranges[currentBlock().blockNumber()].push_back(
        InlineRange(bracketStart, closingEnd, RangeType::Link));

    return closingEnd - 1; // -1 because the for loop will increment
}
```

- [ ] **Step 2: Wire wikilinks into highlightInlineRules()**

In `highlightInlineRules()`, replace the else branch (the `highlightLinkOrImage` call) with:

```cpp
        } else if (currentChar == QLatin1Char('!') && i + 2 < text.length()
                   && text.at(i + 1) == QLatin1Char('[') && text.at(i + 2) == QLatin1Char('[')) {
            // Obsidian embed: ![[...]]
            i = highlightObsidianWikiLink(text, i);
        } else if (currentChar == QLatin1Char('[') && i + 1 < text.length()
                   && text.at(i + 1) == QLatin1Char('[')) {
            // Obsidian wikilink: [[...]]
            i = highlightObsidianWikiLink(text, i);
        } else {
            i = highlightLinkOrImage(text, i);
        }
```

The full `highlightInlineRules` for-loop body should now be:

```cpp
    for (int i = 0; i < text.length(); ++i) {
        QChar currentChar = text.at(i);

        if (currentChar == QLatin1Char('`') ||
            currentChar == QLatin1Char('~')) {
            i = highlightInlineSpans(text, i, currentChar);
        } else if (currentChar == QLatin1Char('<') &&
                   MH_SUBSTR(i, 4) == QLatin1String("<!--")) {
            i = highlightInlineComment(text, i);
        } else if (currentChar == QLatin1Char('!') && i + 2 < text.length()
                   && text.at(i + 1) == QLatin1Char('[') && text.at(i + 2) == QLatin1Char('[')) {
            // Obsidian embed: ![[...]]
            i = highlightObsidianWikiLink(text, i);
        } else if (currentChar == QLatin1Char('[') && i + 1 < text.length()
                   && text.at(i + 1) == QLatin1Char('[')) {
            // Obsidian wikilink: [[...]]
            i = highlightObsidianWikiLink(text, i);
        } else {
            i = highlightLinkOrImage(text, i);
        }
    }
```

- [ ] **Step 3: Build and verify**

Run: `cmake --build build 2>&1 | tail -3`

Expected: Builds cleanly.

- [ ] **Step 4: Commit in submodule and parent**

```bash
cd libs/qmarkdowntextedit
git add markdownhighlighter.cpp
git commit -m "feat: implement wikilink and embed highlighting

[[Note]], [[Note|Display]], ![[embed]] with bracket masking,
link/embed format, pipe separator, and range tracking."

cd ../..
git add libs/qmarkdowntextedit
git commit -m "chore: update submodule — wikilink/embed highlighting"
```

---

### Task 3: Tag, Callout, and Block Reference Highlighting

**Files:**
- Modify: `libs/qmarkdowntextedit/markdownhighlighter.cpp`

- [ ] **Step 1: Implement highlightObsidianTags()**

Replace the stub:

```cpp
void MarkdownHighlighter::highlightObsidianTags(const QString &text) {
    // Pattern: #tag, #nested/tag — must start with letter or underscore after #
    // Must not be a heading (# followed by space) or inside code span
    // Must not be preceded by & (HTML entities like &#123;)
    static const QRegularExpression tagPattern(
        QStringLiteral(R"((?<![&\w])#([a-zA-Z_][a-zA-Z0-9_/-]*))"));

    auto it = tagPattern.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        int start = match.capturedStart();
        int length = match.capturedLength();

        // Skip if inside code span
        if (isPosInACodeSpan(currentBlock().blockNumber(), start)) {
            continue;
        }

        // Skip if this is a heading marker (# followed by space at line start)
        if (start == 0 && match.capturedEnd() < text.length()
            && text.at(match.capturedEnd()) == QLatin1Char(' ')) {
            continue;
        }

        setFormat(start, length, _formats[ObsidianTag]);
    }
}
```

- [ ] **Step 2: Implement highlightObsidianCallouts()**

Replace the stub:

```cpp
void MarkdownHighlighter::highlightObsidianCallouts(const QString &text) {
    // Pattern: > [!type] at start of blockquote line
    // Optionally followed by +/- (fold indicator) and title
    static const QRegularExpression calloutPattern(
        QStringLiteral(R"(^>\s*(\[!)([a-zA-Z]+)(\])([+-])?)"));

    auto match = calloutPattern.match(text);
    if (!match.hasMatch()) return;

    const auto maskedFormat = _formats[MaskedSyntax];

    // [! delimiter
    setFormat(match.capturedStart(1), match.capturedLength(1), maskedFormat);
    // Type keyword (note, warning, tip, etc.)
    setFormat(match.capturedStart(2), match.capturedLength(2), _formats[ObsidianCallout]);
    // ] delimiter
    setFormat(match.capturedStart(3), match.capturedLength(3), maskedFormat);
    // Optional +/- fold indicator
    if (match.lastCapturedIndex() >= 4 && match.capturedLength(4) > 0) {
        setFormat(match.capturedStart(4), match.capturedLength(4), maskedFormat);
    }
}
```

- [ ] **Step 3: Implement highlightObsidianBlockRef()**

Replace the stub:

```cpp
void MarkdownHighlighter::highlightObsidianBlockRef(const QString &text) {
    // Pattern: ^block-id at end of line, preceded by space
    static const QRegularExpression blockRefPattern(
        QStringLiteral(R"(\s(\^[a-zA-Z0-9-]+)$)"));

    auto match = blockRefPattern.match(text);
    if (!match.hasMatch()) return;

    // Format just the ^block-id part (group 1, not the leading space)
    setFormat(match.capturedStart(1), match.capturedLength(1), _formats[ObsidianBlockRef]);
}
```

- [ ] **Step 4: Wire tags into highlightInlineRules()**

In `highlightInlineRules()`, after the `highlightEmAndStrong(text, 0)` call at the end, add:

```cpp
    highlightObsidianTags(text);
```

Tags are applied after emphasis so they don't interfere with `##` heading detection.

- [ ] **Step 5: Wire callouts and block refs into highlightMarkdown()**

In `highlightMarkdown()`, inside the `if (!text.isEmpty() && !isBlockCodeBlock)` block, after `highlightInlineRules(text)`, add:

```cpp
        highlightObsidianCallouts(text);
        highlightObsidianBlockRef(text);
```

The block should now end:

```cpp
        highlightInlineRules(text);

        // Obsidian block-level syntax
        highlightObsidianCallouts(text);
        highlightObsidianBlockRef(text);
    }
```

- [ ] **Step 6: Build and verify**

Run: `cmake --build build 2>&1 | tail -3`

Expected: Builds cleanly.

- [ ] **Step 7: Commit in submodule and parent**

```bash
cd libs/qmarkdowntextedit
git add markdownhighlighter.cpp
git commit -m "feat: implement tag, callout, and block reference highlighting

#tags with nested/path support, > [!type] callout keywords,
^block-id references at end of line."

cd ../..
git add libs/qmarkdowntextedit
git commit -m "chore: update submodule — tag/callout/block-ref highlighting"
```

---

### Task 4: Highlight and Comment Syntax

**Files:**
- Modify: `libs/qmarkdowntextedit/markdownhighlighter.cpp`

- [ ] **Step 1: Implement highlightObsidianHighlight()**

Replace the stub:

```cpp
int MarkdownHighlighter::highlightObsidianHighlight(const QString &text, int startPos) {
    // Pattern: ==highlighted text==
    if (startPos + 1 >= text.length()) return startPos;
    if (text.at(startPos + 1) != QLatin1Char('=')) return startPos;

    // Skip if inside code span
    if (isPosInACodeSpan(currentBlock().blockNumber(), startPos)) {
        return startPos;
    }

    // Find closing ==
    int searchFrom = startPos + 2;
    int closingPos = text.indexOf(QStringLiteral("=="), searchFrom);
    if (closingPos == -1 || closingPos == searchFrom) return startPos;

    const auto maskedFormat = _formats[MaskedSyntax];

    // Opening ==
    setFormat(startPos, 2, maskedFormat);
    // Highlighted content
    setFormat(startPos + 2, closingPos - startPos - 2, _formats[ObsidianHighlight]);
    // Closing ==
    setFormat(closingPos, 2, maskedFormat);

    return closingPos + 1; // +1 because we consumed the second =, loop will advance past it
}
```

- [ ] **Step 2: Implement highlightObsidianComments()**

Replace the stub:

```cpp
void MarkdownHighlighter::highlightObsidianComments(const QString &text) {
    // Pattern: %%comment%% — can be inline or span multiple lines
    const auto commentFormat = _formats[ObsidianComment];
    const auto maskedFormat = _formats[MaskedSyntax];

    // Check if we're continuing a multi-line comment from previous block
    if (previousBlockState() == ObsidianComment) {
        int closingPos = text.indexOf(QStringLiteral("%%"));
        if (closingPos == -1) {
            // Entire line is still in comment
            setFormat(0, text.length(), commentFormat);
            setCurrentBlockState(ObsidianComment);
            return;
        } else {
            // Comment ends on this line
            setFormat(0, closingPos, commentFormat);
            setFormat(closingPos, 2, maskedFormat);
            // Continue looking for more comments on the same line
        }
    }

    // Find inline %%...%% patterns
    int searchFrom = 0;
    if (previousBlockState() == ObsidianComment) {
        // Skip past the closing %% we just handled
        int closingPos = text.indexOf(QStringLiteral("%%"));
        if (closingPos != -1) searchFrom = closingPos + 2;
    }

    while (searchFrom < text.length()) {
        int openPos = text.indexOf(QStringLiteral("%%"), searchFrom);
        if (openPos == -1) break;

        int contentStart = openPos + 2;
        int closePos = text.indexOf(QStringLiteral("%%"), contentStart);

        if (closePos == -1) {
            // Opening %% without closing — multi-line comment starts
            setFormat(openPos, 2, maskedFormat);
            setFormat(contentStart, text.length() - contentStart, commentFormat);
            setCurrentBlockState(ObsidianComment);
            return;
        }

        // Inline comment: %%content%%
        setFormat(openPos, 2, maskedFormat);
        setFormat(contentStart, closePos - contentStart, commentFormat);
        setFormat(closePos, 2, maskedFormat);

        searchFrom = closePos + 2;
    }
}
```

- [ ] **Step 3: Wire highlight into highlightInlineRules()**

In `highlightInlineRules()`, in the for-loop, add a case for `=` before the else/`highlightLinkOrImage` branch:

```cpp
        } else if (currentChar == QLatin1Char('=') && i + 1 < text.length()
                   && text.at(i + 1) == QLatin1Char('=')) {
            // Obsidian highlight: ==text==
            i = highlightObsidianHighlight(text, i);
        } else {
            i = highlightLinkOrImage(text, i);
        }
```

- [ ] **Step 4: Wire comments into highlightMarkdown()**

In `highlightMarkdown()`, add `highlightObsidianComments(text)` call. This needs to run for ALL lines (including inside code blocks for block-state tracking), so place it after the code block section, alongside `highlightFrontmatterBlock`:

```cpp
    highlightCommentBlock(text);
    if (isBlockCodeBlock) highlightCodeFence(text);
    highlightFrontmatterBlock(text);
    highlightObsidianComments(text);  // Add here — needs block state for multi-line
```

But wait — comments should NOT be highlighted inside code blocks. Add a guard:

```cpp
    if (!isBlockCodeBlock) {
        highlightObsidianComments(text);
    }
```

Final `highlightMarkdown()` ending:

```cpp
    highlightCommentBlock(text);
    if (isBlockCodeBlock) highlightCodeFence(text);
    highlightFrontmatterBlock(text);
    if (!isBlockCodeBlock) highlightObsidianComments(text);
}
```

- [ ] **Step 5: Build and verify**

Run: `cmake --build build 2>&1 | tail -3`

Expected: Builds cleanly.

- [ ] **Step 6: Commit in submodule and parent**

```bash
cd libs/qmarkdowntextedit
git add markdownhighlighter.cpp
git commit -m "feat: implement ==highlight== and %%comment%% syntax

==text== with yellow background. %%comment%% with gray italic,
supporting both inline and multi-line block state."

cd ../..
git add libs/qmarkdowntextedit
git commit -m "chore: update submodule — highlight/comment syntax"
```

---

### Task 5: Unit Tests

**Files:**
- Create: `tests/editor/CMakeLists.txt`
- Create: `tests/editor/tst_obsidian_highlighting.cpp`
- Modify: `CMakeLists.txt` (add test subdirectory)

- [ ] **Step 1: Write tests**

`tests/editor/tst_obsidian_highlighting.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextLayout>
#include "markdownhighlighter.h"

class TestObsidianHighlighting : public QObject {
    Q_OBJECT

    // Helper: get format at a character position in first block
    QTextCharFormat formatAt(QTextDocument *doc, int pos, int block = 0) {
        auto tb = doc->findBlockByNumber(block);
        auto *layout = tb.layout();
        auto ranges = layout->formats();
        for (const auto &range : ranges) {
            if (pos >= range.start && pos < range.start + range.length) {
                return range.format;
            }
        }
        return {};
    }

    // Helper: check if position has a specific foreground color
    bool hasForeground(QTextDocument *doc, int pos, const QColor &color, int block = 0) {
        auto fmt = formatAt(doc, pos, block);
        return fmt.foreground().color() == color;
    }

    // Obsidian format colors (must match initTextFormats)
    const QColor wikiLinkColor{123, 108, 217};
    const QColor tagColor{224, 108, 117};
    const QColor calloutColor{209, 154, 102};
    const QColor commentColor{92, 99, 112};
    const QColor maskedColor{204, 204, 204};
    const QColor highlightBg{255, 243, 176};

    QTextDocument *createDoc(const QString &text) {
        auto *doc = new QTextDocument(this);
        auto *hl = new MarkdownHighlighter(doc);
        Q_UNUSED(hl)
        doc->setPlainText(text);
        // Force highlighting to run
        for (auto block = doc->begin(); block != doc->end(); block = block.next()) {
            block.layout()->formats(); // Trigger lazy formatting
        }
        // Process events to allow deferred highlighting
        QCoreApplication::processEvents();
        return doc;
    }

private Q_SLOTS:
    void testWikiLinkBasic()
    {
        auto *doc = createDoc(QStringLiteral("See [[My Note]] here"));
        // "[[" at pos 4-5 should be masked
        QVERIFY(hasForeground(doc, 4, maskedColor));
        // "My Note" at pos 6-12 should be wikilink color
        QVERIFY(hasForeground(doc, 6, wikiLinkColor));
        // "]]" at pos 13-14 should be masked
        QVERIFY(hasForeground(doc, 13, maskedColor));
    }

    void testWikiLinkWithAlias()
    {
        auto *doc = createDoc(QStringLiteral("[[Note|Display Text]]"));
        // "Note" at pos 2-5 should be wikilink
        QVERIFY(hasForeground(doc, 2, wikiLinkColor));
        // "|" at pos 6 should be masked
        QVERIFY(hasForeground(doc, 6, maskedColor));
        // "Display Text" at pos 7-18 should be wikilink
        QVERIFY(hasForeground(doc, 7, wikiLinkColor));
    }

    void testEmbed()
    {
        auto *doc = createDoc(QStringLiteral("![[image.png]]"));
        // Content should have embed color (same as wikilink)
        QVERIFY(hasForeground(doc, 3, wikiLinkColor));
    }

    void testTag()
    {
        auto *doc = createDoc(QStringLiteral("Hello #project tag"));
        // "#project" at pos 6-13 should be tag color
        QVERIFY(hasForeground(doc, 6, tagColor));
    }

    void testNestedTag()
    {
        auto *doc = createDoc(QStringLiteral("#parent/child/deep"));
        QVERIFY(hasForeground(doc, 0, tagColor));
    }

    void testHeadingNotTag()
    {
        auto *doc = createDoc(QStringLiteral("# Heading"));
        // "#" should NOT be tag color — it's a heading
        QVERIFY(!hasForeground(doc, 0, tagColor));
    }

    void testCallout()
    {
        auto *doc = createDoc(QStringLiteral("> [!warning] Be careful"));
        // "warning" should be callout color
        // Find where "warning" starts — "> [!" is 4 chars
        QVERIFY(hasForeground(doc, 4, calloutColor));
    }

    void testHighlightSyntax()
    {
        auto *doc = createDoc(QStringLiteral("Some ==highlighted== text"));
        // "==" delimiters should be masked
        QVERIFY(hasForeground(doc, 5, maskedColor));  // opening ==
        // "highlighted" should have highlight background
        auto fmt = formatAt(doc, 7);
        QCOMPARE(fmt.background().color(), highlightBg);
    }

    void testComment()
    {
        auto *doc = createDoc(QStringLiteral("Visible %%hidden%% visible"));
        // "%%" delimiters should be masked
        QVERIFY(hasForeground(doc, 8, maskedColor));
        // "hidden" should be comment color
        QVERIFY(hasForeground(doc, 10, commentColor));
    }

    void testBlockRef()
    {
        auto *doc = createDoc(QStringLiteral("Some text ^my-block-id"));
        // "^my-block-id" should be block ref color
        QVERIFY(hasForeground(doc, 10, commentColor));  // Same gray as blockref
    }

    void testWikiLinkNotInCode()
    {
        auto *doc = createDoc(QStringLiteral("`[[not a link]]`"));
        // Inside code span — should NOT be wikilink color
        QVERIFY(!hasForeground(doc, 3, wikiLinkColor));
    }

    void testTagNotInCode()
    {
        auto *doc = createDoc(QStringLiteral("`#not-a-tag`"));
        QVERIFY(!hasForeground(doc, 1, tagColor));
    }
};

QTEST_MAIN(TestObsidianHighlighting)
#include "tst_obsidian_highlighting.moc"
```

- [ ] **Step 2: Create test CMakeLists**

`tests/editor/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_EditorTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_obsidian_highlighting tst_obsidian_highlighting.cpp)
add_test(NAME tst_obsidian_highlighting COMMAND tst_obsidian_highlighting)
target_link_libraries(tst_obsidian_highlighting PRIVATE
    Qt6::Test Qt6::Widgets qmarkdowntextedit)
set_tests_properties(tst_obsidian_highlighting PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 3: Add test subdirectory to root CMakeLists.txt**

Add after the other test subdirectories:
```cmake
add_subdirectory(tests/editor)
```

- [ ] **Step 4: Build and run tests**

Run:
```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_obsidian_highlighting --output-on-failure
```

Expected: Tests pass. If any fail due to format timing (highlighter deferred processing), the test helper may need `QTest::qWait(100)` before checking formats. Adjust if needed.

- [ ] **Step 5: Run all tests**

Run: `cd build && ctest --output-on-failure`

Expected: All 15 tests pass (13 existing + 1 new quickswitcher + 1 new highlighting).

- [ ] **Step 6: Commit**

```bash
git add tests/editor/ CMakeLists.txt
git commit -m "feat: add unit tests for Obsidian syntax highlighting

Tests: wikilinks, aliases, embeds, tags, nested tags, heading-not-tag,
callouts, ==highlight==, %%comments%%, ^block-refs, code span exclusion."
```

---

Self-review:

1. **Spec coverage:** Wikilinks ✓, aliases ✓, embeds ✓, tags ✓, nested tags ✓, callouts ✓, ==highlight== ✓, %%comments%% (inline + multi-line) ✓, ^block-refs ✓. Code span exclusion ✓. Heading-not-tag ✓. Format colors match spec ✓. Breadcrumb comments for Batch B ✓ (InlineRange storage, `// Future: Ctrl+Click, hover preview`).

2. **Placeholder scan:** All code is complete. No TBDs.

3. **Type consistency:** `HighlighterState` enum names consistent across tasks (ObsidianTag, ObsidianCallout, ObsidianHighlight, ObsidianComment, ObsidianBlockRef, ObsidianEmbed). `_formats[]` keys match. Method signatures match declarations.
