// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/Vault.h"

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/DataAdapter.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

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
    if (m_basePath.isEmpty()) return;

    QDirIterator it(m_basePath,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString abs = it.next();
        QFileInfo fi(abs);
        QString rel = VaultPaths::normalize(QDir(m_basePath).relativeFilePath(abs));
        if (rel.startsWith(QStringLiteral(".obsidian/")) ||
            rel == QStringLiteral(".obsidian") ||
            rel.startsWith(QStringLiteral(".corbomite/")) ||
            rel == QStringLiteral(".corbomite") ||
            rel.startsWith(QStringLiteral(".trash/")) ||
            rel == QStringLiteral(".trash")) {
            continue;
        }

        if (fi.isDir()) {
            auto folder = std::make_unique<TFolder>(this, rel);
            m_fileMap.emplace(rel, std::move(folder));
        } else if (fi.isFile()) {
            auto file = std::make_unique<TFile>(this, rel);
            FileStat stat;
            stat.exists    = true;
            stat.isFile    = true;
            stat.sizeBytes = fi.size();
            stat.mtimeMs   = fi.lastModified().toMSecsSinceEpoch();
            stat.ctimeMs   = fi.birthTime().toMSecsSinceEpoch();
            file->stat     = stat;
            m_fileMap.emplace(rel, std::move(file));
        }
    }
    for (auto &[k, v] : m_fileMap) {
        if (k == QStringLiteral("/")) continue;
        const int slash = k.lastIndexOf(QLatin1Char('/'));
        TFolder *parent = m_root;
        if (slash > 0) {
            auto pit = m_fileMap.find(k.left(slash));
            if (pit != m_fileMap.end()) {
                parent = dynamic_cast<TFolder *>(pit->second.get());
            }
        }
        if (parent) {
            v->parent = parent;
            parent->children.append(v.get());
        }
    }
}

void Vault::teardownTree()
{
    m_fileMap.clear();
    auto root = std::make_unique<TFolder>(this, QStringLiteral("/"));
    m_root = root.get();
    m_fileMap.emplace(QStringLiteral("/"), std::move(root));
}

} // namespace Corbomite
