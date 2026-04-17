// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/Vault.h"

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/DataAdapter.h"

#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <functional>

#include "PathNormalization.h"
#include "Watcher.h"

namespace Corbomite {

Vault::Vault(DataAdapter *adapter, QObject *parent)
    : QObject(parent)
    , m_adapter(adapter)
{
    static const int kMetatypes = [] {
        qRegisterMetaType<Corbomite::TAbstractFile *>("Corbomite::TAbstractFile*");
        qRegisterMetaType<Corbomite::TFile *>("Corbomite::TFile*");
        qRegisterMetaType<Corbomite::TFolder *>("Corbomite::TFolder*");
        return 0;
    }();
    Q_UNUSED(kMetatypes);

    auto root = std::make_unique<TFolder>(this, QStringLiteral("/"));
    m_root = root.get();
    m_fileMap.emplace(QStringLiteral("/"), std::move(root));

    m_watcher = std::make_unique<detail::Watcher>(this);
    connect(m_watcher.get(), &detail::Watcher::created,
            this, &Vault::onExternalCreated);
    connect(m_watcher.get(), &detail::Watcher::modified,
            this, &Vault::onExternalModified);
    connect(m_watcher.get(), &detail::Watcher::deleted,
            this, &Vault::onExternalDeleted);
    connect(m_watcher.get(), &detail::Watcher::renamed,
            this, &Vault::onExternalRenamed);
}

Vault::~Vault() = default;

void Vault::load(const QString &basePath)
{
    unload();
    m_basePath = QDir::cleanPath(basePath);
    buildTree();
    if (m_watcher) m_watcher->start(m_basePath);
    m_loaded = true;
}

void Vault::unload()
{
    const bool wasLoaded = m_loaded;
    if (m_watcher) m_watcher->stop();
    teardownTree();
    m_basePath.clear();
    m_loaded = false;
    if (wasLoaded) Q_EMIT closed();
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

void Vault::onExternalCreated(const QString &relPath)
{
    const QString rel = VaultPaths::normalize(relPath);
    if (m_fileMap.count(rel)) return;  // already tracked

    QFileInfo fi(m_basePath + QLatin1Char('/') + rel);
    if (!fi.exists()) return;

    TFolder *parent = m_root;
    const int slash = rel.lastIndexOf(QLatin1Char('/'));
    if (slash > 0) {
        if (auto *p = getFolderByPath(rel.left(slash))) parent = p;
    }

    if (fi.isDir()) {
        auto folder = std::make_unique<TFolder>(this, rel);
        folder->parent = parent;
        parent->children.append(folder.get());
        TAbstractFile *raw = folder.get();
        m_fileMap.emplace(rel, std::move(folder));
        Q_EMIT created(raw);
    } else if (fi.isFile()) {
        auto file = std::make_unique<TFile>(this, rel);
        file->parent = parent;
        FileStat fs;
        fs.exists    = true;
        fs.isFile    = true;
        fs.sizeBytes = fi.size();
        fs.mtimeMs   = fi.lastModified().toMSecsSinceEpoch();
        fs.ctimeMs   = fi.birthTime().toMSecsSinceEpoch();
        file->stat   = fs;
        parent->children.append(file.get());
        TAbstractFile *raw = file.get();
        m_fileMap.emplace(rel, std::move(file));
        Q_EMIT created(raw);
    }
}

void Vault::onExternalModified(const QString &relPath)
{
    const QString rel = VaultPaths::normalize(relPath);
    TFile *f = getFileByPath(rel);
    if (!f) return;
    QFileInfo fi(m_basePath + QLatin1Char('/') + rel);
    if (!fi.exists()) return;
    const qint64 mtimeMs = fi.lastModified().toMSecsSinceEpoch();
    if (consumeSelfWrite(rel, mtimeMs)) return;  // self-write echo, suppress

    FileStat fs;
    fs.exists    = true;
    fs.isFile    = true;
    fs.sizeBytes = fi.size();
    fs.mtimeMs   = mtimeMs;
    fs.ctimeMs   = fi.birthTime().toMSecsSinceEpoch();
    f->stat      = fs;
    Q_EMIT modified(f);
}

void Vault::stampSelfWrite(const QString &rel, qint64 mtimeMs)
{
    m_selfWriteMtimes.insert(rel, mtimeMs);
    QTimer::singleShot(1000, this, [this, rel] {
        m_selfWriteMtimes.remove(rel);
    });
}

bool Vault::consumeSelfWrite(const QString &rel, qint64 mtimeMs)
{
    auto it = m_selfWriteMtimes.find(rel);
    if (it == m_selfWriteMtimes.end()) return false;
    if (it.value() != mtimeMs) return false;
    m_selfWriteMtimes.erase(it);
    return true;
}

void Vault::onExternalDeleted(const QString &relPath)
{
    const QString rel = VaultPaths::normalize(relPath);
    auto it = m_fileMap.find(rel);
    if (it == m_fileMap.end()) return;

    std::unique_ptr<TAbstractFile> owned = std::move(it->second);
    m_fileMap.erase(it);

    owned->deleted = true;
    if (TFolder *parent = owned->parent) {
        parent->children.removeAll(owned.get());
    }
    TAbstractFile *raw = owned.get();
    m_pendingDelete.push_back(std::move(owned));
    Q_EMIT deletedFile(raw);

    QTimer::singleShot(0, this, [this] { m_pendingDelete.clear(); });
}

void Vault::onExternalRenamed(const QString &oldRel, const QString &newRel)
{
    const QString oldR = VaultPaths::normalize(oldRel);
    const QString newR = VaultPaths::normalize(newRel);
    auto it = m_fileMap.find(oldR);
    if (it == m_fileMap.end()) return;
    auto node = std::move(it->second);
    m_fileMap.erase(it);
    node->setPath(newR);
    TAbstractFile *raw = node.get();
    m_fileMap.emplace(newR, std::move(node));
    Q_EMIT renamed(raw, oldR);
}

} // namespace Corbomite
