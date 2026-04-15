// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorViewManager.h"
#include "EditorViewSpace.h"
#include "NoteEditorWidget.h"
#include <markoff/Editor.h>
#include "corbomite/core/PaneLayoutBridge.h"
#include "corbomite/core/RegexRenderEngine.h"
#include "corbomite/core/RenderProfile.h"
#include "corbomite/core/NoteDocument.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QTabBar>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>

namespace Corbomite {

EditorViewManager::EditorViewManager(QWidget *parent)
    : QWidget(parent)
    , m_rootSplitter(new QSplitter(Qt::Horizontal, this))
    , m_canvasEngine(std::make_unique<RegexRenderEngine>())
{
    m_canvasEngine->setProfile(RenderProfile::canvasCard());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_rootSplitter);

    // Create the initial view space
    auto *initialSpace = createViewSpace();
    m_rootSplitter->addWidget(initialSpace);
    setActiveViewSpace(initialSpace);
}

EditorViewManager::~EditorViewManager() = default;

EditorViewSpace *EditorViewManager::createViewSpace()
{
    auto *space = new EditorViewSpace(this);
    space->setCanvasEngine(m_canvasEngine.get());
    space->setHoverPopover(m_hoverPopover);
    connectViewSpace(space);
    m_viewSpaces.append(space);
    return space;
}

