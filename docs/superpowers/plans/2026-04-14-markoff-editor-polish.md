# Markoff Editor Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement five independent editor polish items identified in the Obsidian audit, from easiest to hardest.

**Architecture:** Each task is self-contained — no cross-dependencies. Tasks 1-4 are pure code changes in the existing markoff/markoff-parser libraries. Task 5 adds a `yaml-cpp` external dependency to markoff-parser. All follow TDD.

**Tech Stack:** C++20, Qt6, KDE Frameworks 6, tree-sitter, yaml-cpp (Task 5 only)

**Spec:** `docs/superpowers/specs/2026-04-14-markoff-editor-polish-design.md`

---

### Task 1: CJK IME Autocorrect

**Files:**
- Modify: `libs/markoff/src/MarkdownTextItem.cpp` (keyPressEvent, ~line 527)
- Create: `libs/markoff/tests/tst_cjk_autocorrect.cpp`
- Modify: `libs/markoff/tests/CMakeLists.txt`

- [x] **Step 1: Write the failing test**

Create `libs/markoff/tests/tst_cjk_autocorrect.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QObject>
#include <QTest>
#include <QGraphicsScene>

#include "MarkdownTextItem.h"
#include "MarkdownHighlighter.h"
#include <markoff-parser/TreeSitterParser.h>

using namespace Markoff;

class TestCjkAutocorrect : public QObject {
    Q_OBJECT

private slots:
    void fullWidthDoubleBracketOpenReplacesWithWikilink();
    void fullWidthDoubleBracketCloseReplacesWithClose();
    void fullWidthExclamationBracketReplacesWithEmbed();
    void singleFullWidthBracketDoesNotReplace();
    void replacementIsUndoable();

private:
    void typeText(MarkdownTextItem *item, const QString &text);
    QGraphicsScene m_scene;
};

void TestCjkAutocorrect::typeText(MarkdownTextItem *item, const QString &text)
{
    for (const QChar &ch : text) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        item->keyPressEvent(&event);
    }
}

void TestCjkAutocorrect::fullWidthDoubleBracketOpenReplacesWithWikilink()
{
    auto *item = new MarkdownTextItem;
    m_scene.addItem(item);
    item->setPlainText({});

    typeText(item, QStringLiteral("\u3010\u3010")); // 【【
    QCOMPARE(item->document()->toPlainText(), QStringLiteral("[["));
}

void TestCjkAutocorrect::fullWidthDoubleBracketCloseReplacesWithClose()
{
    auto *item = new MarkdownTextItem;
    m_scene.addItem(item);
    item->setPlainText({});

    typeText(item, QStringLiteral("\u3011\u3011")); // 】】
    QCOMPARE(item->document()->toPlainText(), QStringLiteral("]]"));
}

void TestCjkAutocorrect::fullWidthExclamationBracketReplacesWithEmbed()
{
    auto *item = new MarkdownTextItem;
    m_scene.addItem(item);
    item->setPlainText({});

    typeText(item, QStringLiteral("\uff01\u3010\u3010")); // ！【【
    QCOMPARE(item->document()->toPlainText(), QStringLiteral("![["));
}

void TestCjkAutocorrect::singleFullWidthBracketDoesNotReplace()
{
    auto *item = new MarkdownTextItem;
    m_scene.addItem(item);
    item->setPlainText({});

    typeText(item, QStringLiteral("\u3010")); // single 【
    QCOMPARE(item->document()->toPlainText(), QStringLiteral("\u3010"));
}

void TestCjkAutocorrect::replacementIsUndoable()
{
    auto *item = new MarkdownTextItem;
    m_scene.addItem(item);
    item->setPlainText({});

    typeText(item, QStringLiteral("\u3010\u3010")); // 【【 → [[
    QCOMPARE(item->document()->toPlainText(), QStringLiteral("[["));

    item->document()->undo();
    // Undo should remove the [[ (the replacement was grouped with input)
    QVERIFY(item->document()->toPlainText() != QStringLiteral("[["));
}

QTEST_MAIN(TestCjkAutocorrect)
#include "tst_cjk_autocorrect.moc"
```

