// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractItemModel>
#include <QString>
#include <memory>
#include <vector>

namespace Corbomite {

class TAbstractFile;
class TFile;
class Vault;

class NotesTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        IsDirectoryRole,
        ModifiedTimeRole,
        FileTypeRole
    };

    explicit NotesTreeModel(Vault *vault, QObject *parent = nullptr);

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

    Vault *m_vault;
    std::unique_ptr<TreeNode> m_root;
};

} // namespace Corbomite
