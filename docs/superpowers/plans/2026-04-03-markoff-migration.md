# Replace qmarkdowntextedit with Markoff — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace qmarkdowntextedit with Markoff::Editor via composition, preserving NoteEditorWidget's public API so all consumers remain unchanged.

**Architecture:** NoteEditorWidget becomes a QWidget owning a Markoff::Editor child. Completion triggers come from Markoff signals instead of key/mouse event overrides. Link resolution moves to a VaultResourceProvider implementing Markoff::ResourceProvider.

**Tech Stack:** C++20, Qt6, KDE Frameworks 6, Markoff (libs/markoff)

**Spec:** `docs/superpowers/specs/2026-04-03-markoff-migration-design.md`

---

## File Map

| File | Action | Purpose |
|---|---|---|
| `src/editor/VaultResourceProvider.h` | Create | ResourceProvider impl using VaultModel |
| `src/editor/VaultResourceProvider.cpp` | Create | Implementation |
| `src/editor/NoteEditorWidget.h` | Rewrite | QWidget with Markoff::Editor child |
| `src/editor/NoteEditorWidget.cpp` | Rewrite | Signal wiring, completion, NoteDocument sync |
| `src/CMakeLists.txt` | Modify | Swap link target |
| `CMakeLists.txt` | Modify | Remove qmarkdowntextedit subdirectory |
| `tests/editor/tst_obsidian_highlighting.cpp` | Delete | Replaced by Markoff's test suite |
| `tests/editor/CMakeLists.txt` | Modify | Remove test target |
| `.gitmodules` | Modify | Remove submodule entry |

---

### Task 1: Create VaultResourceProvider

**Files:**
- Create: `src/editor/VaultResourceProvider.h`
- Create: `src/editor/VaultResourceProvider.cpp`
- Modify: `src/CMakeLists.txt` (add new source file)

- [ ] **Step 1: Create header**

```cpp
// src/editor/VaultResourceProvider.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <markoff/ResourceProvider.h>

namespace Corbomite {

class VaultModel;

class VaultResourceProvider : public Markoff::ResourceProvider {
public:
    VaultResourceProvider(VaultModel *vault, const QString &noteRelativePath);

    QUrl resolveImage(const QString &name) const override;
    std::optional<QString> resolveEmbed(const QString &name) const override;
    QUrl resolveLink(const QString &target) const override;
    bool linkExists(const QString &target) const override;

private:
    QString resolveTarget(const QString &target) const;

    VaultModel *m_vault;
    QString m_vaultPath;
    QString m_noteDir; // directory containing the current note
};

} // namespace Corbomite
```

- [ ] **Step 2: Create implementation**