- [x] **Step 2: Register the test in CMakeLists.txt**

Add to `libs/markoff/tests/CMakeLists.txt`:

```cmake
add_executable(tst_markoff_cjk_autocorrect tst_cjk_autocorrect.cpp)
add_test(NAME tst_markoff_cjk_autocorrect COMMAND tst_markoff_cjk_autocorrect)
target_link_libraries(tst_markoff_cjk_autocorrect PRIVATE Qt6::Test Qt6::Widgets markoff)
target_include_directories(tst_markoff_cjk_autocorrect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set_tests_properties(tst_markoff_cjk_autocorrect PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [x] **Step 3: Build and verify tests fail**

Run:
```bash
cd /home/clinton/dev/Corbomite && cmake --build build --target tst_markoff_cjk_autocorrect
cd build && ./bin/tst_markoff_cjk_autocorrect
```
Expected: Tests fail (no autocorrect implemented yet, full-width chars remain).

- [x] **Step 4: Implement CJK autocorrect in keyPressEvent**

In `libs/markoff/src/MarkdownTextItem.cpp`, modify `keyPressEvent()` to add the autocorrect after `m_control->processEvent(event)`:

```cpp
void MarkdownTextItem::keyPressEvent(QKeyEvent *event)
{
    // Check for cursor-at-boundary before forwarding
    QTextCursor cursor = m_control->textCursor();
    bool atStart = cursor.atStart();
    bool atEnd = cursor.atEnd();

    m_control->processEvent(event);

    // CJK full-width bracket autocorrect (Obsidian compat).
    // Longest match first: ！【【 before 【【.
    if (!event->text().isEmpty()) {
        QTextCursor c = m_control->textCursor();
        int pos = c.position();
        if (pos >= 3) {
            c.setPosition(pos - 3);
            c.setPosition(pos, QTextCursor::KeepAnchor);
            if (c.selectedText() == QStringLiteral("\uff01\u3010\u3010")) {
                c.beginEditBlock();
                c.removeSelectedText();
                c.insertText(QStringLiteral("![["));
                c.endEditBlock();
                goto cjk_done;
            }
        }
        if (pos >= 2) {
            c = m_control->textCursor();
            c.setPosition(pos - 2);
            c.setPosition(pos, QTextCursor::KeepAnchor);
            QString sel = c.selectedText();
            if (sel == QStringLiteral("\u3010\u3010")) {
                c.beginEditBlock();
                c.removeSelectedText();
                c.insertText(QStringLiteral("[["));
                c.endEditBlock();
            } else if (sel == QStringLiteral("\u3011\u3011")) {
                c.beginEditBlock();
                c.removeSelectedText();
                c.insertText(QStringLiteral("]]"));
                c.endEditBlock();
            }
        }
    }
    cjk_done:

    // If cursor didn't move for arrow keys, we're at a boundary
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Home) {
        if (atStart && m_control->textCursor().atStart())
            emit cursorAtBoundary(Qt::TopEdge);
    } else if (event->key() == Qt::Key_Down || event->key() == Qt::Key_End) {
        if (atEnd && m_control->textCursor().atEnd())
            emit cursorAtBoundary(Qt::BottomEdge);
    }
}
```

- [x] **Step 5: Build and verify tests pass**

Run:
```bash
cd /home/clinton/dev/Corbomite && cmake --build build --target tst_markoff_cjk_autocorrect
cd build && ./bin/tst_markoff_cjk_autocorrect
```
Expected: All 5 tests PASS.

- [x] **Step 6: Run full markoff test suite**

Run:
```bash
cd /home/clinton/dev/Corbomite/build && find . -name "tst_markoff*" -type f -executable | while read t; do "$t" 2>&1 | grep -E "^(Totals|FAIL)"; done
```
Expected: All existing tests still pass.

- [x] **Step 7: Commit**

```bash
cd /home/clinton/dev/Corbomite && git add libs/markoff/src/MarkdownTextItem.cpp libs/markoff/tests/tst_cjk_autocorrect.cpp libs/markoff/tests/CMakeLists.txt
git commit -m "feat(markoff): CJK full-width bracket autocorrect

