// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileExplorerView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace Corbomite {

FileExplorerView::FileExplorerView(VaultProxy *vault,
                                    FileManagerProxy *fileManager,
                                    WorkspaceController *workspace,
                                    QWidget *parent)
    : QWidget(parent)
    , m_vault(vault)
    , m_fmProxy(fileManager)
    , m_workspace(workspace)
    , m_treeView(new QTreeView(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_treeView);

    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setAnimated(true);
    m_treeView->installEventFilter(this);

    if (m_vault) {
        m_model = new NotesTreeModel(m_vault, this);
        m_treeView->setModel(m_model);

        // Preserve folder expansion state across model resets — the model
        // does a full beginResetModel/endResetModel on every create/delete/
        // rename event (NotesTreeModel.cpp:222-243), which collapses every
        // folder back to its initial state. Snapshot before the reset, restore
        // after. Uses the existing expanded/setExpanded helpers above.
        connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this,
                [this]() { m_savedExpansion = expandedFolderPaths(); });
        connect(m_model, &QAbstractItemModel::modelReset, this,
                [this]() { setExpandedFolderPaths(m_savedExpansion); });
    }

    connect(m_treeView, &QTreeView::doubleClicked, this,
            &FileExplorerView::onDoubleClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this,
            &FileExplorerView::showContextMenu);
}

FileExplorerView::~FileExplorerView() = default;

QStringList FileExplorerView::expandedFolderPaths() const
{
    QStringList out;
    if (!m_model || !m_treeView) return out;

    // DFS over folder indices; recurse through expanded folders only — a
    // collapsed folder's children aren't part of the "expanded" set anyway.
    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
        const int rows = m_model->rowCount(parent);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex idx = m_model->index(row, 0, parent);
            if (!idx.isValid()) continue;
            if (!idx.data(NotesTreeModel::IsDirectoryRole).toBool()) continue;
            if (m_treeView->isExpanded(idx)) {
                out.append(idx.data(NotesTreeModel::PathRole).toString());
                walk(idx);
            }
        }
    };
    walk(QModelIndex());
    return out;
}

void FileExplorerView::setExpandedFolderPaths(const QStringList &paths)
{
    if (!m_model || !m_treeView) return;
    // Expand parent folders first so child expansions have an expanded
    // ancestor chain (QTreeView::setExpanded doesn't auto-expand parents).
    QStringList sorted = paths;
    std::sort(sorted.begin(), sorted.end(),
              [](const QString &a, const QString &b) {
                  return a.count(QLatin1Char('/')) < b.count(QLatin1Char('/'));
              });
    for (const QString &path : sorted) {
        const QModelIndex idx = m_model->indexForPath(path);
        if (idx.isValid()) m_treeView->setExpanded(idx, true);
    }
}

void FileExplorerView::revealPath(const QString &relativePath)
{
    if (!m_model || !m_treeView || relativePath.isEmpty()) return;

    const QModelIndex idx = m_model->indexForPath(relativePath);
    if (!idx.isValid()) return;

    // Walk up the chain and expand every ancestor so the target row is
    // actually visible. QTreeView::scrollTo only works on expanded rows.
    QModelIndex ancestor = idx.parent();
    while (ancestor.isValid()) {
        m_treeView->expand(ancestor);
        ancestor = ancestor.parent();
    }

    m_treeView->setCurrentIndex(idx);
    m_treeView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

void FileExplorerView::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    if (index.data(NotesTreeModel::IsDirectoryRole).toBool()) return;
    onNoteActivated(index.data(NotesTreeModel::PathRole).toString());
}

void FileExplorerView::onNoteActivated(const QString &path)
{
    if (m_workspace && !path.isEmpty()) m_workspace->openFile(path);
}

