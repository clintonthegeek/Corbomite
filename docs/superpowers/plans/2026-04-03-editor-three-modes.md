# Editor Three Modes (Source / LivePreview / Reading) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `ViewMode { Source, LivePreview, Reading }` to `NoteEditorWidget`, wire it through the editor stack, replace `NotePreviewWidget` with `Markoff::ReadingView`, and expose three explicit mode actions in MainWindow.

**Architecture:** `NoteEditorWidget` owns a `QStackedWidget` containing `Markoff::Editor` (Source/LivePreview) and `Markoff::ReadingView` (Reading). Mode is per-tab, fully encapsulated. `EditorViewSpace` drops all preview-tracking state; its `activeEditor()` always returns a valid `NoteEditorWidget*`. `EditorViewManager` and `MainWindow` delegate to the per-tab widget.

**Tech Stack:** C++20, Qt6, Markoff::Editor, Markoff::ReadingView

---

## File Map

| File | Action | What changes |
|---|---|---|
| `src/editor/NoteEditorWidget.h` | Modify | Add `ViewMode` enum, `setViewMode`, `viewMode`, `viewModeChanged` signal, `m_modeStack`, `m_readingView`, `m_viewMode` |
| `src/editor/NoteEditorWidget.cpp` | Modify | QStackedWidget construction, ReadingView link wiring, `setViewMode` impl, resource provider on ReadingView |
| `src/editor/EditorViewSpace.h` | Modify | Remove preview tracking fields/methods; add `setViewMode`/`viewMode`; include NoteEditorWidget.h instead of forward-declare |
| `src/editor/EditorViewSpace.cpp` | Modify | Remove `NotePreviewWidget` usage, simplify `onTabChanged`/`closeTab`/`openNote`, replace `toggleEditorMode`/`isPreviewMode` |
| `src/editor/EditorViewManager.h` | Modify | Remove `m_readingEngine`, `toggleEditorMode`, `isPreviewMode`; add `setViewMode`/`viewMode` |
| `src/editor/EditorViewManager.cpp` | Modify | Remove `m_readingEngine` init/pass, replace toggle/preview with viewMode, update session save/restore |
| `src/app/MainWindow.h` | Modify | Remove `toggleEditorMode()` slot |
| `src/app/MainWindow.cpp` | Modify | Replace one toggle action with three mode actions; remove NotePreviewWidget include; wire `viewModeChanged` to status bar |
| `src/editor/NotePreviewWidget.h` | Delete | Replaced by Markoff::ReadingView |
| `src/editor/NotePreviewWidget.cpp` | Delete | Replaced by Markoff::ReadingView |
| `src/CMakeLists.txt` | Modify | Remove NotePreviewWidget.cpp |

---

### Task 1: NoteEditorWidget — add ViewMode + QStackedWidget + ReadingView

This task is purely additive. The existing `toggleEditorMode` / `NotePreviewWidget` infrastructure in EditorViewSpace is untouched and still compiles.

**Files:**
- Modify: `src/editor/NoteEditorWidget.h`
- Modify: `src/editor/NoteEditorWidget.cpp`

- [ ] **Step 1: Replace NoteEditorWidget.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Markoff {
class Editor;
class ReadingView;
}

namespace Corbomite {

class NoteDocument;
class VaultModel;
class VaultResourceProvider;
class CompletionPopup;
class QStackedWidget;

class NoteEditorWidget : public QWidget {
    Q_OBJECT

public:
    enum class ViewMode { Source, LivePreview, Reading };
    Q_ENUM(ViewMode)

    explicit NoteEditorWidget(QWidget *parent = nullptr);

    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;
    void setVaultModel(VaultModel *vault);

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    Markoff::Editor *editor() const;

    int currentLine() const;
    int currentColumn() const;

Q_SIGNALS:
    void cursorInfoChanged(int line, int column, int wordCount);
    void linkActivated(const QString &targetPath);
    void viewModeChanged(ViewMode mode);

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

