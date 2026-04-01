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
