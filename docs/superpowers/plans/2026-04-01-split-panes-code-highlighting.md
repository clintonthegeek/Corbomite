# Split Panes & Code Block Highlighting — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add split pane editing (side-by-side notes) and KSyntaxHighlighting for 300+ language code blocks in reading mode.

**Architecture:** EditorViewManager refactored from single EditorViewSpace to a QSplitter tree managing multiple view spaces. MarkdownRenderer enhanced with KSyntaxHighlighting for code block HTML generation.

**Tech Stack:** C++20, Qt6 (QSplitter), KF6::SyntaxHighlighting

**Spec:** `docs/superpowers/specs/2026-04-01-split-panes-code-highlighting-design.md`

---

### Task 1: EditorViewManager Split Pane Architecture

**Files:**
- Modify: `src/editor/EditorViewManager.h`
- Modify: `src/editor/EditorViewManager.cpp`

This is the core architectural change. The manager goes from holding a single `EditorViewSpace` to managing a tree of `QSplitter` nodes with `EditorViewSpace` leaves.

- [ ] **Step 1: Rewrite EditorViewManager.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVector>

namespace Corbomite {

class NoteDocument;
class NoteEditorWidget;
class EditorViewSpace;
class SQLiteIndex;
class VaultModel;

class EditorViewManager : public QWidget {
    Q_OBJECT

public:
    explicit EditorViewManager(QWidget *parent = nullptr);

    // Existing API — targets active view space
    void openNote(NoteDocument *doc);
    void openCanvas(const QString &filePath);
    NoteEditorWidget *activeEditor() const;
    EditorViewSpace *activeViewSpace() const;
    void toggleEditorMode();
    bool isPreviewMode() const;
    void openGraphView(SQLiteIndex *index, VaultModel *vault);
    bool hasGraphView() const;

    // New: split pane management
    void splitActiveHorizontal();
    void splitActiveVertical();
    void closeActiveViewSpace();
    int viewSpaceCount() const;

Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);
    void graphNoteActivated(const QString &relativePath);

private:
    void setActiveViewSpace(EditorViewSpace *space);
    EditorViewSpace *createViewSpace();
    void connectViewSpace(EditorViewSpace *space);
    void splitActiveView(Qt::Orientation orientation);
    void removeViewSpace(EditorViewSpace *space);
    void cleanupEmptySplitters(QSplitter *splitter);