```cpp
// src/editor/VaultResourceProvider.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "VaultResourceProvider.h"
#include "corbomite/models/VaultModel.h"

#include <QDir>
#include <QFileInfo>

namespace Corbomite {

VaultResourceProvider::VaultResourceProvider(VaultModel *vault, const QString &noteRelativePath)
    : m_vault(vault)
    , m_vaultPath(vault ? vault->path() : QString())
{
    // Extract directory of current note for relative resolution
    int lastSlash = noteRelativePath.lastIndexOf(QLatin1Char('/'));
    m_noteDir = lastSlash > 0 ? noteRelativePath.left(lastSlash) : QString();
}

QString VaultResourceProvider::resolveTarget(const QString &target) const
{
    if (target.isEmpty()) return {};

    QString withExt = target;
    if (!withExt.endsWith(QStringLiteral(".md")) && !withExt.endsWith(QStringLiteral(".canvas"))) {
        withExt += QStringLiteral(".md");
    }

    // Try relative to current note first
    if (!m_noteDir.isEmpty()) {
        QString relative = m_noteDir + QLatin1Char('/') + withExt;
        if (m_vault && m_vault->noteExists(relative)) {
            return relative;
        }
    }

    // Try as-is (relative to vault root)
    if (m_vault && m_vault->noteExists(withExt)) {
        return withExt;
    }

    // Shortest-path match: search all notes for matching filename
    if (m_vault) {
        QString filename = withExt.mid(withExt.lastIndexOf(QLatin1Char('/')) + 1);
        const auto notes = m_vault->allNotes();
        for (const auto &meta : notes) {
            if (meta.relativePath.endsWith(QLatin1Char('/') + filename)
                || meta.relativePath == filename) {
                return meta.relativePath;
            }
        }
    }

    return withExt; // Return as-is even if not found
}

QUrl VaultResourceProvider::resolveImage(const QString &name) const
{
    if (m_vaultPath.isEmpty()) return {};

    // Try relative to current note
    if (!m_noteDir.isEmpty()) {
        QString path = m_vaultPath + QLatin1Char('/') + m_noteDir + QLatin1Char('/') + name;
        if (QFileInfo::exists(path)) {
            return QUrl::fromLocalFile(path);
        }
    }

    // Try relative to vault root
    QString path = m_vaultPath + QLatin1Char('/') + name;
    if (QFileInfo::exists(path)) {
        return QUrl::fromLocalFile(path);
    }

    return {};
}

std::optional<QString> VaultResourceProvider::resolveEmbed(const QString &name) const
{
    QString resolved = resolveTarget(name);
    if (m_vault) {
        if (auto *doc = m_vault->cachedDocument(resolved)) {
            return doc->markdown();
        }
        // Try reading the file directly
        QString absPath = m_vaultPath + QLatin1Char('/') + resolved;
        QFile file(absPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString::fromUtf8(file.readAll());
        }
    }
    return std::nullopt;
}

QUrl VaultResourceProvider::resolveLink(const QString &target) const
{
    QString resolved = resolveTarget(target);
    if (m_vaultPath.isEmpty()) return {};
    return QUrl::fromLocalFile(m_vaultPath + QLatin1Char('/') + resolved);
}

bool VaultResourceProvider::linkExists(const QString &target) const
{
    if (!m_vault) return false;

    QString withExt = target;
    if (!withExt.endsWith(QStringLiteral(".md")) && !withExt.endsWith(QStringLiteral(".canvas"))) {
        withExt += QStringLiteral(".md");
    }

    // Check all the same paths resolveTarget checks
    if (!m_noteDir.isEmpty()) {
        if (m_vault->noteExists(m_noteDir + QLatin1Char('/') + withExt))
            return true;
    }
    if (m_vault->noteExists(withExt))
        return true;

    // Shortest-path match
    QString filename = withExt.mid(withExt.lastIndexOf(QLatin1Char('/')) + 1);
    const auto notes = m_vault->allNotes();
    for (const auto &meta : notes) {
        if (meta.relativePath.endsWith(QLatin1Char('/') + filename)
            || meta.relativePath == filename) {
            return true;
        }
    }

    return false;
}

} // namespace Corbomite
```

- [ ] **Step 3: Add VaultResourceProvider.cpp to src/CMakeLists.txt**

In `src/CMakeLists.txt`, add `editor/VaultResourceProvider.cpp` to the CorbomiteApp source list, after `editor/CompletionDelegate.cpp`:

```cmake
    editor/CompletionDelegate.cpp
    editor/VaultResourceProvider.cpp
    editor/NotePreviewWidget.cpp
```

- [ ] **Step 4: Verify it compiles**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -20`
Expected: Compiles (VaultResourceProvider is not referenced yet, but should compile as a translation unit)

- [ ] **Step 5: Commit**

```bash
git add src/editor/VaultResourceProvider.h src/editor/VaultResourceProvider.cpp src/CMakeLists.txt
git commit -m "feat(editor): add VaultResourceProvider for Markoff integration"
```

---

### Task 2: Rewrite NoteEditorWidget header

**Files:**
- Modify: `src/editor/NoteEditorWidget.h`

- [ ] **Step 1: Replace NoteEditorWidget.h with composition-based version**

Replace the entire contents of `src/editor/NoteEditorWidget.h` with:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Markoff { class Editor; }

namespace Corbomite {

class NoteDocument;
class VaultModel;
class VaultResourceProvider;
class CompletionPopup;

class NoteEditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit NoteEditorWidget(QWidget *parent = nullptr);

    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;
    void setVaultModel(VaultModel *vault);

    Markoff::Editor *editor() const;

    int currentLine() const;
    int currentColumn() const;

Q_SIGNALS:
    void cursorInfoChanged(int line, int column, int wordCount);
    void linkActivated(const QString &targetPath);

private:
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onTextChanged();
    void onCursorPositionChanged(int line, int column);
    void syncFromDocument();

    // Completion
    void triggerWikiLinkCompletion();
    void triggerTagCompletion();
    void dismissCompletion();
    void onCompletionAccepted(const QString &text, const QString &data);

    // Link resolution
    QString resolveTarget(const QString &target) const;

    Markoff::Editor *m_editor = nullptr;
    NoteDocument *m_doc = nullptr;
    VaultModel *m_vault = nullptr;
    VaultResourceProvider *m_resourceProvider = nullptr;
    bool m_updatingFromDoc = false;
    int m_cachedWordCount = 0;

    // Completion state
    CompletionPopup *m_completionPopup = nullptr;
    int m_completionTriggerPos = -1;
    enum class CompletionMode { None, WikiLink, Tag };
    CompletionMode m_completionMode = CompletionMode::None;
};

} // namespace Corbomite
```

