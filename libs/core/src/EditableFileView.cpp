// libs/core/src/EditableFileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/EditableFileView.h"
#include "corbomite/core/NoteDocument.h"

#include <QLineEdit>
#include <QMenu>
#include <QKeyEvent>
#include <QIcon>
#include <KLocalizedString>

namespace Corbomite {

EditableFileView::EditableFileView(WorkspaceLeaf *leaf, QWidget *parent)
    : FileView(leaf, parent)
{
}

void EditableFileView::onOpen()
{
    FileView::onOpen();
}

void EditableFileView::onPaneMenu(QMenu *menu)
{
    FileView::onPaneMenu(menu);
    if (m_file) {
        menu->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                        i18n("Rename..."), this, &EditableFileView::startRename);
    }
}

void EditableFileView::startRename()
{
    if (!m_file || m_renaming) return;
    m_renaming = true;
    m_originalName = m_file->name();
    m_renaming = false;
}

void EditableFileView::finishRename()
{
    m_renaming = false;
}

void EditableFileView::cancelRename()
{
    m_renaming = false;
}

} // namespace Corbomite