void EditorViewManager::setHoverPopover(HoverPopover *popover)
{
    m_hoverPopover = popover;
    for (auto *space : std::as_const(m_viewSpaces)) {
        space->setHoverPopover(popover);
    }
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
    connect(space, &EditorViewSpace::splitRightRequested,
            this, &EditorViewManager::splitActiveHorizontal);
    connect(space, &EditorViewSpace::splitDownRequested,
            this, &EditorViewManager::splitActiveVertical);
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

void EditorViewManager::setViewMode(NoteEditorWidget::ViewMode mode)
{
    if (m_activeViewSpace) m_activeViewSpace->setViewMode(mode);
}

NoteEditorWidget::ViewMode EditorViewManager::viewMode() const
{
    return m_activeViewSpace ? m_activeViewSpace->viewMode() : NoteEditorWidget::ViewMode::Editing;
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

bool EditorViewManager::queryClose()
{
    QStringList modifiedPaths;
    for (auto *space : std::as_const(m_viewSpaces)) {
        modifiedPaths.append(space->modifiedDocumentPaths());
    }
    modifiedPaths.removeDuplicates();

    if (modifiedPaths.isEmpty()) {
        return true;
    }

    QStringList names;
    for (const QString &path : std::as_const(modifiedPaths)) {
        QString name = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        names.append(name);
    }

    QString message;
    if (names.size() == 1) {
        message = i18n("The document \"%1\" has unsaved changes.\n\nDo you want to save before closing?", names.first());
    } else {
        message = i18n("The following documents have unsaved changes:\n\n%1\n\nDo you want to save before closing?",
                        names.join(QStringLiteral("\n")));
    }

    auto result = KMessageBox::warningTwoActionsCancel(
        this,
        message,
        i18n("Unsaved Changes"),
        KStandardGuiItem::save(),
        KStandardGuiItem::discard()
    );

    if (result == KMessageBox::Cancel) {
        return false;
    }

    if (result == KMessageBox::PrimaryAction) {
        for (auto *space : std::as_const(m_viewSpaces)) {
            space->saveAllModified();
        }
    }

    return true;
}

void EditorViewManager::closeAllDocuments()
{
    for (auto *space : std::as_const(m_viewSpaces)) {
        space->closeAllTabs();
    }

    resetToSingleViewSpace();
}

void EditorViewManager::resetToSingleViewSpace()
{
    if (m_viewSpaces.size() <= 1) return;

    auto *keepSpace = m_viewSpaces.first();

    // Remove all other view spaces
    for (int i = m_viewSpaces.size() - 1; i >= 1; --i) {
        auto *space = m_viewSpaces.at(i);
        m_viewSpaces.removeAt(i);
        space->deleteLater();
    }

    // Remove all intermediate splitters — reparent keepSpace directly under root
    QList<QSplitter *> childSplitters;
    for (int i = m_rootSplitter->count() - 1; i >= 0; --i) {
        auto *child = qobject_cast<QSplitter *>(m_rootSplitter->widget(i));
        if (child) {
            childSplitters.append(child);
        }
    }

    m_rootSplitter->addWidget(keepSpace);

    for (auto *s : std::as_const(childSplitters)) {
        s->deleteLater();
    }

    setActiveViewSpace(keepSpace);
    Q_EMIT activeEditorChanged(keepSpace->activeEditor());
}

QVector<EditorViewSpace *> EditorViewManager::viewSpaces() const
{
    return m_viewSpaces;
}

QSplitter *EditorViewManager::rootSplitter() const
{
    return m_rootSplitter;
}

namespace {

/// Build a list of PaneLeaves representing the tabs in a single
/// EditorViewSpace. Captures file path, view type (note/canvas/graph),
/// mode (reading/source), and cursor/scroll state for the *active* tab
/// in the space (we only re-open the active document fully hydrated;
/// other tabs' cursor state is best-effort and captured on focus).
QList<PaneLeaf> leavesForSpace(EditorViewSpace *space, bool isActiveSpace)
{
    QList<PaneLeaf> leaves;
    auto *tabBar = space->findChild<QTabBar *>();
    if (!tabBar) return leaves;

    for (int i = 0; i < tabBar->count(); ++i) {
        const QString path = tabBar->tabData(i).toString();
        PaneLeaf leaf;
        leaf.filePath = path;

        QString viewType;
        if (path == QStringLiteral("__graph__")) {
            viewType = QStringLiteral("graph");
        } else if (path.endsWith(QStringLiteral(".canvas"))) {
            viewType = QStringLiteral("canvas");
        } else {
            viewType = QStringLiteral("markdown");
        }
        leaf.viewType = viewType;

        QJsonObject viewState;
        viewState.insert(QStringLiteral("type"), viewType);
        QJsonObject inner;
        if (!path.isEmpty()) inner.insert(QStringLiteral("file"), path);

        // Only capture cursor/mode for the active tab of the active space —
        // matches the old behaviour and avoids polling every hidden editor.
        if (tabBar->currentIndex() == i) {
            auto *editor = space->activeEditor();
            if (editor && editor->noteDocument()
                    && editor->noteDocument()->relativePath() == path) {
                inner.insert(QStringLiteral("cursorLine"), editor->currentLine());
                inner.insert(QStringLiteral("cursorColumn"), editor->currentColumn());
                if (editor->editor()->verticalScrollBar()) {
                    inner.insert(QStringLiteral("scrollPosition"),
                                 editor->editor()->verticalScrollBar()->value());
                }
                const auto vm = editor->viewMode();
                const QString modeStr =
                    (vm == NoteEditorWidget::ViewMode::Reading)
                        ? QStringLiteral("preview")
                        : QStringLiteral("source");
                inner.insert(QStringLiteral("mode"), modeStr);
                leaf.mode = modeStr;
            }
            leaf.unknown.insert(QStringLiteral("_corbomiteActive"), true);
        }

        viewState.insert(QStringLiteral("state"), inner);
        leaf.viewState = viewState;
        leaves.append(leaf);
    }

    // For the *active* space, ensure the currently-focused tab is first
    // so PaneLayoutBridge picks it up as the activeLeafId.
    if (isActiveSpace && tabBar->currentIndex() > 0
            && tabBar->currentIndex() < leaves.size()) {
        leaves.move(tabBar->currentIndex(), 0);
    }

    return leaves;
}

} // namespace

PaneLayout EditorViewManager::buildPaneLayout() const
{
    return PaneLayoutBridge::serializeFromSplitter(
        m_rootSplitter,
        [this](QWidget *w) -> QList<PaneLeaf> {
            auto *space = qobject_cast<EditorViewSpace *>(w);
            if (!space) return {};
            return leavesForSpace(space, space == m_activeViewSpace);
        },
        m_activeViewSpace);
}

void EditorViewManager::applyPaneLayout(
    const PaneLayout &layout,
    std::function<void(EditorViewSpace *space, const PaneLeaf &leaf)> openTab)
{
    // Tear down current state — drop all tabs + view spaces.
    closeAllDocuments();

    // After closeAllDocuments the manager holds exactly one empty view space
    // in the root splitter. We need to strip that baseline so the bridge can
    // populate the root cleanly.
    for (auto *space : std::as_const(m_viewSpaces)) {
        space->deleteLater();
    }
    m_viewSpaces.clear();
    m_activeViewSpace = nullptr;

    // Clear any remaining children of the root splitter (intermediate
    // QSplitter nodes from prior layouts).
    while (m_rootSplitter->count() > 0) {
        QWidget *w = m_rootSplitter->widget(0);
        w->setParent(nullptr);
        w->deleteLater();
    }

    QList<QWidget *> created = PaneLayoutBridge::deserializeIntoSplitter(
        layout,
        m_rootSplitter,
        [this]() -> QWidget * { return createViewSpace(); },
        [&openTab](QWidget *w, const PaneLeaf &leaf) {
            auto *space = qobject_cast<EditorViewSpace *>(w);
            if (!space) return;
            if (leaf.viewType == QStringLiteral("graph")) return; // skip
            openTab(space, leaf);
        });

    // Activate the first-created view space by default. A caller wanting
    // a specific tab/space active can re-activate after openTab runs.
    if (!m_viewSpaces.isEmpty()) {
        setActiveViewSpace(m_viewSpaces.first());
        Q_EMIT activeEditorChanged(m_viewSpaces.first()->activeEditor());
    }

    // Restore the active-tab index and per-leaf state (cursor / mode) from
    // the layout's leaves.
    auto applyLeafState = [](EditorViewSpace *space, const PaneLeaf &leaf) {
        if (leaf.viewType != QStringLiteral("markdown")) return;
        if (leaf.mode == QStringLiteral("preview")) {
            space->setViewMode(NoteEditorWidget::ViewMode::Reading);
        } else if (leaf.mode == QStringLiteral("source")) {
            space->setViewMode(NoteEditorWidget::ViewMode::Editing);
        }
        auto *editor = space->activeEditor();
        if (!editor) return;
        const auto inner = leaf.viewState.value(QStringLiteral("state")).toObject();
        const int line = inner.value(QStringLiteral("cursorLine")).toInt(1);
        const int scroll = inner.value(QStringLiteral("scrollPosition")).toInt(0);
        editor->editor()->goToLine(line);
        if (editor->editor()->verticalScrollBar() && scroll > 0) {
            editor->editor()->verticalScrollBar()->setValue(scroll);
        }
    };

    // Walk the layout tree in the same order as the bridge materialised
    // widgets; pair each leaf-index with the next view-space in `created`.
    int spaceIdx = 0;
    layout.root()->walk([&](const PaneLayoutIndex *node) {
        if (node->isSplit()) return true;
        if (spaceIdx >= created.size()) return false;
        auto *space = qobject_cast<EditorViewSpace *>(created[spaceIdx++]);
        if (!space) return true;

        auto *tabBar = space->findChild<QTabBar *>();
        // Apply the active leaf's cursor/mode.
        if (node->viewCount() > 0 && tabBar) {
            const int tabIdx = (node->currentTab() < tabBar->count())
                ? node->currentTab() : 0;
            tabBar->setCurrentIndex(tabIdx);
            applyLeafState(space, *node->viewAt(tabIdx));
        }
        return true;
    });
}

} // namespace Corbomite