- [ ] **Step 2: Do not build yet — implementation follows in Task 3**

---

### Task 3: Rewrite NoteEditorWidget implementation

**Files:**
- Modify: `src/editor/NoteEditorWidget.cpp`

- [ ] **Step 1: Replace NoteEditorWidget.cpp with composition-based version**

Replace the entire contents of `src/editor/NoteEditorWidget.cpp` with:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionPopup.h"
#include "VaultResourceProvider.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/VaultModel.h"
#include "dialogs/QuickSwitcherModel.h"

#include <markoff/Editor.h>

#include <QKeyEvent>
#include <QVBoxLayout>
#include <QStringListModel>

namespace Corbomite {

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_editor(new Markoff::Editor(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_editor);

    // Content sync
    connect(m_editor, &Markoff::Editor::textChanged,
            this, &NoteEditorWidget::onTextChanged);

    // Cursor info
    connect(m_editor, &Markoff::Editor::cursorPositionChanged,
            this, &NoteEditorWidget::onCursorPositionChanged);

    // Word count — cache for cursorInfoChanged
    connect(m_editor, &Markoff::Editor::wordCountChanged,
            this, [this](int count) { m_cachedWordCount = count; });

    // Link clicks
    connect(m_editor, &Markoff::Editor::linkClicked,
            this, [this](const QString &target) {
        Q_EMIT linkActivated(resolveTarget(target));
    });

    // Completion triggers
    connect(m_editor, &Markoff::Editor::wikiLinkTrigger,
            this, [this](int pos) {
        m_completionTriggerPos = pos;
        triggerWikiLinkCompletion();
    });
    connect(m_editor, &Markoff::Editor::tagTrigger,
            this, [this](int pos) {
        m_completionTriggerPos = pos;
        triggerTagCompletion();
    });
    connect(m_editor, &Markoff::Editor::completionDismissHint,
            this, &NoteEditorWidget::dismissCompletion);

    // Install event filter for completion key interception
    m_editor->installEventFilter(this);
}

void NoteEditorWidget::setNoteDocument(NoteDocument *doc)
{
    m_doc = doc;
    if (m_doc) {
        // Set up resource provider for this note
        delete m_resourceProvider;
        m_resourceProvider = nullptr;
        if (m_vault) {
            m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
            m_editor->setResourceProvider(m_resourceProvider);
        }
        syncFromDocument();
    } else {
        m_editor->clear();
    }
}

NoteDocument *NoteEditorWidget::noteDocument() const
{
    return m_doc;
}

void NoteEditorWidget::setVaultModel(VaultModel *vault)
{
    m_vault = vault;
    // Update resource provider if we already have a document
    if (m_doc && m_vault) {
        delete m_resourceProvider;
        m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
        m_editor->setResourceProvider(m_resourceProvider);
    }
}

Markoff::Editor *NoteEditorWidget::editor() const
{
    return m_editor;
}

int NoteEditorWidget::currentLine() const
{
    return m_editor->cursorLine();
}

int NoteEditorWidget::currentColumn() const
{
    return m_editor->cursorColumn();
}

bool NoteEditorWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_editor && event->type() == QEvent::KeyPress && m_completionPopup) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Down:
            m_completionPopup->selectNext();
            return true;
        case Qt::Key_Up:
            m_completionPopup->selectPrevious();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            if (m_completionPopup->hasSelection()) {
                onCompletionAccepted(m_completionPopup->selectedText(),
                                     m_completionPopup->selectedData());
                return true;
            }
            break;
        case Qt::Key_Escape:
            dismissCompletion();
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void NoteEditorWidget::onTextChanged()
{
    if (m_updatingFromDoc || !m_doc) return;
    m_doc->setMarkdown(m_editor->toPlainText());
}

void NoteEditorWidget::onCursorPositionChanged(int line, int column)
{
    if (!m_doc) return;
    Q_EMIT cursorInfoChanged(line, column, m_cachedWordCount);
}