    QStackedWidget *m_modeStack = nullptr;
    Markoff::Editor *m_editor = nullptr;
    Markoff::ReadingView *m_readingView = nullptr;
    ViewMode m_viewMode = ViewMode::Source;

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

- [ ] **Step 2: Update NoteEditorWidget.cpp**

Replace the entire file:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionPopup.h"
#include "VaultResourceProvider.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/VaultModel.h"
#include "dialogs/QuickSwitcherModel.h"

#include <markoff/Editor.h>
#include <markoff/ReadingView.h>

#include <QKeyEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QStringListModel>

namespace Corbomite {

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_modeStack(new QStackedWidget(this))
    , m_editor(new Markoff::Editor(m_modeStack))
    , m_readingView(new Markoff::ReadingView(m_modeStack))
{
    m_modeStack->addWidget(m_editor);      // index 0: Source / LivePreview
    m_modeStack->addWidget(m_readingView); // index 1: Reading
    m_modeStack->setCurrentIndex(0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_modeStack);

    connect(m_editor, &Markoff::Editor::textChanged,
            this, &NoteEditorWidget::onTextChanged);
    connect(m_editor, &Markoff::Editor::cursorPositionChanged,
            this, &NoteEditorWidget::onCursorPositionChanged);
    connect(m_editor, &Markoff::Editor::wordCountChanged,
            this, [this](int count) { m_cachedWordCount = count; });
    connect(m_editor, &Markoff::Editor::linkClicked,
            this, [this](const QString &target) {
        Q_EMIT linkActivated(resolveTarget(target));
    });
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
    connect(m_readingView, &Markoff::ReadingView::linkClicked,
            this, [this](const QString &target) {
        Q_EMIT linkActivated(resolveTarget(target));
    });

    m_editor->installEventFilter(this);
}

void NoteEditorWidget::setNoteDocument(NoteDocument *doc)
{
    m_doc = doc;
    if (m_doc) {
        m_editor->setResourceProvider(nullptr);
        m_readingView->setResourceProvider(nullptr);
        delete m_resourceProvider;
        m_resourceProvider = nullptr;
        if (m_vault) {
            m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
            m_editor->setResourceProvider(m_resourceProvider);
            m_readingView->setResourceProvider(m_resourceProvider);
        }
        syncFromDocument();
        if (m_viewMode == ViewMode::Reading)
            m_readingView->setMarkdown(m_doc->markdown());
    } else {
        m_editor->clear();
        m_readingView->setMarkdown({});
    }
}

NoteDocument *NoteEditorWidget::noteDocument() const
{
    return m_doc;
}

void NoteEditorWidget::setVaultModel(VaultModel *vault)
{
    m_vault = vault;
    if (m_doc && m_vault) {
        m_editor->setResourceProvider(nullptr);
        m_readingView->setResourceProvider(nullptr);
        delete m_resourceProvider;
        m_resourceProvider = new VaultResourceProvider(m_vault, m_doc->relativePath());
        m_editor->setResourceProvider(m_resourceProvider);
        m_readingView->setResourceProvider(m_resourceProvider);
    }
}

void NoteEditorWidget::setViewMode(ViewMode mode)
{
    if (m_viewMode == mode) return;
    m_viewMode = mode;

    if (mode == ViewMode::Reading) {
        if (m_doc)
            m_readingView->setMarkdown(m_editor->toPlainText());
        m_modeStack->setCurrentWidget(m_readingView);
    } else {
        m_editor->setMode(mode == ViewMode::LivePreview
            ? Markoff::Editor::Mode::LivePreview
            : Markoff::Editor::Mode::Source);
        m_modeStack->setCurrentWidget(m_editor);
    }

    Q_EMIT viewModeChanged(mode);
}

NoteEditorWidget::ViewMode NoteEditorWidget::viewMode() const
{
    return m_viewMode;
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
            this, [this]() {
        m_completionPopup = nullptr;
        m_completionMode = CompletionMode::None;
    });

    QRect cr = m_editor->cursorScreenRect();
    m_completionPopup->move(cr.bottomLeft() + QPoint(0, 2));
    m_completionPopup->show();
}

void NoteEditorWidget::triggerTagCompletion()
{
    if (!m_vault) return;
    dismissCompletion();

    m_completionMode = CompletionMode::Tag;

    auto *model = new QStringListModel(m_vault->allTags(), this);

    m_completionPopup = new CompletionPopup(model, this);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() {
        m_completionPopup = nullptr;
        m_completionMode = CompletionMode::None;
    });

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

    QString source = m_editor->toPlainText();
    int triggerPos = m_completionTriggerPos;
    if (triggerPos < 0 || triggerPos > source.size()) {
        dismissCompletion();
        return;
    }

