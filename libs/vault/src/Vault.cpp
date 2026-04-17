// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/Vault.h"

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/DataAdapter.h"

#include <QDir>
#include <QFileInfo>
#include <functional>

#include "PathNormalization.h"

namespace Corbomite {

Vault::Vault(DataAdapter *adapter, QObject *parent)
    : QObject(parent)
    , m_adapter(adapter)
{
    auto root = std::make_unique<TFolder>(this, QStringLiteral("/"));
    m_root = root.get();
    m_fileMap.emplace(QStringLiteral("/"), std::move(root));
}

Vault::~Vault() = default;

void Vault::load(const QString &basePath)
{
    unload();
    m_basePath = QDir::cleanPath(basePath);
    buildTree();
    m_loaded = true;
}

void Vault::unload()
{
    teardownTree();
    m_basePath.clear();
    m_loaded = false;
}

bool Vault::isLoaded() const { return m_loaded; }

QString Vault::getName() const
{
    return QFileInfo(m_basePath).fileName();
}

QString Vault::basePath() const { return m_basePath; }

TFolder *Vault::getRoot() const { return m_root; }

TAbstractFile *Vault::getAbstractFileByPath(const QString &path) const
{
    const auto it = m_fileMap.find(VaultPaths::normalize(path));
    return it == m_fileMap.end() ? nullptr : it->second.get();
}

TFile *Vault::getFileByPath(const QString &path) const
{
    return dynamic_cast<TFile *>(getAbstractFileByPath(path));
}

TFolder *Vault::getFolderByPath(const QString &path) const
{
    return dynamic_cast<TFolder *>(getAbstractFileByPath(path));
}

QVector<TFile *> Vault::getMarkdownFiles() const
{
    QVector<TFile *> out;
    for (const auto &[k, v] : m_fileMap) {
        if (auto *f = dynamic_cast<TFile *>(v.get())) {
            if (f->extension == QStringLiteral("md")) out.append(f);
        }
    }
    return out;
}

QVector<TFile *> Vault::getFiles() const
{
    QVector<TFile *> out;
    for (const auto &[k, v] : m_fileMap) {
        if (auto *f = dynamic_cast<TFile *>(v.get())) out.append(f);
    }
    return out;
}

QVector<TAbstractFile *> Vault::getAllLoadedFiles() const
{
    QVector<TAbstractFile *> out;
    out.reserve(static_cast<qsizetype>(m_fileMap.size()));
    for (const auto &[k, v] : m_fileMap) {
        if (k != QStringLiteral("/")) out.append(v.get());
    }
    return out;
}

bool Vault::isEmpty() const { return m_fileMap.size() <= 1; }

void Vault::buildTree()
{
    if (m_basePath.isEmpty() || !m_adapter) return;

    std::function<void(const QString &, TFolder *)> walk =
        [&](const QString &absDir, TFolder *parent) {
            const QStringList entries = m_adapter->list(absDir);
            for (const QString &entry : entries) {
                const QString absChild = absDir + QLatin1Char('/') + entry;
                const QString rel = VaultPaths::normalize(
                    QDir(m_basePath).relativeFilePath(absChild));
                if (rel.startsWith(QStringLiteral(".obsidian/")) ||
                    rel == QStringLiteral(".obsidian") ||
                    rel.startsWith(QStringLiteral(".corbomite/")) ||
                    rel == QStringLiteral(".corbomite") ||
                    rel.startsWith(QStringLiteral(".trash/")) ||
                    rel == QStringLiteral(".trash")) {
                    continue;
                }

                const FileStat st = m_adapter->stat(absChild);
                if (st.isDirectory) {
                    auto folder = std::make_unique<TFolder>(this, rel);
                    folder->parent = parent;
                    parent->children.append(folder.get());
                    TFolder *raw = folder.get();
                    m_fileMap.emplace(rel, std::move(folder));
                    walk(absChild, raw);
                } else if (st.isFile) {
                    auto file = std::make_unique<TFile>(this, rel);
                    file->parent = parent;
                    FileStat fs;
                    fs.exists    = true;
                    fs.isFile    = true;
                    fs.sizeBytes = st.sizeBytes;
                    fs.mtimeMs   = st.mtimeMs;
                    fs.ctimeMs   = st.ctimeMs;
                    file->stat   = fs;
                    parent->children.append(file.get());
                    m_fileMap.emplace(rel, std::move(file));
                }
            }
        };
    walk(m_basePath, m_root);
}

void Vault::teardownTree()
{
    m_fileMap.clear();
    auto root = std::make_unique<TFolder>(this, QStringLiteral("/"));
    m_root = root.get();
    m_fileMap.emplace(QStringLiteral("/"), std::move(root));
}

} // namespace Corbomite
