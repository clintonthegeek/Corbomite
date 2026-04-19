// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTreeView>
#include <QWidget>

namespace Corbomite {

class FileManagerProxy;
class NotesTreeModel;
class VaultProxy;
class WorkspaceController;

class FileExplorerView : public QWidget
{
    Q_OBJECT
public:
    FileExplorerView(VaultProxy *vault,
                     FileManagerProxy *fileManager,
                     WorkspaceController *workspace,
                     QWidget *parent = nullptr);
    ~FileExplorerView() override;

    /// Walk the tree and return the relative paths of every currently-
    /// expanded folder index. Used by FileExplorerPlugin::saveSessionState
    /// so the expand state survives a vault close/reopen.
    QStringList expandedFolderPaths() const;

    /// Expand the rows for every path in `paths` that the current model
    /// can resolve. Missing paths are silently ignored (folder may have
    /// been deleted between sessions).
    void setExpandedFolderPaths(const QStringList &paths);

    /// Scroll to + select the row for `relativePath`, expanding every
    /// ancestor folder along the way. No-op if the path is not in the
    /// current tree. Consumed by the Cluster R "Reveal file in navigation"
    /// menu item via the `file-explorer:reveal-file` command.
    void revealPath(const QString &relativePath);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void onDoubleClicked(const QModelIndex &index);
    void showContextMenu(const QPoint &pos);
    void onNoteActivated(const QString &relativePath);
    void onNewNoteIn(const QString &folder);
    void onDeleteNote(const QString &relativePath);
    void onRenameNote(const QString &relativePath);

    VaultProxy *m_vault = nullptr;
    FileManagerProxy *m_fmProxy = nullptr;
    WorkspaceController *m_workspace = nullptr;

    QTreeView *m_treeView;
    NotesTreeModel *m_model = nullptr;
};

} // namespace Corbomite