    int line = m_editor->cursorLine();
    int col = m_editor->cursorColumn();
    if (line < 1 || col < 1) {
        dismissCompletion();
        return;
    }
    int absPos = 0;
    int currentLine = 1;
    bool found = false;
    for (int i = 0; i < source.size(); ++i) {
        if (currentLine == line) {
            absPos = i + col - 1;
            found = true;
            break;
        }
        if (source[i] == QLatin1Char('\n'))
            ++currentLine;
    }
    if (!found) {
        dismissCompletion();
        return;
    }

    QString before = source.left(triggerPos);
    QString after = source.mid(absPos);
    QString insertion = (m_completionMode == CompletionMode::WikiLink)
        ? text + QStringLiteral("]]")
        : text;

    m_updatingFromDoc = true;
    m_editor->setPlainText(before + insertion + after);
    m_updatingFromDoc = false;
    if (m_doc)
        m_doc->setMarkdown(m_editor->toPlainText());

    dismissCompletion();
}

// --- Link Resolution ---

QString NoteEditorWidget::resolveTarget(const QString &target) const
{
    if (target.isEmpty()) return {};
    if (target.endsWith(QStringLiteral(".md")) || target.endsWith(QStringLiteral(".canvas")))
        return target;
    return target + QStringLiteral(".md");
}

} // namespace Corbomite
```

- [ ] **Step 3: Build to verify Task 1 compiles (build is still green — old preview infrastructure untouched)**

```bash
cmake --build build 2>&1 | tail -20
```
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/editor/NoteEditorWidget.h src/editor/NoteEditorWidget.cpp
git commit -m "feat(editor): add ViewMode (Source/LivePreview/Reading) to NoteEditorWidget

Add QStackedWidget containing Markoff::Editor and Markoff::ReadingView.
setViewMode() switches the stack and calls Editor::setMode() for
Source/LivePreview. ReadingView linkClicked wires to linkActivated.
Resource provider set on both widgets."
```

---

### Task 2: Rewire EditorViewSpace + EditorViewManager + MainWindow

All three files must be updated together since they form a call chain. Build must be clean at the end of this task.

**Files:**
- Modify: `src/editor/EditorViewSpace.h`
- Modify: `src/editor/EditorViewSpace.cpp`
- Modify: `src/editor/EditorViewManager.h`
- Modify: `src/editor/EditorViewManager.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`

- [ ] **Step 1: Replace EditorViewSpace.h**

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QHash>
#include "NoteEditorWidget.h"
#include "corbomite/models/TabModel.h"

class MarkdownRenderEngine;

namespace Corbomite {

class NoteDocument;
class GraphViewTab;
class CanvasViewTab;
class SQLiteIndex;
class VaultModel;

class EditorViewSpace : public QWidget {
    Q_OBJECT

public:
    explicit EditorViewSpace(QWidget *parent = nullptr);

    void setCanvasEngine(MarkdownRenderEngine *engine);
    void openNote(NoteDocument *doc);
    void closeTab(int index);
    NoteEditorWidget *activeEditor() const;
    TabModel *tabModel();

    void setViewMode(NoteEditorWidget::ViewMode mode);
    NoteEditorWidget::ViewMode viewMode() const;

    void openCanvas(const QString &filePath);
    void openGraphView(SQLiteIndex *index, VaultModel *vault);
    bool hasGraphView() const;

    void closeAllTabs();
    bool hasModifiedDocuments() const;
    QStringList modifiedDocumentPaths() const;
    void saveAllModified();

Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);
    void internalLinkClicked(const QString &targetPath);
    void graphNoteActivated(const QString &relativePath);
    void splitRightRequested();
    void splitDownRequested();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void showTabContextMenu(const QPoint &pos);