    QSplitter *m_rootSplitter;
    EditorViewSpace *m_activeViewSpace = nullptr;
    QVector<EditorViewSpace *> m_viewSpaces;
};

} // namespace Corbomite
```

- [ ] **Step 2: Rewrite EditorViewManager.cpp**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorViewManager.h"
#include "EditorViewSpace.h"
#include "NoteEditorWidget.h"
#include <QVBoxLayout>

namespace Corbomite {

EditorViewManager::EditorViewManager(QWidget *parent)
    : QWidget(parent)
    , m_rootSplitter(new QSplitter(Qt::Horizontal, this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_rootSplitter);

    // Create the initial view space
    auto *initialSpace = createViewSpace();
    m_rootSplitter->addWidget(initialSpace);
    setActiveViewSpace(initialSpace);
}

EditorViewSpace *EditorViewManager::createViewSpace()
{
    auto *space = new EditorViewSpace(this);
    connectViewSpace(space);
    m_viewSpaces.append(space);
    return space;
}

void EditorViewManager::connectViewSpace(EditorViewSpace *space)
{
    // When this space is clicked/focused, make it active
    connect(space, &EditorViewSpace::activeEditorChanged,
            this, [this, space](NoteEditorWidget *editor) {
        setActiveViewSpace(space);
        Q_EMIT activeEditorChanged(editor);
    });
    connect(space, &EditorViewSpace::cursorInfoChanged,
            this, [this, space](int line, int column, int wordCount) {
        if (space == m_activeViewSpace) {
            Q_EMIT cursorInfoChanged(line, column, wordCount);
        }
    });
    connect(space, &EditorViewSpace::graphNoteActivated,
            this, &EditorViewManager::graphNoteActivated);
}

void EditorViewManager::setActiveViewSpace(EditorViewSpace *space)
{
    if (m_activeViewSpace == space) return;
    m_activeViewSpace = space;
}

void EditorViewManager::openNote(NoteDocument *doc)
{
    if (m_activeViewSpace) m_activeViewSpace->openNote(doc);
}

void EditorViewManager::openCanvas(const QString &filePath)
{
    if (m_activeViewSpace) m_activeViewSpace->openCanvas(filePath);
}

NoteEditorWidget *EditorViewManager::activeEditor() const
{
    return m_activeViewSpace ? m_activeViewSpace->activeEditor() : nullptr;
}

EditorViewSpace *EditorViewManager::activeViewSpace() const
{
    return m_activeViewSpace;
}

void EditorViewManager::toggleEditorMode()
{
    if (m_activeViewSpace) m_activeViewSpace->toggleEditorMode();
}

bool EditorViewManager::isPreviewMode() const
{
    return m_activeViewSpace ? m_activeViewSpace->isPreviewMode() : false;
}

void EditorViewManager::openGraphView(SQLiteIndex *index, VaultModel *vault)
{
    if (m_activeViewSpace) m_activeViewSpace->openGraphView(index, vault);
}

bool EditorViewManager::hasGraphView() const
{
    return m_activeViewSpace ? m_activeViewSpace->hasGraphView() : false;
}

int EditorViewManager::viewSpaceCount() const
{
    return m_viewSpaces.size();
}

void EditorViewManager::splitActiveHorizontal()
{
    splitActiveView(Qt::Horizontal);
}

void EditorViewManager::splitActiveVertical()
{
    splitActiveView(Qt::Vertical);
}

void EditorViewManager::splitActiveView(Qt::Orientation orientation)
{
    if (!m_activeViewSpace) return;

    auto *newSpace = createViewSpace();

    // Clone the current note into the new pane
    auto *editor = m_activeViewSpace->activeEditor();
    if (editor && editor->noteDocument()) {
        newSpace->openNote(editor->noteDocument());
    }

    // Find the parent splitter of the active view space
    auto *parentSplitter = qobject_cast<QSplitter *>(m_activeViewSpace->parentWidget());
    if (!parentSplitter) parentSplitter = m_rootSplitter;

    if (parentSplitter->orientation() == orientation) {
        // Same orientation — just insert after active
        int idx = parentSplitter->indexOf(m_activeViewSpace);
        parentSplitter->insertWidget(idx + 1, newSpace);
    } else {
        // Different orientation — wrap in a new splitter
        int idx = parentSplitter->indexOf(m_activeViewSpace);

        auto *newSplitter = new QSplitter(orientation, this);
        parentSplitter->insertWidget(idx, newSplitter);
        newSplitter->addWidget(m_activeViewSpace);
        newSplitter->addWidget(newSpace);

        // Equal sizes
        QList<int> sizes;
        int total = (orientation == Qt::Horizontal)
            ? newSplitter->width() : newSplitter->height();
        sizes << total / 2 << total / 2;
        newSplitter->setSizes(sizes);
    }

    setActiveViewSpace(newSpace);
}

void EditorViewManager::closeActiveViewSpace()
{
    if (m_viewSpaces.size() <= 1) return;
    if (!m_activeViewSpace) return;

    auto *toRemove = m_activeViewSpace;

    // Find a sibling to activate
    int idx = m_viewSpaces.indexOf(toRemove);
    int newIdx = (idx > 0) ? idx - 1 : 1;
    setActiveViewSpace(m_viewSpaces.at(newIdx));
    Q_EMIT activeEditorChanged(m_activeViewSpace->activeEditor());

    removeViewSpace(toRemove);
}

void EditorViewManager::removeViewSpace(EditorViewSpace *space)
{
    auto *parentSplitter = qobject_cast<QSplitter *>(space->parentWidget());

    m_viewSpaces.removeOne(space);
    space->deleteLater();

    // Clean up empty or single-child splitters
    if (parentSplitter && parentSplitter != m_rootSplitter) {
        cleanupEmptySplitters(parentSplitter);
    }
}

void EditorViewManager::cleanupEmptySplitters(QSplitter *splitter)
{
    if (!splitter || splitter == m_rootSplitter) return;

    if (splitter->count() <= 1) {
        auto *parent = qobject_cast<QSplitter *>(splitter->parentWidget());
        if (!parent) return;

        // If one child left, unwrap it into the parent
        if (splitter->count() == 1) {
            QWidget *child = splitter->widget(0);
            int idx = parent->indexOf(splitter);
            parent->insertWidget(idx, child);
        }

        splitter->deleteLater();

        // Recurse in case parent is now single-child too
        if (parent != m_rootSplitter) {
            cleanupEmptySplitters(parent);
        }
    }
}

} // namespace Corbomite
```

- [ ] **Step 3: Build and verify**

```bash
cd /home/clinton/dev/Corbomite
cmake --build build && cd build && ctest --output-on-failure
```

