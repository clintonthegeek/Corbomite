// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/proxies/VaultProxy.h"
#include <QDateTime>
#include <algorithm>
#include <functional>

namespace Corbomite {

namespace {
bool isTreeFile(const TFile *tf)
{
    if (!tf) return false;
    return tf->extension == QLatin1String("md")
        || tf->extension == QLatin1String("canvas");
}
}  // namespace

NotesTreeModel::NotesTreeModel(VaultProxy *vault, QObject *parent)
    : QAbstractItemModel(parent)
    , m_vault(vault)
    , m_root(std::make_unique<TreeNode>())
{
    m_root->name = QStringLiteral("root");
    m_root->isDirectory = true;

    if (m_vault) {
        // Subscribe to the forwarded VaultProxy signals. Note: these only
        // fire when the owning plugin holds the `vault.events` permission;
        // without that grant the signals are defined but never emit.
        connect(m_vault, &VaultProxy::created, this, &NotesTreeModel::onCreated);
        connect(m_vault, &VaultProxy::deletedFile, this, &NotesTreeModel::onDeleted);
        connect(m_vault, &VaultProxy::renamed, this, &NotesTreeModel::onRenamed);
        rebuild();
    }
}

QModelIndex NotesTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0) return {};
    auto *parentNode = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    if (!parentNode || row < 0 || row >= static_cast<int>(parentNode->children.size())) return {};
    return createIndex(row, 0, parentNode->children[row].get());
}

QModelIndex NotesTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) return {};
    auto *node = nodeFromIndex(child);
    if (!node || !node->parentNode || node->parentNode == m_root.get()) return {};

    auto *grandparent = node->parentNode->parentNode;
    if (!grandparent) return {};

    for (size_t i = 0; i < grandparent->children.size(); ++i) {
        if (grandparent->children[i].get() == node->parentNode) {
            return createIndex(static_cast<int>(i), 0, node->parentNode);
        }
    }
    return {};
}

int NotesTreeModel::rowCount(const QModelIndex &parent) const
{
    auto *node = parent.isValid() ? nodeFromIndex(parent) : m_root.get();
    if (!node || !node->isDirectory) return 0;
    return static_cast<int>(node->children.size());
}

int NotesTreeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant NotesTreeModel::data(const QModelIndex &index, int role) const
{
    auto *node = nodeFromIndex(index);
    if (!node) return {};

    switch (role) {
    case Qt::DisplayRole:
        return node->name;
    case PathRole:
        return node->relativePath;
    case IsDirectoryRole:
        return node->isDirectory;
    case ModifiedTimeRole:
        if (!node->isDirectory && m_vault) {
            if (TFile *tf = m_vault->getFileByPath(node->relativePath)) {
                if (tf->stat) {
                    return QDateTime::fromMSecsSinceEpoch(tf->stat->mtimeMs);
                }
            }
        }
        return {};
    case FileTypeRole:
        if (node->isDirectory) return QStringLiteral("directory");
        if (node->name.endsWith(QStringLiteral(".canvas"))) return QStringLiteral("canvas");
        return QStringLiteral("markdown");
    }
    return {};
}

Qt::ItemFlags NotesTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex NotesTreeModel::indexForPath(const QString &relativePath) const
{
    // Walk tree to find node with matching path
    std::function<QModelIndex(TreeNode *, const QString &)> findNode;
    findNode = [&](TreeNode *node, const QString &path) -> QModelIndex {
        for (size_t i = 0; i < node->children.size(); ++i) {
            auto *child = node->children[i].get();
            if (child->relativePath == path) {
                return createIndex(static_cast<int>(i), 0, child);
            }
            if (child->isDirectory) {
                auto result = findNode(child, path);
                if (result.isValid()) return result;
            }
        }
        return {};
    };
    return findNode(m_root.get(), relativePath);
}

NotesTreeModel::TreeNode *NotesTreeModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) return nullptr;
    return static_cast<TreeNode *>(index.internalPointer());
}

NotesTreeModel::TreeNode *NotesTreeModel::findOrCreateDir(const QString &dirPath)
{
    if (dirPath.isEmpty()) return m_root.get();

    const auto parts = dirPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    TreeNode *current = m_root.get();
    QString accumulated;

    for (const auto &part : parts) {
        if (!accumulated.isEmpty()) accumulated += QLatin1Char('/');
        accumulated += part;

        TreeNode *found = nullptr;
        for (auto &child : current->children) {
            if (child->isDirectory && child->name == part) {
                found = child.get();
                break;
            }
        }
        if (!found) {
            auto node = std::make_unique<TreeNode>();
            node->name = part;
            node->relativePath = accumulated;
            node->isDirectory = true;
            node->parentNode = current;
            found = node.get();
            current->children.push_back(std::move(node));
        }
        current = found;
    }
    return current;
}

void NotesTreeModel::sortChildren(TreeNode *node)
{
    std::sort(node->children.begin(), node->children.end(),
              [](const std::unique_ptr<TreeNode> &a, const std::unique_ptr<TreeNode> &b) {
                  // Directories before files
                  if (a->isDirectory != b->isDirectory) return a->isDirectory;
                  // Alphabetical within same type (case insensitive)
                  return a->name.compare(b->name, Qt::CaseInsensitive) < 0;
              });
    for (auto &child : node->children) {
        if (child->isDirectory) sortChildren(child.get());
    }
}

void NotesTreeModel::rebuild()
{
    beginResetModel();
    m_root->children.clear();

    if (!m_vault) {
        endResetModel();
        return;
    }

    QSet<QString> seenPaths; // Guard against duplicates

    for (TFile *tf : m_vault->getFiles()) {
        if (!isTreeFile(tf)) continue;
        const QString path = tf->path;
        if (seenPaths.contains(path)) continue;
        seenPaths.insert(path);

        int lastSlash = path.lastIndexOf(QLatin1Char('/'));
        QString dirPath = lastSlash > 0 ? path.left(lastSlash) : QString();
        QString fileName = path.mid(lastSlash + 1);
        if (fileName.isEmpty()) continue;

        auto *parentDir = findOrCreateDir(dirPath);
        auto node = std::make_unique<TreeNode>();
        node->name = fileName;
        node->relativePath = path;
        node->isDirectory = false;
        node->parentNode = parentDir;
        parentDir->children.push_back(std::move(node));
    }

    sortChildren(m_root.get());
    endResetModel();
}

void NotesTreeModel::onCreated(TAbstractFile *f)
{
    TFile *tf = dynamic_cast<TFile *>(f);
    if (!isTreeFile(tf)) return;
    // Simple approach: full rebuild. Optimize later if needed.
    rebuild();
}

void NotesTreeModel::onDeleted(TAbstractFile *f)
{
    TFile *tf = dynamic_cast<TFile *>(f);
    if (!isTreeFile(tf)) return;
    rebuild();
}

void NotesTreeModel::onRenamed(TAbstractFile *f, const QString &oldPath)
{
    Q_UNUSED(oldPath)
    TFile *tf = dynamic_cast<TFile *>(f);
    if (!isTreeFile(tf)) return;
    rebuild();
}

} // namespace Corbomite
