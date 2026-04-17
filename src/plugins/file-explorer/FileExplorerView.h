// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTreeView>
#include <QWidget>

namespace Corbomite {

class FileManagerProxy;
class NotesTreeModel;
class Vault;
class WorkspaceController;

class FileExplorerView : public QWidget
{
    Q_OBJECT
public:
    FileExplorerView(Vault *vault,
                     FileManagerProxy *fileManager,
                     WorkspaceController *workspace,
                     QWidget *parent = nullptr);
    ~FileExplorerView() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void onDoubleClicked(const QModelIndex &index);
    void showContextMenu(const QPoint &pos);
    void onNoteActivated(const QString &relativePath);
    void onNewNoteIn(const QString &folder);
    void onDeleteNote(const QString &relativePath);
    void onRenameNote(const QString &relativePath);

    Vault *m_vault = nullptr;
    FileManagerProxy *m_fmProxy = nullptr;
    WorkspaceController *m_workspace = nullptr;

    QTreeView *m_treeView;
    NotesTreeModel *m_model = nullptr;
};

} // namespace Corbomite