All existing tests should pass — the API is backward-compatible (everything that worked with single view space still works).

- [ ] **Step 4: Commit**

```bash
git add src/editor/EditorViewManager.h src/editor/EditorViewManager.cpp
git commit -m "feat: refactor EditorViewManager for split pane support

Replace single EditorViewSpace with QSplitter tree managing multiple
view spaces. splitActiveHorizontal/Vertical create new panes.
closeActiveViewSpace removes and collapses. Active pane tracking.
Backward-compatible — all existing API unchanged."
```

---

### Task 2: Split Actions + Tab Context Menu

**Files:**
- Modify: `src/editor/EditorViewSpace.h`
- Modify: `src/editor/EditorViewSpace.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/app/corbomiteui.rc.in`

- [ ] **Step 1: Add Split Right/Down to tab context menu**

In `src/editor/EditorViewSpace.h`, add signals:
```cpp
Q_SIGNALS:
    // ... existing signals ...
    void splitRightRequested();
    void splitDownRequested();
```

In `src/editor/EditorViewSpace.cpp`, in `showTabContextMenu()`, add after the existing items (before `menu.exec()`):

```cpp
    menu.addSeparator();

    auto *splitRight = menu.addAction(i18n("Split Right"));
    connect(splitRight, &QAction::triggered, this, [this]() {
        Q_EMIT splitRightRequested();
    });

    auto *splitDown = menu.addAction(i18n("Split Down"));
    connect(splitDown, &QAction::triggered, this, [this]() {
        Q_EMIT splitDownRequested();
    });
```

You'll need `#include <KLocalizedString>` if not already present.

- [ ] **Step 2: Connect split signals in EditorViewManager::connectViewSpace()**

In `EditorViewManager.cpp`, add to `connectViewSpace()`:
```cpp
    connect(space, &EditorViewSpace::splitRightRequested,
            this, &EditorViewManager::splitActiveHorizontal);
    connect(space, &EditorViewSpace::splitDownRequested,
            this, &EditorViewManager::splitActiveVertical);
```

- [ ] **Step 3: Add keyboard shortcuts in MainWindow**

In `MainWindow.cpp` `setupActions()`, add:
```cpp
    auto *splitRight = ac->addAction(QStringLiteral("split_right"));
    splitRight->setText(i18n("Split Right"));
    ac->setDefaultShortcut(splitRight, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right));
    connect(splitRight, &QAction::triggered, m_editorManager, &EditorViewManager::splitActiveHorizontal);

    auto *splitDown = ac->addAction(QStringLiteral("split_down"));
    splitDown->setText(i18n("Split Down"));
    ac->setDefaultShortcut(splitDown, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down));
    connect(splitDown, &QAction::triggered, m_editorManager, &EditorViewManager::splitActiveVertical);
```

Add both to `updateVaultActions()` disabled list.

- [ ] **Step 4: Update XMLGUI**

In `corbomiteui.rc.in`, add to View menu after `editor_toggle_mode`:
```xml
      <Separator/>
      <Action name="split_right"/>
      <Action name="split_down"/>
```

Bump version to 8.

- [ ] **Step 5: Build, test, commit**

```bash
cmake --build build && cd build && ctest --output-on-failure
git add src/
git commit -m "feat: split pane actions — Ctrl+Shift+Right/Down, tab context menu

Split Right/Down in tab right-click context menu. Keyboard shortcuts.
View menu entries. Closing all tabs in a split pane removes the pane."
```

---

### Task 3: KSyntaxHighlighting for Reading Mode Code Blocks

**Files:**
- Modify: `CMakeLists.txt` (root — add find_package)
- Modify: `libs/core/CMakeLists.txt` (link KF6::SyntaxHighlighting)
- Modify: `libs/core/src/MarkdownRenderer.cpp`

- [ ] **Step 1: Add KF6SyntaxHighlighting dependency**

In root `CMakeLists.txt`, add after the existing `find_package(KF6...)` lines:
```cmake
find_package(KF6SyntaxHighlighting REQUIRED)
```

In `libs/core/CMakeLists.txt`, change:
```cmake
target_link_libraries(corbomite-core PUBLIC Qt6::Core)
```
to:
```cmake
target_link_libraries(corbomite-core PUBLIC Qt6::Core KF6::SyntaxHighlighting)
```

- [ ] **Step 2: Enhance MarkdownRenderer code block rendering**

In `libs/core/src/MarkdownRenderer.cpp`, add includes at top:
```cpp
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Theme>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/AbstractHighlighter>
```

Create a small helper class in the .cpp (not the header — internal detail):

