// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileExplorerPanel.h"
#include "corbomite/models/NotesTreeModel.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QKeyEvent>
#include <KLocalizedString>

namespace Corbomite {

FileExplorerPanel::FileExplorerPanel(QWidget *parent)
    : QWidget(parent)
    , m_treeView(new QTreeView(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_treeView);

    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setDragEnabled(false); // Enable later
    m_treeView->setAnimated(true);

    m_treeView->installEventFilter(this);

    connect(m_treeView, &QTreeView::doubleClicked, this, &FileExplorerPanel::onDoubleClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &FileExplorerPanel::showContextMenu);
}

void FileExplorerPanel::setModel(NotesTreeModel *model)
{
    m_treeView->setModel(model);
}

void FileExplorerPanel::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    bool isDir = index.data(NotesTreeModel::IsDirectoryRole).toBool();
    if (isDir) return; // Expand/collapse handled by tree

    QString path = index.data(NotesTreeModel::PathRole).toString();
    Q_EMIT noteActivated(path);
}

void FileExplorerPanel::showContextMenu(const QPoint &pos)
{
    auto index = m_treeView->indexAt(pos);
    QMenu menu(this);

    if (index.isValid()) {
        bool isDir = index.data(NotesTreeModel::IsDirectoryRole).toBool();
        QString path = index.data(NotesTreeModel::PathRole).toString();

        if (isDir) {
            auto *newNote = menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")),
                                           i18n("New Note Here"));
            connect(newNote, &QAction::triggered, this, [this, path]() {
                Q_EMIT newNoteRequested(path);
            });
        } else {
            auto *open = menu.addAction(i18n("Open"));
            connect(open, &QAction::triggered, this, [this, path]() {
                Q_EMIT noteActivated(path);
            });
            menu.addSeparator();
            auto *rename = menu.addAction(i18n("Rename"));
            connect(rename, &QAction::triggered, this, [this, path]() {
                Q_EMIT renameNoteRequested(path);
            });
            auto *del = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                                       i18n("Delete"));
            connect(del, &QAction::triggered, this, [this, path]() {
                Q_EMIT deleteNoteRequested(path);
            });
        }
    } else {
        auto *newNote = menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")),
                                       i18n("New Note"));
        connect(newNote, &QAction::triggered, this, [this]() {
            Q_EMIT newNoteRequested(QString());
        });
    }

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

bool FileExplorerPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_treeView && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_F2) {
            auto index = m_treeView->currentIndex();
            if (index.isValid() && !index.data(NotesTreeModel::IsDirectoryRole).toBool()) {
                QString path = index.data(NotesTreeModel::PathRole).toString();
                Q_EMIT renameNoteRequested(path);
                return true;
            }
        }
        if (keyEvent->key() == Qt::Key_Delete) {
            auto index = m_treeView->currentIndex();
            if (index.isValid() && !index.data(NotesTreeModel::IsDirectoryRole).toBool()) {
                QString path = index.data(NotesTreeModel::PathRole).toString();
                Q_EMIT deleteNoteRequested(path);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace Corbomite