    QTabBar *m_tabBar;
    QStackedWidget *m_stack;
    TabModel m_tabModel;
    QHash<QString, NoteEditorWidget *> m_editors; // relativePath -> editor
    MarkdownRenderEngine *m_canvasEngine = nullptr;
};

} // namespace Corbomite
```

- [ ] **Step 2: Replace EditorViewSpace.cpp**

Replace the entire file:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorViewSpace.h"
#include "NoteEditorWidget.h"
#include "graph/GraphViewTab.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include <QVBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QMenu>
#include <KLocalizedString>

namespace Corbomite {

EditorViewSpace::EditorViewSpace(QWidget *parent)
    : QWidget(parent)
    , m_tabBar(new QTabBar(this))
    , m_stack(new QStackedWidget(this))
{
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setDocumentMode(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addWidget(m_stack);

    m_tabBar->installEventFilter(this);
    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_tabBar, &QTabBar::currentChanged, this, &EditorViewSpace::onTabChanged);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &EditorViewSpace::onTabCloseRequested);
    connect(m_tabBar, &QTabBar::customContextMenuRequested, this, &EditorViewSpace::showTabContextMenu);
}

void EditorViewSpace::setCanvasEngine(MarkdownRenderEngine *engine)
{
    m_canvasEngine = engine;
}

void EditorViewSpace::openNote(NoteDocument *doc)
{
    if (!doc) return;

    const QString &path = doc->relativePath();

    if (m_editors.contains(path)) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_tabBar->tabData(i).toString() == path) {
                m_tabBar->setCurrentIndex(i);
                return;
            }
        }
    }

    auto *editor = new NoteEditorWidget(m_stack);
    editor->setNoteDocument(doc);
    m_editors.insert(path, editor);

    int stackIdx = m_stack->addWidget(editor);
    Q_UNUSED(stackIdx);
    int tabIdx = m_tabBar->addTab(doc->name());
    m_tabBar->setTabData(tabIdx, path);
    m_tabBar->setCurrentIndex(tabIdx);
    m_tabModel.openTab(path);

    connect(doc, &NoteDocument::modificationChanged, this, [this, path](bool modified) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_tabBar->tabData(i).toString() == path) {
                QString title = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
                int dot = title.lastIndexOf(QLatin1Char('.'));
                if (dot > 0) title = title.left(dot);
                if (modified) title += QStringLiteral(" \u2022");
                m_tabBar->setTabText(i, title);
                break;
            }
        }
    });

    connect(editor, &NoteEditorWidget::cursorInfoChanged,
            this, &EditorViewSpace::cursorInfoChanged);
}

void EditorViewSpace::setViewMode(NoteEditorWidget::ViewMode mode)
{
    if (auto *editor = activeEditor())
        editor->setViewMode(mode);
}

NoteEditorWidget::ViewMode EditorViewSpace::viewMode() const
{
    if (auto *editor = activeEditor())
        return editor->viewMode();
    return NoteEditorWidget::ViewMode::Source;
}

void EditorViewSpace::closeTab(int index)
{
    if (index < 0 || index >= m_tabBar->count()) return;

    QString path = m_tabBar->tabData(index).toString();
    m_tabBar->removeTab(index);

    if (path == QStringLiteral("__graph__")) {
        for (int i = 0; i < m_stack->count(); ++i) {
            if (auto *graph = qobject_cast<GraphViewTab *>(m_stack->widget(i))) {
                m_stack->removeWidget(graph);
                graph->deleteLater();
                break;
            }
        }
        return;
    }

    if (path.endsWith(QStringLiteral(".canvas"))) {
        for (int i = 0; i < m_stack->count(); ++i) {
            if (auto *canvas = qobject_cast<CanvasViewTab *>(m_stack->widget(i))) {
                if (canvas->filePath() == path) {
                    m_stack->removeWidget(canvas);
                    canvas->deleteLater();
                    break;
                }
            }
        }
        return;
    }

    if (auto *editor = m_editors.take(path)) {
        m_stack->removeWidget(editor);
        editor->deleteLater();
    }

    for (int i = 0; i < m_tabModel.rowCount(); ++i) {
        if (m_tabModel.tabPath(i) == path) {
            m_tabModel.closeTab(i);
            break;
        }
    }
}

NoteEditorWidget *EditorViewSpace::activeEditor() const
{
    return qobject_cast<NoteEditorWidget *>(m_stack->currentWidget());
}

TabModel *EditorViewSpace::tabModel()
{
    return &m_tabModel;
}

void EditorViewSpace::onTabChanged(int index)
{
    if (index < 0 || index >= m_tabBar->count()) {
        Q_EMIT activeEditorChanged(nullptr);
        return;
    }

    QString path = m_tabBar->tabData(index).toString();

    if (path == QStringLiteral("__graph__")) {
        for (int i = 0; i < m_stack->count(); ++i) {
            if (auto *graph = qobject_cast<GraphViewTab *>(m_stack->widget(i))) {
                m_stack->setCurrentWidget(graph);
                break;
            }
        }
        Q_EMIT activeEditorChanged(nullptr);
        return;
    }

    if (path.endsWith(QStringLiteral(".canvas"))) {
        for (int i = 0; i < m_stack->count(); ++i) {
            if (auto *canvas = qobject_cast<CanvasViewTab *>(m_stack->widget(i))) {
                if (canvas->filePath() == path) {
                    m_stack->setCurrentWidget(canvas);
                    break;
                }
            }
        }
        Q_EMIT activeEditorChanged(nullptr);
        return;
    }

    if (auto *editor = m_editors.value(path)) {
        m_stack->setCurrentWidget(editor);
        m_tabModel.setActiveTab(index);
        Q_EMIT activeEditorChanged(editor);
    }
}

void EditorViewSpace::onTabCloseRequested(int index)
{
    closeTab(index);
}

void EditorViewSpace::openCanvas(const QString &filePath)
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == filePath) {
            m_tabBar->setCurrentIndex(i);
            return;
        }
    }

    auto *canvasTab = new CanvasViewTab(filePath, m_stack);
    if (m_canvasEngine) {
        canvasTab->setRenderEngine(m_canvasEngine);
    }
    m_stack->addWidget(canvasTab);

    QString name = filePath.mid(filePath.lastIndexOf(QLatin1Char('/')) + 1);
    if (name.endsWith(QStringLiteral(".canvas"))) name.chop(7);

    int tabIdx = m_tabBar->addTab(QIcon::fromTheme(QStringLiteral("draw-rectangle")), name);
    m_tabBar->setTabData(tabIdx, filePath);
    m_tabBar->setCurrentIndex(tabIdx);

    connect(canvasTab, &CanvasViewTab::modificationChanged, this, [this, filePath](bool modified) {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_tabBar->tabData(i).toString() == filePath) {
                QString title = filePath.mid(filePath.lastIndexOf(QLatin1Char('/')) + 1);
                if (title.endsWith(QStringLiteral(".canvas"))) title.chop(7);
                if (modified) title += QStringLiteral(" \u2022");
                m_tabBar->setTabText(i, title);
                break;
            }
        }
    });
}

void EditorViewSpace::openGraphView(SQLiteIndex *index, VaultModel *vault)
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == QStringLiteral("__graph__")) {
            m_tabBar->setCurrentIndex(i);
            return;
        }
    }

    auto *graphTab = new GraphViewTab(index, vault, m_stack);
    m_stack->addWidget(graphTab);
    int tabIdx = m_tabBar->addTab(QIcon::fromTheme(QStringLiteral("preferences-system-network")),
                                   QStringLiteral("Graph View"));
    m_tabBar->setTabData(tabIdx, QStringLiteral("__graph__"));
    m_tabBar->setCurrentIndex(tabIdx);

    connect(graphTab, &GraphViewTab::noteActivated,
            this, &EditorViewSpace::graphNoteActivated);
    connect(graphTab, &GraphViewTab::openNoteInNewTabRequested,
            this, [this](const QString &path) {
        Q_EMIT graphNoteActivated(path);
    });
    connect(graphTab, &GraphViewTab::revealInNavigationRequested,
            this, [this](const QString &path) {
        Q_EMIT graphNoteActivated(path);
    });
    connect(graphTab, &GraphViewTab::deleteNoteRequested,
            this, [this](const QString &path) {
        Q_UNUSED(path);
    });
}

bool EditorViewSpace::hasGraphView() const
{
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i).toString() == QStringLiteral("__graph__"))
            return true;
    }
    return false;
}

bool EditorViewSpace::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_tabBar && event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            int tabIndex = m_tabBar->tabAt(mouseEvent->pos());
            if (tabIndex >= 0) {
                closeTab(tabIndex);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void EditorViewSpace::showTabContextMenu(const QPoint &pos)
{
    int tabIndex = m_tabBar->tabAt(pos);
    if (tabIndex < 0) return;

    QMenu menu(this);

    auto *closeAction = menu.addAction(i18n("Close"));
    connect(closeAction, &QAction::triggered, this, [this, tabIndex]() {
        closeTab(tabIndex);
    });

    auto *closeOthers = menu.addAction(i18n("Close Others"));
    connect(closeOthers, &QAction::triggered, this, [this, tabIndex]() {
        for (int i = m_tabBar->count() - 1; i >= 0; --i) {
            if (i != tabIndex) closeTab(i);
        }
    });

    auto *closeAll = menu.addAction(i18n("Close All"));
    connect(closeAll, &QAction::triggered, this, [this]() {
        while (m_tabBar->count() > 0) closeTab(0);
    });

    menu.addSeparator();

    auto *splitRight = menu.addAction(i18n("Split Right"));
    connect(splitRight, &QAction::triggered, this, [this]() {
        Q_EMIT splitRightRequested();
    });

    auto *splitDown = menu.addAction(i18n("Split Down"));
    connect(splitDown, &QAction::triggered, this, [this]() {
        Q_EMIT splitDownRequested();
    });

    menu.exec(m_tabBar->mapToGlobal(pos));
}

void EditorViewSpace::closeAllTabs()
{
    while (m_tabBar->count() > 0) {
        closeTab(0);
    }
}

bool EditorViewSpace::hasModifiedDocuments() const
{
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        auto *editor = it.value();
        if (editor && editor->noteDocument() && editor->noteDocument()->isModified()) {
            return true;
        }
    }
    return false;
}

QStringList EditorViewSpace::modifiedDocumentPaths() const
{
    QStringList paths;
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        auto *editor = it.value();
        if (editor && editor->noteDocument() && editor->noteDocument()->isModified()) {
            paths.append(it.key());
        }
    }
    return paths;
}

void EditorViewSpace::saveAllModified()
{
    FileSystemAdapter fs;
    for (auto it = m_editors.constBegin(); it != m_editors.constEnd(); ++it) {
        auto *editor = it.value();
        if (editor && editor->noteDocument() && editor->noteDocument()->isModified()) {
            auto *doc = editor->noteDocument();
            if (fs.writeFile(doc->filePath(), doc->markdown())) {
                doc->setModified(false);
                Q_EMIT doc->saved();
            }
        }
    }
}

} // namespace Corbomite
```

