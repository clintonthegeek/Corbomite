// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorViewManager.h"
#include "EditorViewSpace.h"
#include <QVBoxLayout>

namespace Corbomite {

EditorViewManager::EditorViewManager(QWidget *parent)
    : QWidget(parent)
    , m_viewSpace(new EditorViewSpace(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_viewSpace);

    connect(m_viewSpace, &EditorViewSpace::activeEditorChanged,
            this, &EditorViewManager::activeEditorChanged);
    connect(m_viewSpace, &EditorViewSpace::cursorInfoChanged,
            this, &EditorViewManager::cursorInfoChanged);
    connect(m_viewSpace, &EditorViewSpace::graphNoteActivated,
            this, &EditorViewManager::graphNoteActivated);
}

void EditorViewManager::openNote(NoteDocument *doc)
{
    m_viewSpace->openNote(doc);
}

void EditorViewManager::openCanvas(const QString &filePath)
{
    m_viewSpace->openCanvas(filePath);
}

NoteEditorWidget *EditorViewManager::activeEditor() const
{
    return m_viewSpace->activeEditor();
}

EditorViewSpace *EditorViewManager::activeViewSpace() const
{
    return m_viewSpace;
}

void EditorViewManager::toggleEditorMode()
{
    m_viewSpace->toggleEditorMode();
}

bool EditorViewManager::isPreviewMode() const
{
    return m_viewSpace->isPreviewMode();
}

void EditorViewManager::openGraphView(SQLiteIndex *index, VaultModel *vault)
{
    m_viewSpace->openGraphView(index, vault);
}

bool EditorViewManager::hasGraphView() const
{
    return m_viewSpace->hasGraphView();
}

} // namespace Corbomite