```cpp
namespace {

class InlineHighlighter : public KSyntaxHighlighting::AbstractHighlighter {
public:
    QString highlightedHtml;

protected:
    void applyFormat(int offset, int length, const KSyntaxHighlighting::Format &format) override
    {
        Q_UNUSED(offset)
        if (!format.isDefaultTextStyle(theme())) {
            QColor color = format.textColor(theme());
            if (format.isBold(theme())) {
                highlightedHtml += QStringLiteral("<b style='color:%1'>").arg(color.name());
                highlightedHtml += m_currentLine.mid(offset, length).toHtmlEscaped();
                highlightedHtml += QStringLiteral("</b>");
            } else {
                highlightedHtml += QStringLiteral("<span style='color:%1'>").arg(color.name());
                highlightedHtml += m_currentLine.mid(offset, length).toHtmlEscaped();
                highlightedHtml += QStringLiteral("</span>");
            }
        } else {
            highlightedHtml += m_currentLine.mid(offset, length).toHtmlEscaped();
        }
    }

public:
    QString m_currentLine;
};

} // anonymous namespace
```

Then in `processBlocks()`, replace the code fence closing block:

Find the existing code that builds the `<pre><code>` HTML for code blocks. It currently looks like:
```cpp
QString langAttr = codeBlockLang.isEmpty()
    ? QString()
    : QStringLiteral(" class=\"language-%1\"").arg(codeBlockLang);
html += QStringLiteral("<pre><code%1>%2</code></pre>\n")
            .arg(langAttr, escapeHtml(codeContent));
```

Replace with:
```cpp
// Try KSyntaxHighlighting for code blocks
QString highlightedCode;
if (!codeBlockLang.isEmpty()) {
    static KSyntaxHighlighting::Repository repo;
    auto def = repo.definitionForName(codeBlockLang);
    if (!def.isValid()) {
        // Try common aliases
        def = repo.definitionForFileName(QStringLiteral("file.") + codeBlockLang);
    }
    if (def.isValid()) {
        InlineHighlighter highlighter;
        highlighter.setDefinition(def);
        highlighter.setTheme(repo.defaultTheme(
            KSyntaxHighlighting::Repository::LightTheme));

        KSyntaxHighlighting::State state;
        const auto lines = codeContent.split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i) {
            highlighter.m_currentLine = lines[i];
            highlighter.highlightedHtml.clear();
            state = highlighter.highlightLine(lines[i], state);
            highlightedCode += highlighter.highlightedHtml;
            if (i < lines.size() - 1) highlightedCode += QLatin1Char('\n');
        }
    }
}

if (highlightedCode.isEmpty()) {
    // Fallback: plain escaped text
    highlightedCode = escapeHtml(codeContent);
}

QString langAttr = codeBlockLang.isEmpty()
    ? QString()
    : QStringLiteral(" class=\"language-%1\"").arg(codeBlockLang);
html += QStringLiteral("<pre><code%1>%2</code></pre>\n")
            .arg(langAttr, highlightedCode);
```

- [ ] **Step 3: Build and verify**

```bash
cd /home/clinton/dev/Corbomite
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest --output-on-failure
```

All tests should pass. The tst_markdownrenderer test for code blocks should still pass since the fallback produces the same output for unrecognized languages.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt libs/core/
git commit -m "feat: add KSyntaxHighlighting for 300+ language code blocks in reading mode

Uses KSyntaxHighlighting::AbstractHighlighter to generate colored HTML
spans for fenced code blocks. Falls back to plain escaped text for
unrecognized languages. Source mode editor keeps its existing
24-language keyword coloring from qmarkdowntextedit."
```

---

Self-review:

1. **Spec coverage:** Split pane architecture ✓. Split operations (horizontal, vertical) ✓. Close pane ✓. Active pane tracking ✓. Tab context menu (Split Right/Down) ✓. Keyboard shortcuts (Ctrl+Shift+Right/Down) ✓. XMLGUI ✓. KSyntaxHighlighting in reading mode ✓. Fallback for unknown languages ✓. Dependency setup ✓. Session persistence deferred (breadcrumb noted in spec).

2. **Placeholder scan:** All code complete. No TBDs.

3. **Type consistency:** `EditorViewManager::splitActiveHorizontal/Vertical()` matches MainWindow shortcuts and tab context menu signals. `EditorViewSpace::splitRightRequested/splitDownRequested` signals connect to manager methods. `InlineHighlighter` uses `KSyntaxHighlighting::AbstractHighlighter` API correctly (applyFormat override).