- [ ] **Step 3: Update EditorViewManager.h**

Replace the member section and public API (keep all other methods unchanged). The key changes: remove `MarkdownRenderEngine` forward decl, remove `m_readingEngine`, replace `toggleEditorMode`/`isPreviewMode` with `setViewMode`/`viewMode`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVector>
#include <QJsonObject>
#include <memory>
#include <functional>
#include "NoteEditorWidget.h"

namespace Corbomite {

class MarkdownRenderEngine;
class NoteDocument;
class EditorViewSpace;
class SQLiteIndex;
class VaultModel;

class EditorViewManager : public QWidget {
    Q_OBJECT

public:
    explicit EditorViewManager(QWidget *parent = nullptr);
    ~EditorViewManager() override;

    void openNote(NoteDocument *doc);
    void openCanvas(const QString &filePath);
    NoteEditorWidget *activeEditor() const;
    EditorViewSpace *activeViewSpace() const;

    void setViewMode(NoteEditorWidget::ViewMode mode);
    NoteEditorWidget::ViewMode viewMode() const;

    void openGraphView(SQLiteIndex *index, VaultModel *vault);
    bool hasGraphView() const;

    void splitActiveHorizontal();
    void splitActiveVertical();
    void closeActiveViewSpace();
    int viewSpaceCount() const;