Auto-replace 【【→[[, 】】→]], ！【【→![[ after text input.
Longest-match-first, grouped as single undo unit."
```

---

### Task 2: `parseLinktext` Subpath Extraction

**Files:**
- Create: `libs/markoff-parser/include/markoff-parser/LinkTextParser.h`
- Create: `libs/markoff-parser/src/LinkTextParser.cpp`
- Modify: `libs/markoff-parser/CMakeLists.txt`
- Create: `libs/markoff-parser/tests/tst_linktext.cpp`
- Modify: `libs/markoff-parser/tests/CMakeLists.txt`

- [x] **Step 1: Write the failing test**

Create `libs/markoff-parser/tests/tst_linktext.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QObject>
#include <QTest>
#include <markoff-parser/LinkTextParser.h>

using namespace Markoff;

class TestLinktext : public QObject {
    Q_OBJECT
private slots:
    void pathWithHeading();
    void pathWithBlockRef();
    void pathOnly();
    void headingOnly();
    void blockRefOnly();
    void emptyString();
    void pathWithSpacesInSubpath();
    void multipleHashes();
};

void TestLinktext::pathWithHeading()
{
    auto r = parseLinktext(QStringLiteral("Note#Heading"));
    QCOMPARE(r.path, QStringLiteral("Note"));
    QCOMPARE(r.subpath, QStringLiteral("#Heading"));
}

void TestLinktext::pathWithBlockRef()
{
    auto r = parseLinktext(QStringLiteral("Note#^blockid"));
    QCOMPARE(r.path, QStringLiteral("Note"));
    QCOMPARE(r.subpath, QStringLiteral("#^blockid"));
}

void TestLinktext::pathOnly()
{
    auto r = parseLinktext(QStringLiteral("Note"));
    QCOMPARE(r.path, QStringLiteral("Note"));
    QCOMPARE(r.subpath, QString());
}

void TestLinktext::headingOnly()
{
    auto r = parseLinktext(QStringLiteral("#Heading"));
    QCOMPARE(r.path, QString());
    QCOMPARE(r.subpath, QStringLiteral("#Heading"));
}

void TestLinktext::blockRefOnly()
{
    auto r = parseLinktext(QStringLiteral("#^block"));
    QCOMPARE(r.path, QString());
    QCOMPARE(r.subpath, QStringLiteral("#^block"));
}

void TestLinktext::emptyString()
{
    auto r = parseLinktext(QString());
    QCOMPARE(r.path, QString());
    QCOMPARE(r.subpath, QString());
}

void TestLinktext::pathWithSpacesInSubpath()
{
    auto r = parseLinktext(QStringLiteral("Note.md#Sub heading with spaces"));
    QCOMPARE(r.path, QStringLiteral("Note.md"));
    QCOMPARE(r.subpath, QStringLiteral("#Sub heading with spaces"));
}

void TestLinktext::multipleHashes()
{
    auto r = parseLinktext(QStringLiteral("Note#First#Second"));
    QCOMPARE(r.path, QStringLiteral("Note"));
    QCOMPARE(r.subpath, QStringLiteral("#First#Second"));
}

QTEST_APPLESS_MAIN(TestLinktext)
#include "tst_linktext.moc"
```

- [x] **Step 2: Create the header**

Create `libs/markoff-parser/include/markoff-parser/LinkTextParser.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_LINKTEXTPARSER_H
#define MARKOFF_LINKTEXTPARSER_H

#include <QString>

namespace Markoff {

struct LinkTarget {
    QString path;       ///< "Note" or "Note.md"
    QString subpath;    ///< "#Heading", "#^blockid", or "" if none
};

/// Split a wikilink target into path and subpath at the first '#'.
/// Pipe display text (|alias) must already be stripped before calling.
LinkTarget parseLinktext(const QString &linktext);

} // namespace Markoff

#endif // MARKOFF_LINKTEXTPARSER_H
```

- [x] **Step 3: Register test and source in CMakeLists**

Add to `libs/markoff-parser/CMakeLists.txt` source list:

```cmake
    src/LinkTextParser.cpp
```

Add to `libs/markoff-parser/tests/CMakeLists.txt`:

```cmake
add_executable(tst_markoff_parser_linktext tst_linktext.cpp)
add_test(NAME tst_markoff_parser_linktext COMMAND tst_markoff_parser_linktext)
target_link_libraries(tst_markoff_parser_linktext PRIVATE Qt6::Test markoff-parser)
set_tests_properties(tst_markoff_parser_linktext PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [x] **Step 4: Create stub implementation (tests should fail)**

Create `libs/markoff-parser/src/LinkTextParser.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <markoff-parser/LinkTextParser.h>

namespace Markoff {

LinkTarget parseLinktext(const QString &linktext)
{
    Q_UNUSED(linktext);
    return {};
}

} // namespace Markoff
```

Build and verify tests fail:
```bash
cd /home/clinton/dev/Corbomite && cmake --build build --target tst_markoff_parser_linktext
cd build && ./bin/tst_markoff_parser_linktext
```
Expected: Most tests FAIL (stub returns empty for everything).

- [x] **Step 5: Implement parseLinktext**

Replace the stub in `libs/markoff-parser/src/LinkTextParser.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <markoff-parser/LinkTextParser.h>

namespace Markoff {

LinkTarget parseLinktext(const QString &linktext)
{
    if (linktext.isEmpty())
        return {};

    const int hashIdx = linktext.indexOf(QLatin1Char('#'));
    if (hashIdx < 0)
        return {linktext, {}};

    return {linktext.left(hashIdx), linktext.mid(hashIdx)};
}

} // namespace Markoff
```

- [x] **Step 6: Build and verify all tests pass**

```bash
cd /home/clinton/dev/Corbomite && cmake --build build --target tst_markoff_parser_linktext
cd build && ./bin/tst_markoff_parser_linktext
```
Expected: All 8 tests PASS.

- [x] **Step 7: Run full parser test suite**

```bash
cd /home/clinton/dev/Corbomite/build && find . -name "tst_markoff_parser*" -type f -executable | while read t; do "$t" 2>&1 | grep -E "^(Totals|FAIL)"; done
```
Expected: All existing parser tests still pass.

- [x] **Step 8: Commit**

```bash
cd /home/clinton/dev/Corbomite && git add libs/markoff-parser/include/markoff-parser/LinkTextParser.h libs/markoff-parser/src/LinkTextParser.cpp libs/markoff-parser/CMakeLists.txt libs/markoff-parser/tests/tst_linktext.cpp libs/markoff-parser/tests/CMakeLists.txt
git commit -m "feat(markoff-parser): add parseLinktext for [[Note#Heading]] splitting

Splits wikilink target at first '#' into path and subpath.
Enables scroll-to-heading on link click."
```

---

### Task 3: Triple-Click Line-Extend Selection

**Files:**
- Modify: `libs/markoff/include/markoff/EditorSettings.h`
- Modify: `libs/markoff/src/TextControl.cpp` (~lines 1746-1756)
- Modify: `libs/markoff/src/TextControl_p.h` (add state flag)

- [x] **Step 1: Investigate current behavior**

Launch the test app and triple-click on a line, then drag downward:
```bash
cd /home/clinton/dev/Corbomite && ./build/bin/markoff-testapp libs/markoff/tests/showcase.md &
```

Test manually:
1. Triple-click on "First item" in the Unordered list section
2. Hold mouse button and drag downward to "Second item"
3. Observe: does the selection extend by whole lines, or by individual characters?

Record the result. If selection extends by whole lines already, skip to Step 7 (document and commit). If it extends by characters, continue to Step 2.

- [x] **Step 2: Add `tripleClickSelectsLine` to EditorSettings**

In `libs/markoff/include/markoff/EditorSettings.h`, add the new field:

```cpp
struct EditorSettings {
    int tabSize = 4;
    bool lineNumbers = false;
    bool lineWrap = true;
    bool highlightCurrentLine = true;
    bool highlightingEnabled = true;
    bool tripleClickSelectsLine = true; // false = Qt default (paragraph)
};
```

- [x] **Step 3: Add triple-click state tracking to TextControl**

In `libs/markoff/src/TextControl_p.h`, add a flag to the private data (alongside the existing `selectedBlockOnTrippleClick` member):

```cpp
bool tripleClickDragActive = false;
```

- [x] **Step 4: Modify mouseMoveEvent for line-granularity drag after triple-click**

In `libs/markoff/src/TextControl.cpp`, find the `mouseMoveEvent` handler. When `tripleClickDragActive` is true, snap the selection to line boundaries:

The existing triple-click code at ~line 1746 sets `selectedBlockOnTrippleClick`. After that block, set `tripleClickDragActive = true`.

In the mouseMoveEvent drag handler, when `tripleClickDragActive` is true, extend selection by whole lines instead of characters. Reset `tripleClickDragActive = false` in `mouseReleaseEvent`.

The exact implementation depends on what Step 1 reveals. If Qt already handles this (the `selectedBlockOnTrippleClick` cursor is used for drag extension), this step may be unnecessary.

- [x] **Step 5: Build and test manually**

```bash
cd /home/clinton/dev/Corbomite && cmake --build build --target markoff-testapp
```

Repeat the manual test from Step 1. Verify line-extend behavior.

- [x] **Step 6: Run full test suite**

```bash
cd /home/clinton/dev/Corbomite/build && find . -name "tst_markoff*" -type f -executable | while read t; do "$t" 2>&1 | grep -E "^(Totals|FAIL)"; done
```
Expected: All existing tests still pass.

- [x] **Step 7: Commit**

```bash
cd /home/clinton/dev/Corbomite && git add libs/markoff/include/markoff/EditorSettings.h libs/markoff/src/TextControl.cpp libs/markoff/src/TextControl_p.h
git commit -m "feat(markoff): triple-click line-extend selection

Add tripleClickSelectsLine EditorSettings toggle (default true).
When enabled, drag after triple-click extends by whole lines."
```

---

### Task 4: Atomic Undo Grouping for Multi-Line Toggles

**Files:**
- Modify: `libs/markoff/src/Editor.cpp` (toggle methods)
- Create: `libs/markoff/tests/tst_undo_grouping.cpp`
- Modify: `libs/markoff/tests/CMakeLists.txt`

- [x] **Step 1: Write the failing test**

Create `libs/markoff/tests/tst_undo_grouping.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QObject>
#include <QTest>
#include "markoff/Editor.h"

using namespace Markoff;

class TestUndoGrouping : public QObject {
    Q_OBJECT
private slots:
    void headingToggleOnMultipleLinesIsAtomicUndo();
};

void TestUndoGrouping::headingToggleOnMultipleLinesIsAtomicUndo()
{
    Editor editor;
    editor.setPlainText(QStringLiteral("Line one\nLine two\nLine three"));

    // Select all three lines
    editor.selectAll();

    // Toggle heading (increase level)
    editor.increaseHeadingLevel();

    // Verify all lines have heading markers
    QString after = editor.toPlainText();
    QVERIFY(after.contains(QStringLiteral("# Line one")));
    QVERIFY(after.contains(QStringLiteral("# Line two")));
    QVERIFY(after.contains(QStringLiteral("# Line three")));

    // Single undo should restore ALL lines
    editor.undo();
    QString undone = editor.toPlainText();
    QCOMPARE(undone, QStringLiteral("Line one\nLine two\nLine three"));
}

QTEST_MAIN(TestUndoGrouping)
#include "tst_undo_grouping.moc"
```

- [x] **Step 2: Register the test**

Add to `libs/markoff/tests/CMakeLists.txt`:

```cmake
add_executable(tst_markoff_undo_grouping tst_undo_grouping.cpp)
add_test(NAME tst_markoff_undo_grouping COMMAND tst_markoff_undo_grouping)
target_link_libraries(tst_markoff_undo_grouping PRIVATE Qt6::Test Qt6::Widgets markoff)
set_tests_properties(tst_markoff_undo_grouping PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [x] **Step 3: Build and verify test fails**

```bash
cd /home/clinton/dev/Corbomite && cmake --build build --target tst_markoff_undo_grouping
cd build && ./bin/tst_markoff_undo_grouping
```
Expected: FAIL — undo only restores one line instead of all three.

- [x] **Step 4: Audit and fix Editor.cpp toggle methods**

In `libs/markoff/src/Editor.cpp`, read each toggle method. For any method that modifies the document across multiple lines without `beginEditBlock`/`endEditBlock`, wrap the multi-line operation. The key methods to check and fix:

- `increaseHeadingLevel()` (~line 701): wraps the text cursor operations in `beginEditBlock`/`endEditBlock`
- `decreaseHeadingLevel()` (~line 717): same
- `toggleCheckbox()` (~line 730): same
- `insertBlockQuote()`: same
- `wrapSelection()` (~line 600): if it handles multi-line selections, same

For each method, the pattern is:
```cpp
void Editor::increaseHeadingLevel()
{
    auto *ti = focusedTextItem();
    if (!ti) return;
    QTextCursor cursor = ti->textControl()->textCursor();
    cursor.beginEditBlock();  // <-- ADD
    // ... existing line-by-line manipulation ...
    cursor.endEditBlock();    // <-- ADD
    ti->textControl()->setTextCursor(cursor);
}
```

Read each method's body to confirm it iterates lines. Wrap only those that do multi-line work.

- [x] **Step 5: Build and verify test passes**

```bash
cd /home/clinton/dev/Corbomite && cmake --build build --target tst_markoff_undo_grouping
cd build && ./bin/tst_markoff_undo_grouping
```
Expected: PASS — single undo restores all three lines.

- [x] **Step 6: Run full test suite**

```bash
cd /home/clinton/dev/Corbomite/build && find . -name "tst_markoff*" -type f -executable | while read t; do "$t" 2>&1 | grep -E "^(Totals|FAIL)"; done
```
Expected: All tests pass.

- [x] **Step 7: Commit**

```bash
cd /home/clinton/dev/Corbomite && git add libs/markoff/src/Editor.cpp libs/markoff/tests/tst_undo_grouping.cpp libs/markoff/tests/CMakeLists.txt
git commit -m "fix(markoff): group multi-line toggles as single undo unit

Wrap increaseHeadingLevel, decreaseHeadingLevel, toggleCheckbox,
insertBlockQuote, and wrapSelection in beginEditBlock/endEditBlock
so Ctrl+Z undoes the entire multi-line operation atomically."
```

---

### Task 5: YAML Frontmatter Parsing

**Files:**
- Modify: `libs/markoff-parser/CMakeLists.txt` (add yaml-cpp)
- Modify: `libs/markoff-parser/include/markoff-parser/Document.h`
- Modify: `libs/markoff-parser/src/Document.cpp`
- Create: `libs/markoff-parser/tests/tst_frontmatter.cpp`
- Modify: `libs/markoff-parser/tests/CMakeLists.txt`

- [x] **Step 1: Verify yaml-cpp is installed**

```bash
pkg-config --modversion yaml-cpp || pacman -Qi yaml-cpp
```
Expected: Version printed (e.g. `0.8.0`). If not installed:
```bash
sudo pacman -S yaml-cpp
```

- [x] **Step 2: Write the failing tests**

Create `libs/markoff-parser/tests/tst_frontmatter.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QObject>
#include <QTest>
#include <QVariant>
#include <markoff-parser/Document.h>

using namespace Markoff;

class TestFrontmatter : public QObject {
    Q_OBJECT
private slots:
    void standardFrontmatter();
    void listStyleTags();
    void commaStyleTags();
    void emptyFrontmatter();
    void invalidYaml();
    void booleanValues();
    void numericValues();
    void noFrontmatter();
};

void TestFrontmatter::standardFrontmatter()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\ntitle: My Note\ntags:\n  - foo\n  - bar\naliases:\n  - mn\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 3);
    QCOMPARE(props[0].key, QStringLiteral("title"));
    QCOMPARE(props[0].value.toString(), QStringLiteral("My Note"));
    QCOMPARE(props[1].key, QStringLiteral("tags"));
    QCOMPARE(props[1].value.toStringList(), QStringList({QStringLiteral("foo"), QStringLiteral("bar")}));
    QCOMPARE(props[2].key, QStringLiteral("aliases"));
    QCOMPARE(props[2].value.toStringList(), QStringList({QStringLiteral("mn")}));
}

void TestFrontmatter::listStyleTags()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\ntags: [alpha, beta]\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 1);
    QCOMPARE(props[0].value.toStringList(), QStringList({QStringLiteral("alpha"), QStringLiteral("beta")}));
}