void NoteEditorWidget::syncFromDocument()
{
    if (!m_doc) return;
    m_updatingFromDoc = true;
    m_editor->setPlainText(m_doc->markdown());
    m_doc->setModified(false);
    m_updatingFromDoc = false;
}

// --- Completion ---

void NoteEditorWidget::triggerWikiLinkCompletion()
{
    if (!m_vault) return;
    dismissCompletion();

    m_completionMode = CompletionMode::WikiLink;

    auto *model = new QuickSwitcherModel(this);
    model->setNotes(m_vault->allNotes());

    m_completionPopup = new CompletionPopup(model, this);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() { m_completionPopup = nullptr; m_completionMode = CompletionMode::None; });

    QRect cr = m_editor->cursorScreenRect();
    m_completionPopup->move(cr.bottomLeft() + QPoint(0, 2));
    m_completionPopup->show();
}

void NoteEditorWidget::triggerTagCompletion()
{
    if (!m_vault) return;
    dismissCompletion();

    m_completionMode = CompletionMode::Tag;

    auto tags = m_vault->allTags();
    auto *model = new QStringListModel(tags, this);

    m_completionPopup = new CompletionPopup(model, this);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() { m_completionPopup = nullptr; m_completionMode = CompletionMode::None; });

    QRect cr = m_editor->cursorScreenRect();
    m_completionPopup->move(cr.bottomLeft() + QPoint(0, 2));
    m_completionPopup->show();
}

void NoteEditorWidget::dismissCompletion()
{
    if (m_completionPopup) {
        m_completionPopup->close();
        m_completionPopup = nullptr;
    }
    m_completionMode = CompletionMode::None;
    m_completionTriggerPos = -1;
}

void NoteEditorWidget::onCompletionAccepted(const QString &text, const QString &data)
{
    Q_UNUSED(data)

    // Markoff handles the text insertion at the trigger position.
    // We just need to tell it what to insert.
    if (m_completionMode == CompletionMode::WikiLink) {
        // Insert the note name + closing brackets
        // The [[ is already in the document; Markoff's wikiLinkTrigger fired after it
        // We need to replace text from trigger pos to current cursor with: NoteName]]
        QString source = m_editor->toPlainText();
        int cursorPos = m_completionTriggerPos + (m_editor->cursorColumn() - 1);
        // Simpler approach: get full text, splice, and set
        // But this loses undo history. Better: use the editor's own text manipulation.
        // For now, reconstruct from source text.
        int triggerPos = m_completionTriggerPos;
        QString before = source.left(triggerPos);
        // Find current cursor position in source
        // cursorLine/cursorColumn are 1-based
        int line = m_editor->cursorLine();
        int col = m_editor->cursorColumn();
        int pos = 0;
        int currentLine = 1;
        for (int i = 0; i < source.size(); ++i) {
            if (currentLine == line) {
                pos = i + col - 1;
                break;
            }
            if (source[i] == QLatin1Char('\n')) {
                ++currentLine;
            }
        }
        QString after = source.mid(pos);
        m_updatingFromDoc = true;
        m_editor->setPlainText(before + text + QStringLiteral("]]") + after);
        m_updatingFromDoc = false;
        if (m_doc) {
            m_doc->setMarkdown(m_editor->toPlainText());
        }
    } else if (m_completionMode == CompletionMode::Tag) {
        QString source = m_editor->toPlainText();
        int triggerPos = m_completionTriggerPos;
        QString before = source.left(triggerPos);
        int line = m_editor->cursorLine();
        int col = m_editor->cursorColumn();
        int pos = 0;
        int currentLine = 1;
        for (int i = 0; i < source.size(); ++i) {
            if (currentLine == line) {
                pos = i + col - 1;
                break;
            }
            if (source[i] == QLatin1Char('\n')) {
                ++currentLine;
            }
        }
        QString after = source.mid(pos);
        m_updatingFromDoc = true;
        m_editor->setPlainText(before + text + after);
        m_updatingFromDoc = false;
        if (m_doc) {
            m_doc->setMarkdown(m_editor->toPlainText());
        }
    }

    dismissCompletion();
}

// --- Link Resolution ---