    bool queryClose();
    void closeAllDocuments();

    QJsonObject buildSessionState() const;
    void restoreFromSession(const QJsonObject &editorState,
                            std::function<void(const QString &path, EditorViewSpace *space)> openTabCallback);
    QVector<EditorViewSpace *> viewSpaces() const;
    QSplitter *rootSplitter() const;

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
    void resetToSingleViewSpace();
    void rebuildSplitLayout(const QJsonValue &node, QSplitter *parent);
    void restoreTabState(const QJsonObject &paneJson, EditorViewSpace *space);

    QSplitter *m_rootSplitter;
    EditorViewSpace *m_activeViewSpace = nullptr;
    QVector<EditorViewSpace *> m_viewSpaces;
    std::unique_ptr<MarkdownRenderEngine> m_canvasEngine;
};

} // namespace Corbomite
```

- [ ] **Step 4: Update EditorViewManager.cpp**

Make these targeted edits:

**a) Remove `m_readingEngine` from constructor initializer and body.** Find:
```cpp
EditorViewManager::EditorViewManager(QWidget *parent)
    : QWidget(parent)
    , m_rootSplitter(new QSplitter(Qt::Horizontal, this))
    , m_readingEngine(std::make_unique<RegexRenderEngine>())
    , m_canvasEngine(std::make_unique<RegexRenderEngine>())
{
    m_readingEngine->setProfile(RenderProfile::readingMode());
    m_canvasEngine->setProfile(RenderProfile::canvasCard());
```
Replace with:
```cpp
EditorViewManager::EditorViewManager(QWidget *parent)
    : QWidget(parent)
    , m_rootSplitter(new QSplitter(Qt::Horizontal, this))
    , m_canvasEngine(std::make_unique<RegexRenderEngine>())
{
    m_canvasEngine->setProfile(RenderProfile::canvasCard());
```

**b) Remove `space->setRenderEngine(m_readingEngine.get())` from `createViewSpace()`.** Find:
```cpp
    space->setRenderEngine(m_readingEngine.get());
    space->setCanvasEngine(m_canvasEngine.get());
```
Replace with:
```cpp
    space->setCanvasEngine(m_canvasEngine.get());
```

**c) Replace `toggleEditorMode()` and `isPreviewMode()` implementations.** Find:
```cpp
void EditorViewManager::toggleEditorMode()
{
    if (m_activeViewSpace) m_activeViewSpace->toggleEditorMode();
}

bool EditorViewManager::isPreviewMode() const
{
    return m_activeViewSpace ? m_activeViewSpace->isPreviewMode() : false;
}
```
Replace with:
```cpp
void EditorViewManager::setViewMode(NoteEditorWidget::ViewMode mode)
{
    if (m_activeViewSpace) m_activeViewSpace->setViewMode(mode);
}

NoteEditorWidget::ViewMode EditorViewManager::viewMode() const
{
    return m_activeViewSpace ? m_activeViewSpace->viewMode() : NoteEditorWidget::ViewMode::Source;
}
```

**d) Update session state save (around line 362).** Find:
```cpp
                    // Save reading mode state
                    if (space->isPreviewMode() && tabBar->currentIndex() == i) {
                        tabObj[QStringLiteral("readingMode")] = true;
                    }
```
Replace with:
```cpp
                    // Save view mode for the active tab
                    if (tabBar->currentIndex() == i) {
                        auto *ed = space->activeEditor();
                        if (ed) {
                            auto vm = ed->viewMode();
                            if (vm == NoteEditorWidget::ViewMode::LivePreview)
                                tabObj[QStringLiteral("viewMode")] = QStringLiteral("livePreview");
                            else if (vm == NoteEditorWidget::ViewMode::Reading)
                                tabObj[QStringLiteral("viewMode")] = QStringLiteral("reading");
                        }
                    }
```

**e) Update session state restore (around line 513).** Find:
```cpp
    // Restore reading mode
    if (tabObj[QStringLiteral("readingMode")].toBool()) {
        space->toggleEditorMode();
    }
```
Replace with:
```cpp
    // Restore view mode (backwards compatible with old readingMode key)
    QString savedMode = tabObj[QStringLiteral("viewMode")].toString();
    if (savedMode == QStringLiteral("livePreview")) {
        space->setViewMode(NoteEditorWidget::ViewMode::LivePreview);
    } else if (savedMode == QStringLiteral("reading")
               || tabObj[QStringLiteral("readingMode")].toBool()) {
        space->setViewMode(NoteEditorWidget::ViewMode::Reading);
    }
```

- [ ] **Step 5: Update MainWindow.h**

Remove `void toggleEditorMode();` from the private slots section. No other changes needed — the three mode actions use lambdas.

- [ ] **Step 6: Update MainWindow.cpp**

**a) Remove** `#include "editor/NotePreviewWidget.h"` (line 9).

**b) Replace the `editor_toggle_mode` action block** (around line 273). Find:
```cpp
    auto *toggleMode = ac->addAction(QStringLiteral("editor_toggle_mode"));
    toggleMode->setText(i18n("Toggle Reading Mode"));
    toggleMode->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
    ac->setDefaultShortcut(toggleMode, QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(toggleMode, &QAction::triggered, this, &MainWindow::toggleEditorMode);
```
Replace with:
```cpp
    auto *sourceMode = ac->addAction(QStringLiteral("view_source_mode"));
    sourceMode->setText(i18n("Source"));
    sourceMode->setIcon(QIcon::fromTheme(QStringLiteral("text-x-markdown")));
    connect(sourceMode, &QAction::triggered, this, [this]() {
        if (auto *editor = m_editorManager->activeEditor())
            editor->setViewMode(NoteEditorWidget::ViewMode::Source);
    });

    auto *livePreviewMode = ac->addAction(QStringLiteral("view_live_preview_mode"));
    livePreviewMode->setText(i18n("Live Preview"));
    livePreviewMode->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right")));
    connect(livePreviewMode, &QAction::triggered, this, [this]() {
        if (auto *editor = m_editorManager->activeEditor())
            editor->setViewMode(NoteEditorWidget::ViewMode::LivePreview);
    });

    auto *readingMode = ac->addAction(QStringLiteral("view_reading_mode"));
    readingMode->setText(i18n("Reading"));
    readingMode->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
    ac->setDefaultShortcut(readingMode, QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(readingMode, &QAction::triggered, this, [this]() {
        if (auto *editor = m_editorManager->activeEditor())
            editor->setViewMode(NoteEditorWidget::ViewMode::Reading);
    });
```

**c) Wire viewModeChanged to status bar.** In the existing `activeEditorChanged` lambda that sets vault model and link nav (around line 750), add at the end of the lambda:
```cpp
        // Update status bar when view mode changes
        connect(editor, &NoteEditorWidget::viewModeChanged,
                this, [this](NoteEditorWidget::ViewMode mode) {
            if (mode == NoteEditorWidget::ViewMode::Reading)
                m_cursorPosLabel->setText(i18n("Reading"));
            // For Source/LivePreview, next cursorInfoChanged restores cursor pos
        }, Qt::UniqueConnection);
        // Apply immediately if already in reading mode
        if (editor->viewMode() == NoteEditorWidget::ViewMode::Reading)
            m_cursorPosLabel->setText(i18n("Reading"));
```

