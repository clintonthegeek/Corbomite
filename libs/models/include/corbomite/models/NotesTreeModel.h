// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractItemModel>
#include <QString>
#include <memory>
#include <vector>

namespace Corbomite {

class TAbstractFile;
class TFile;
class VaultProxy;

class NotesTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        IsDirectoryRole,
        ModifiedTimeRole,
        FileTypeRole
    };

    /// Builds a hierarchical tree of the vault's markdown + canvas files.
    /// Subscribes to `VaultProxy::created` / `modified` / `deletedFile` /
    /// `renamed` to rebuild reactively — the caller must have granted the
    /// proxy `vault.read` AND `vault.events` or signals will never fire
    /// and `getFiles()` will return empty.
    explicit NotesTreeModel(VaultProxy *vault, QObject *parent = nullptr);

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Lookup
    QModelIndex indexForPath(const QString &relativePath) const;

private:
    struct TreeNode {
        QString name;             // filename or directory name
        QString relativePath;     // full relative path for files, dir path for dirs
        bool isDirectory = false;
        TreeNode *parentNode = nullptr;
        std::vector<std::unique_ptr<TreeNode>> children;
    };

    void rebuild();
    TreeNode *nodeFromIndex(const QModelIndex &index) const;
    TreeNode *findOrCreateDir(const QString &dirPath);
    void sortChildren(TreeNode *node);

    void onCreated(TAbstractFile *f);
    void onDeleted(TAbstractFile *f);
    void onRenamed(TAbstractFile *f, const QString &oldPath);

    VaultProxy *m_vault;
    std::unique_ptr<TreeNode> m_root;
};

} // namespace Corbomite