QString NoteEditorWidget::resolveTarget(const QString &target) const
{
    if (target.isEmpty()) return {};

    // If it already has an extension, use as-is
    if (target.endsWith(QStringLiteral(".md")) || target.endsWith(QStringLiteral(".canvas"))) {
        return target;
    }

    return target + QStringLiteral(".md");
}

} // namespace Corbomite
```

- [ ] **Step 2: Do not build yet — CMake changes follow in Task 4**

---

### Task 4: Update CMake and remove qmarkdowntextedit

**Files:**
- Modify: `CMakeLists.txt:39` — remove qmarkdowntextedit subdirectory
- Modify: `src/CMakeLists.txt:55` — swap link target
- Modify: `tests/editor/CMakeLists.txt` — remove test target
- Delete: `tests/editor/tst_obsidian_highlighting.cpp`

- [ ] **Step 1: Remove qmarkdowntextedit subdirectory from root CMakeLists.txt**

In `CMakeLists.txt`, remove line 39:

```cmake
add_subdirectory(libs/qmarkdowntextedit)
```

The `add_subdirectory(libs/markoff)` on line 43 is already present.

- [ ] **Step 2: Swap link target in src/CMakeLists.txt**

In `src/CMakeLists.txt`, replace `qmarkdowntextedit` with `Markoff::Markoff` in `target_link_libraries`:

```cmake
        Corbomite::Models
        Markoff::Markoff
        forcegraph
```

- [ ] **Step 3: Gut the editor test CMakeLists.txt**

Replace the entire contents of `tests/editor/CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_EditorTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Tests removed — highlighting coverage provided by libs/markoff/tests/
```

- [ ] **Step 4: Delete the old test file**

```bash
rm tests/editor/tst_obsidian_highlighting.cpp
```

- [ ] **Step 5: Build and verify**

Run: `cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -30`
Expected: Clean build with no qmarkdowntextedit references

- [ ] **Step 6: Run Markoff tests to confirm they still pass**

Run: `cd build && ctest -R tst_markoff --output-on-failure`
Expected: All 8 markoff tests pass

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/CMakeLists.txt src/editor/NoteEditorWidget.h src/editor/NoteEditorWidget.cpp tests/editor/CMakeLists.txt
git rm tests/editor/tst_obsidian_highlighting.cpp
git commit -m "feat(editor): replace qmarkdowntextedit with Markoff::Editor

Rewrite NoteEditorWidget from inheritance (QMarkdownTextEdit) to
composition (QWidget owning Markoff::Editor). Completion triggers,
link clicks, and cursor tracking now use Markoff signals instead of
key/mouse event overrides.

Add VaultResourceProvider implementing Markoff::ResourceProvider for
wiki link and image resolution against the vault.

Remove tst_obsidian_highlighting — coverage provided by Markoff's
own test suite (8 tests)."
```

---

### Task 5: Remove qmarkdowntextedit git submodule

**Files:**
- Modify: `.gitmodules` — remove entry
- Remove: `libs/qmarkdowntextedit` — cached submodule

- [ ] **Step 1: Remove the submodule**

```bash
git submodule deinit -f libs/qmarkdowntextedit
git rm -f libs/qmarkdowntextedit
rm -rf .git/modules/libs/qmarkdowntextedit
```

- [ ] **Step 2: Verify .gitmodules is now empty or the entry is gone**

Run: `cat .gitmodules`
Expected: File is empty or the qmarkdowntextedit entry is gone

- [ ] **Step 3: Commit**

```bash
git add .gitmodules
git commit -m "chore: remove qmarkdowntextedit git submodule"
```

---

### Task 6: Smoke test the application

- [ ] **Step 1: Launch the app and open a vault**

Run: `./build/Corbomite`

Open the starter vault, open a note with wiki links and tags. Verify:
- Text renders in the editor
- Cursor position shows in the status bar
- Typing `[[` triggers the wiki link completion popup
- Typing `#` mid-line triggers the tag completion popup
- Clicking a wiki link navigates to the target note

- [ ] **Step 2: Verify completion popup positioning**

With a note open, type `[[` and confirm the popup appears near the cursor, not at the top-left corner of the window.

- [ ] **Step 3: Verify no qmarkdowntextedit references remain**

Run: `grep -r 'qmarkdowntextedit\|QMarkdownTextEdit\|markdownhighlighter\.h' --include='*.cpp' --include='*.h' --include='CMakeLists.txt' src/ tests/`
Expected: No matches

- [ ] **Step 4: Run full test suite**

Run: `cd build && ctest --output-on-failure`
Expected: All tests pass
