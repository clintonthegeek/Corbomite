// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileExplorerPanel.h"
#include "corbomite/core/MenuEventEmitter.h"
#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/models/NotesTreeModel.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QKeyEvent>
#include <QSet>
#include <functional>
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

void FileExplorerPanel::setMenuEventEmitter(MenuEventEmitter *emitter)
{
    m_menuEvents = emitter;
}

QStringList FileExplorerPanel::expandedFolders() const
{
    QStringList result;
    auto *model = m_treeView->model();
    if (!model) return result;

    std::function<void(const QModelIndex &)> collectExpanded = [&](const QModelIndex &parent) {
        int rows = model->rowCount(parent);
        for (int i = 0; i < rows; ++i) {
            QModelIndex idx = model->index(i, 0, parent);
            if (m_treeView->isExpanded(idx)) {
                QString path = idx.data(NotesTreeModel::PathRole).toString();
                if (!path.isEmpty()) {
                    result.append(path);
                }
                collectExpanded(idx);
            }
        }
    };
    collectExpanded(QModelIndex());
    return result;
}

void FileExplorerPanel::restoreExpandedFolders(const QStringList &folders)
{
    auto *model = m_treeView->model();
    if (!model || folders.isEmpty()) return;

    QSet<QString> folderSet(folders.begin(), folders.end());

    std::function<void(const QModelIndex &)> restoreExpanded = [&](const QModelIndex &parent) {
        int rows = model->rowCount(parent);
        for (int i = 0; i < rows; ++i) {
            QModelIndex idx = model->index(i, 0, parent);
            QString path = idx.data(NotesTreeModel::PathRole).toString();
            if (folderSet.contains(path)) {
                m_treeView->setExpanded(idx, true);
            }
            if (model->rowCount(idx) > 0) {
                restoreExpanded(idx);
            }
        }
    };
    restoreExpanded(QModelIndex());
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
    MenuSectionHelper helper(&menu);
    QString contextPath;

    if (index.isValid()) {
        const bool isDir = index.data(NotesTreeModel::IsDirectoryRole).toBool();
        contextPath = index.data(NotesTreeModel::PathRole).toString();

        if (isDir) {
            auto *newNote = new QAction(QIcon::fromTheme(QStringLiteral("document-new")),
                                        i18n("New Note Here"), &menu);
            connect(newNote, &QAction::triggered, this, [this, p = contextPath]() {
                Q_EMIT newNoteRequested(p);
            });
            helper.addToSection(newNote, QStringLiteral("action-primary"));
        } else {
            auto *open = new QAction(i18n("Open"), &menu);
            connect(open, &QAction::triggered, this, [this, p = contextPath]() {
                Q_EMIT noteActivated(p);
            });
            helper.addToSection(open, QStringLiteral("open"));

            auto *rename = new QAction(i18n("Rename"), &menu);
            connect(rename, &QAction::triggered, this, [this, p = contextPath]() {
                Q_EMIT renameNoteRequested(p);
            });
            helper.addToSection(rename, QStringLiteral("action"));

            auto *del = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                                    i18n("Delete"), &menu);
            connect(del, &QAction::triggered, this, [this, p = contextPath]() {
                Q_EMIT deleteNoteRequested(p);
            });
            helper.addToSection(del, QStringLiteral("danger"));
        }
    } else {
        auto *newNote = new QAction(QIcon::fromTheme(QStringLiteral("document-new")),
                                    i18n("New Note"), &menu);
        connect(newNote, &QAction::triggered, this, [this]() {
            Q_EMIT newNoteRequested(QString());
        });
        helper.addToSection(newNote, QStringLiteral("action-primary"));
    }

    // Mid-construction emit per docs/obsidian-audit/domains/workspace.md §4.
    // Plugins (when they exist — Cluster N) push items into named sections
    // before we finalize.
    if (m_menuEvents && !contextPath.isEmpty()) {
        m_menuEvents->emitFileMenu(&menu, contextPath);
    }

    helper.finalize();
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