void TestFrontmatter::commaStyleTags()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\ntags: alpha, beta\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 1);
    QCOMPARE(props[0].value.toStringList(), QStringList({QStringLiteral("alpha"), QStringLiteral("beta")}));
}

void TestFrontmatter::emptyFrontmatter()
{
    auto doc = Document::fromMarkdown(QStringLiteral("---\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 0);
}

void TestFrontmatter::invalidYaml()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\n: invalid: yaml: [[\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 0); // graceful empty, no crash
}

void TestFrontmatter::booleanValues()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\npublish: true\ndraft: false\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 2);
    QCOMPARE(props[0].value.toBool(), true);
    QCOMPARE(props[1].value.toBool(), false);
}

void TestFrontmatter::numericValues()
{
    auto doc = Document::fromMarkdown(QStringLiteral(
        "---\nweight: 42\nrating: 3.5\n---\nBody"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 2);
    QCOMPARE(props[0].value.toInt(), 42);
    QCOMPARE(props[1].value.toDouble(), 3.5);
}

void TestFrontmatter::noFrontmatter()
{
    auto doc = Document::fromMarkdown(QStringLiteral("Just body text"));
    auto props = doc->parsedFrontmatter();
    QCOMPARE(props.size(), 0);
}

QTEST_APPLESS_MAIN(TestFrontmatter)
#include "tst_frontmatter.moc"
```

- [x] **Step 3: Add FrontmatterProperty and parsedFrontmatter() to Document.h**

In `libs/markoff-parser/include/markoff-parser/Document.h`, add the struct and method:

```cpp
#include <QVariant>  // add to includes

// Before the Document class:
struct FrontmatterProperty {
    QString key;
    QVariant value;
};

// Inside Document class, after frontmatter():
QList<FrontmatterProperty> parsedFrontmatter() const;
```

- [x] **Step 4: Add yaml-cpp to CMakeLists.txt**

In `libs/markoff-parser/CMakeLists.txt`:

```cmake
find_package(yaml-cpp REQUIRED)
```

And in `target_link_libraries`:

```cmake
    PRIVATE
        yaml-cpp::yaml-cpp
```

Also register the test in `libs/markoff-parser/tests/CMakeLists.txt`:

```cmake
add_executable(tst_markoff_parser_frontmatter tst_frontmatter.cpp)
add_test(NAME tst_markoff_parser_frontmatter COMMAND tst_markoff_parser_frontmatter)
target_link_libraries(tst_markoff_parser_frontmatter PRIVATE Qt6::Test markoff-parser)
set_tests_properties(tst_markoff_parser_frontmatter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [x] **Step 5: Implement parsedFrontmatter() with stub**

In `libs/markoff-parser/src/Document.cpp`, add a stub:

```cpp
QList<FrontmatterProperty> Document::parsedFrontmatter() const
{
    return {};
}
```

Build and verify tests fail:
```bash
cd /home/clinton/dev/Corbomite && cmake --build build --target tst_markoff_parser_frontmatter
cd build && ./bin/tst_markoff_parser_frontmatter
```
Expected: Most tests FAIL (stub returns empty).

- [x] **Step 6: Implement parsedFrontmatter() with yaml-cpp**

Replace the stub in `libs/markoff-parser/src/Document.cpp`:

```cpp
#include <yaml-cpp/yaml.h>  // add to includes

// Helper: convert a YAML node to QVariant
static QVariant yamlNodeToVariant(const YAML::Node &node)
{
    if (!node.IsDefined() || node.IsNull())
        return {};

    if (node.IsScalar()) {
        const std::string s = node.Scalar();
        // Try bool
        if (s == "true" || s == "True" || s == "TRUE")
            return true;
        if (s == "false" || s == "False" || s == "FALSE")
            return false;
        // Try int
        bool ok = false;
        int i = QString::fromStdString(s).toInt(&ok);
        if (ok) return i;
        // Try double
        double d = QString::fromStdString(s).toDouble(&ok);
        if (ok && s.find('.') != std::string::npos) return d;
        // String
        return QString::fromStdString(s);
    }

    if (node.IsSequence()) {
        QStringList list;
        for (const auto &item : node)
            list.append(yamlNodeToVariant(item).toString());
        return list;
    }

    if (node.IsMap()) {
        QVariantMap map;
        for (const auto &pair : node)
            map.insert(QString::fromStdString(pair.first.Scalar()),
                       yamlNodeToVariant(pair.second));
        return map;
    }

    return {};
}

// Helper: normalize tags/aliases comma-separated strings to QStringList
static QVariant normalizeListValue(const QString &key, const QVariant &value)
{
    if ((key == QStringLiteral("tags") || key == QStringLiteral("aliases"))
        && value.typeId() == QMetaType::QString) {
        QStringList parts;
        for (const auto &s : value.toString().split(QLatin1Char(','),
                                                      Qt::SkipEmptyParts))
            parts.append(s.trimmed());
        return parts;
    }
    return value;
}

QList<FrontmatterProperty> Document::parsedFrontmatter() const
{
    const QString raw = d->frontmatter;
    if (raw.isEmpty())
        return {};

    QList<FrontmatterProperty> result;
    try {
        YAML::Node root = YAML::Load(raw.toStdString());
        if (!root.IsMap())
            return {};

        for (const auto &pair : root) {
            FrontmatterProperty prop;
            prop.key = QString::fromStdString(pair.first.Scalar());
            prop.value = normalizeListValue(prop.key,
                                            yamlNodeToVariant(pair.second));
            result.append(prop);
        }
    } catch (const YAML::Exception &) {
        return {};
    }
    return result;
}
```

- [x] **Step 7: Build and verify all tests pass**

```bash
cd /home/clinton/dev/Corbomite && cmake --build build --target tst_markoff_parser_frontmatter
cd build && ./bin/tst_markoff_parser_frontmatter
```
Expected: All 8 tests PASS.

- [x] **Step 8: Run full parser and markoff test suite**

```bash
cd /home/clinton/dev/Corbomite/build && find . -name "tst_markoff*" -type f -executable | while read t; do "$t" 2>&1 | grep -E "^(Totals|FAIL)"; done
```
Expected: All existing tests still pass.

- [x] **Step 9: Commit**

```bash
cd /home/clinton/dev/Corbomite && git add libs/markoff-parser/CMakeLists.txt libs/markoff-parser/include/markoff-parser/Document.h libs/markoff-parser/src/Document.cpp libs/markoff-parser/tests/tst_frontmatter.cpp libs/markoff-parser/tests/CMakeLists.txt
git commit -m "feat(markoff-parser): parse YAML frontmatter via yaml-cpp

Add Document::parsedFrontmatter() returning ordered
QList<FrontmatterProperty>. Handles strings, lists, booleans,
numbers, nested maps. Normalizes tags/aliases comma-separated
strings to QStringList. Returns empty list on invalid YAML."
```