void FileExplorerView::onNewNoteIn(const QString &folder)
{
    if (!m_fmProxy || !m_vault) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, i18n("New Note"),
        i18n("Note name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty()) return;
    TFolder *parent = nullptr;
    if (!folder.isEmpty()) parent = m_vault->getFolderByPath(folder);
    auto *tf = m_fmProxy->createNewMarkdownFile(parent, name);
    if (tf) {
        onNoteActivated(tf->path);
    } else {
        // `Vault::create` returns null on case-insensitive collision (so
        // case-only-different names that would clobber each other on
        // case-folding filesystems are refused even on case-sensitive ext4).
        // Surface that to the user so the silent refusal isn't mysterious.
        QMessageBox::warning(this, i18n("Could not create note"),
            i18n("A note named \"%1\" already exists in this folder "
                 "(case-insensitive match). Pick a different name.", name));
    }
}

void FileExplorerView::onNewCanvasIn(const QString &folder)
{
    // M2.6 — "Create new canvas" (FileExplorer folder-context-menu half of
    // the command; see MainWindow::createNewCanvas() for the command-
    // palette half, which this mirrors). No name prompt: creates
    // "Untitled.canvas" (dedup-numbered on collision via the same
    // FileManager::createNewFile()/collisionFreeName() rule
    // createNewMarkdownFile() uses), opens it, then triggers a rename via
    // promptForFileRename() — the closest existing "start a rename"
    // primitive, since no true inline-rename-on-creation flow exists for
    // notes either (see MainWindow::createNewCanvas()'s comment).
    if (!m_fmProxy || !m_vault) return;
    TFolder *parent = nullptr;
    if (!folder.isEmpty()) parent = m_vault->getFolderByPath(folder);

    // Literal empty `.canvas` JSON contract shape (CanvasDocument::toJson()'s
    // defaults for a document with no nodes/edges) — this plugin doesn't
    // link against libs/canvas, so the shape is inlined rather than pulling
    // in a new dependency for one JSON literal.
    static const QByteArray kEmptyCanvasJson = QByteArrayLiteral("{\n    \"nodes\": [],\n    \"edges\": []\n}\n");

    auto *tf = m_fmProxy->createNewFile(parent, QString(), QStringLiteral("canvas"), kEmptyCanvasJson);
    if (!tf) {
        QMessageBox::warning(this, i18n("Could not create canvas"),
            i18n("A file named \"Untitled.canvas\" already exists in this folder "
                 "(case-insensitive match)."));
        return;
    }
    onNoteActivated(tf->path);
    m_fmProxy->promptForFileRename(tf, this);
}

void FileExplorerView::onDeleteNote(const QString &path)
{
    if (!m_fmProxy || !m_vault) return;
    auto *file = m_vault->getAbstractFileByPath(path);
    if (file) m_fmProxy->promptForDeletion(file, this);
}

void FileExplorerView::onRenameNote(const QString &path)
{
    if (!m_fmProxy || !m_vault) return;
    auto *file = m_vault->getAbstractFileByPath(path);
    if (file) m_fmProxy->promptForFileRename(file, this);
}

void FileExplorerView::showContextMenu(const QPoint &pos)
{
    auto index = m_treeView->indexAt(pos);
    QMenu menu(this);
    QString contextPath;

    if (index.isValid()) {
        const bool isDir = index.data(NotesTreeModel::IsDirectoryRole).toBool();
        contextPath = index.data(NotesTreeModel::PathRole).toString();
        if (isDir) {
            auto *newNote = menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")),
                                           i18n("New Note Here"));
            connect(newNote, &QAction::triggered, this,
                    [this, p = contextPath]() { onNewNoteIn(p); });
            auto *newCanvas = menu.addAction(QIcon::fromTheme(QStringLiteral("draw-rectangle")),
                                             i18n("New Canvas Here"));
            connect(newCanvas, &QAction::triggered, this,
                    [this, p = contextPath]() { onNewCanvasIn(p); });
            // Don't offer rename/delete on the root row.
            if (!contextPath.isEmpty() && contextPath != QStringLiteral("/")) {
                menu.addSeparator();
                auto *rename = menu.addAction(i18n("Rename"));
                connect(rename, &QAction::triggered, this,
                        [this, p = contextPath]() { onRenameNote(p); });
                auto *del = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                                           i18n("Delete"));
                connect(del, &QAction::triggered, this,
                        [this, p = contextPath]() { onDeleteNote(p); });
            }
        } else {
            auto *open = menu.addAction(i18n("Open"));
            connect(open, &QAction::triggered, this,
                    [this, p = contextPath]() { onNoteActivated(p); });
            auto *rename = menu.addAction(i18n("Rename"));
            connect(rename, &QAction::triggered, this,
                    [this, p = contextPath]() { onRenameNote(p); });
            auto *del = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                                       i18n("Delete"));
            connect(del, &QAction::triggered, this,
                    [this, p = contextPath]() { onDeleteNote(p); });
        }
    } else {
        auto *newNote = menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")),
                                       i18n("New Note"));
        connect(newNote, &QAction::triggered, this,
                [this]() { onNewNoteIn(QString()); });
        auto *newCanvas = menu.addAction(QIcon::fromTheme(QStringLiteral("draw-rectangle")),
                                         i18n("New Canvas"));
        connect(newCanvas, &QAction::triggered, this,
                [this]() { onNewCanvasIn(QString()); });
    }
    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

bool FileExplorerView::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_treeView && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const auto idx = m_treeView->currentIndex();
        if (idx.isValid()) {
            const QString path = idx.data(NotesTreeModel::PathRole).toString();
            // F2/Delete work on both files and folders. Suppress on the
            // virtual root row only.
            if (!path.isEmpty() && path != QStringLiteral("/")) {
                if (keyEvent->key() == Qt::Key_F2) { onRenameNote(path); return true; }
                if (keyEvent->key() == Qt::Key_Delete) { onDeleteNote(path); return true; }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace Corbomite