**d) Remove the `toggleEditorMode()` slot implementation** (around line 1022). Delete:
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

- [ ] **Step 7: Build and verify**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -30
```
Expected: clean build, no references to `NotePreviewWidget`, `toggleEditorMode`, or `isPreviewMode`.

- [ ] **Step 8: Commit**

```bash
git add src/editor/EditorViewSpace.h src/editor/EditorViewSpace.cpp \
        src/editor/EditorViewManager.h src/editor/EditorViewManager.cpp \
        src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat(editor): wire three-mode ViewMode through editor stack

Replace EditorViewSpace preview tracking (m_previews, m_previewModePaths,
toggleEditorMode, isPreviewMode) with setViewMode/viewMode delegating
to NoteEditorWidget. activeEditor() always returns non-null for note tabs.

Remove m_readingEngine from EditorViewManager — reading now handled by
Markoff::ReadingView inside NoteEditorWidget.

Add three KActionCollection actions: view_source_mode, view_live_preview_mode,
view_reading_mode (Ctrl+E). Session save/restore updated to viewMode key."
```

---

### Task 3: Delete NotePreviewWidget + final check

**Files:**
- Delete: `src/editor/NotePreviewWidget.h`
- Delete: `src/editor/NotePreviewWidget.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Delete the files**

```bash
rm src/editor/NotePreviewWidget.h src/editor/NotePreviewWidget.cpp
```

- [ ] **Step 2: Remove from src/CMakeLists.txt**

In `src/CMakeLists.txt`, remove `editor/NotePreviewWidget.cpp` from the CorbomiteApp source list.

- [ ] **Step 3: Build clean**

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -20
```
Expected: clean build.

- [ ] **Step 4: Run tests**

```bash
cd build && ctest --output-on-failure 2>&1 | tail -20
```
Expected: same pass rate as before (the 3 pre-existing failures in tst_renderengine/tst_quadtree/benchmark are unrelated).

- [ ] **Step 5: Verify no NotePreviewWidget references remain**

```bash
grep -r 'NotePreviewWidget\|toggleEditorMode\|isPreviewMode\|m_previews\|m_previewModePaths\|m_readingEngine' \
    --include='*.cpp' --include='*.h' \
    src/
```
Expected: no matches.

- [ ] **Step 6: Commit**

```bash
git add src/CMakeLists.txt
git rm src/editor/NotePreviewWidget.h src/editor/NotePreviewWidget.cpp
git commit -m "chore(editor): delete NotePreviewWidget

Reading mode now uses Markoff::ReadingView inside NoteEditorWidget.
NotePreviewWidget (QTextBrowser + MarkdownRenderEngine) is fully replaced."
```
