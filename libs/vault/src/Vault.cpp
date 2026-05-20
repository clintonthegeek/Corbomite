// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/Vault.h"

#include "corbomite/core/NoteDocument.h"
#include <markoff/ParsePool.h>
#include <markoff/MarkoffDocument.h>
#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/storage/DataAdapter.h"
#include "corbomite/storage/CaseSensitivityProbe.h"
#include "corbomite/storage/VaultConfig.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "PathNormalization.h"
#include "Watcher.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcVaultSafety, "corbomite.vault.safety")

namespace Corbomite {

Vault::Vault(DataAdapter *adapter, QObject *parent)
    : QObject(parent)
    , m_adapter(adapter)
{
    static const int kMetatypes = [] {
        qRegisterMetaType<Corbomite::TAbstractFile *>("Corbomite::TAbstractFile*");
        qRegisterMetaType<Corbomite::TFile *>("Corbomite::TFile*");
        qRegisterMetaType<Corbomite::TFolder *>("Corbomite::TFolder*");
        qRegisterMetaType<Corbomite::NoteDocument *>("Corbomite::NoteDocument*");
        return 0;
    }();
    Q_UNUSED(kMetatypes);

    auto root = std::make_unique<TFolder>(this, QStringLiteral("/"));
    m_root = root.get();
    m_fileMap.emplace(QStringLiteral("/"), std::move(root));

    m_parsePool = std::make_unique<Markoff::ParsePool>(this);

    m_watcher = std::make_unique<detail::Watcher>(this);
    connect(m_watcher.get(), &detail::Watcher::created,
            this, &Vault::onExternalCreated);
    connect(m_watcher.get(), &detail::Watcher::modified,
            this, &Vault::onExternalModified);
    connect(m_watcher.get(), &detail::Watcher::deleted,
            this, &Vault::onExternalDeleted);
    connect(m_watcher.get(), &detail::Watcher::renamed,
            this, &Vault::onExternalRenamed);
    connect(m_watcher.get(), &detail::Watcher::rawChange,
            this, &Vault::onExternalRaw);
}

Vault::~Vault()
{
    // Explicitly destroy NoteDocuments here so their MarkoffDocument
    // destructors (which call ParsePool::cancelJobsFor) run while
    // m_parsePool is still alive. Qt's parent–child mechanism would
    // destroy them after member variables are gone, which causes UAF.
    qDeleteAll(m_docs);
    m_docs.clear();
}

void Vault::load(const QString &basePath)
{
    unload();
    m_basePath = QDir::cleanPath(basePath);
    // Probe the underlying filesystem's case-sensitivity once at load. The
    // `create()`/`createFolder()` collision check is always case-insensitive
    // (vault-portable semantics, matching Obsidian); this flag is exposed
    // separately so plugins / future Corbomite paths can warn when a vault
    // contains case-collisions that *will* break on a different FS.
    m_caseSensitiveFs = m_adapter
        ? CaseSensitivityProbe::isCaseSensitive(m_adapter, m_basePath)
        : true;
    buildTree();
    if (m_watcher) m_watcher->start(m_basePath);
    m_loaded = true;
}

bool Vault::isCaseSensitiveFilesystem() const { return m_caseSensitiveFs; }

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

namespace {
// Obsidian spec §3: Vault.read strips a leading UTF-8 BOM (U+FEFF, EF BB BF)
// before returning. readBinary preserves the bytes verbatim.
inline QByteArray stripUtf8Bom(QByteArray bytes)
{
    if (bytes.size() >= 3
        && static_cast<unsigned char>(bytes[0]) == 0xEF
        && static_cast<unsigned char>(bytes[1]) == 0xBB
        && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.remove(0, 3);
    }
    return bytes;
}
} // namespace

QByteArray Vault::read(TFile *f) const
{
    if (!f || !m_adapter) return {};
    auto body = m_adapter->readBinary(m_basePath + QLatin1Char('/') + f->path);
    return body.has_value() ? stripUtf8Bom(*body) : QByteArray{};
}

QByteArray Vault::readBinary(TFile *f) const
{
    if (!f || !m_adapter) return {};
    auto body = m_adapter->readBinary(m_basePath + QLatin1Char('/') + f->path);
    return body.has_value() ? *body : QByteArray{};
}

QByteArray Vault::readRaw(const QString &path) const
{
    if (!m_adapter) return {};
    auto body = m_adapter->readBinary(
        m_basePath + QLatin1Char('/') + VaultPaths::normalize(path));
    return body.has_value() ? stripUtf8Bom(*body) : QByteArray{};
}

QByteArray Vault::cachedRead(TFile *f)
{
    if (!f) return {};
    auto it = m_readCache.constFind(f->path);
    if (it != m_readCache.cend()) return it.value();
    const QByteArray body = read(f);
    m_readCache.insert(f->path, body);
    return body;
}

bool Vault::modify(TFile *f, const QByteArray &body, const WriteHints &hints)
{
    if (!f || !m_adapter) return false;
    const QString abs = m_basePath + QLatin1Char('/') + f->path;

    WriteHints effective = hints;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!effective.mtimeMs.has_value()) effective.mtimeMs = nowMs;
    stampSelfWrite(f->path, *effective.mtimeMs);

    if (!m_adapter->writeBinary(abs, body, effective)) return false;

    m_readCache.insert(f->path, body);
    if (f->stat.has_value()) {
        f->stat->mtimeMs   = *effective.mtimeMs;
        f->stat->sizeBytes = body.size();
    } else {
        FileStat fs;
        fs.exists    = true;
        fs.isFile    = true;
        fs.sizeBytes = body.size();
        fs.mtimeMs   = *effective.mtimeMs;
        f->stat      = fs;
    }
    Q_EMIT modified(f);
    return true;
}

bool Vault::modifyBinary(TFile *f, const QByteArray &body, const WriteHints &hints)
{
    return modify(f, body, hints);
}

bool Vault::append(TFile *f, const QByteArray &body)
{
    if (!f) return false;
    const QByteArray cur = read(f);
    return modify(f, cur + body);
}

namespace {
// Per-path lock registry — Vault::process serialises concurrent calls on
// the same file so a RMW cycle never loses updates. Lives at the module
// level rather than per-Vault because the underlying filesystem is shared.
QMutex &lockForPath(const QString &absolutePath)
{
    static QMutex registryMutex;
    static std::unordered_map<std::string, std::unique_ptr<QMutex>> registry;

    QMutexLocker guard(&registryMutex);
    const std::string key = absolutePath.toStdString();
    auto it = registry.find(key);
    if (it == registry.end()) {
        it = registry.emplace(key, std::make_unique<QMutex>()).first;
    }
    return *it->second;
}
} // namespace

bool Vault::process(TFile *f, const ProcessMutator &mutator)
{
    if (!f || !mutator) return false;
    const QString abs = m_basePath + QLatin1Char('/') + f->path;

    QMutexLocker pathLock(&lockForPath(abs));

    const QByteArray cur = read(f);
    const QByteArray next = mutator(cur);
    return modify(f, next);
}

// Reject when `rel` collides with any existing entry case-insensitively.
// Required for vault-portability: the same vault on a case-insensitive FS
// (HFS+, exFAT, Windows NTFS default) would resolve `Note.md` and `note.md`
// to the same file on disk, but std::unordered_map<QString, …> compares
// case-sensitively. Without this scan create() would happily write a second
// file that races with or stomps the first.
bool Vault::existsCaseInsensitive(const QString &rel) const
{
    for (const auto &kv : m_fileMap) {
        if (kv.first.compare(rel, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

TFile *Vault::create(const QString &path, const QByteArray &body)
{
    if (!m_adapter) return nullptr;
    const QString rel = VaultPaths::normalize(path);
    if (existsCaseInsensitive(rel)) return nullptr;

    const QString abs = m_basePath + QLatin1Char('/') + rel;
    if (!m_adapter->mkpath(QFileInfo(abs).absolutePath())) return nullptr;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    WriteHints hints;
    hints.mtimeMs = nowMs;
    stampSelfWrite(rel, nowMs);
    if (!m_adapter->writeBinary(abs, body, hints)) return nullptr;

    // Build any missing intermediate folders on the in-memory tree.
    TFolder *parent = m_root;
    const int slash = rel.lastIndexOf(QLatin1Char('/'));
    if (slash > 0) {
        const QString parentRel = rel.left(slash);
        if (auto *p = getFolderByPath(parentRel)) {
            parent = p;
        } else {
            const QStringList segments = parentRel.split(QLatin1Char('/'));
            QString cur;
            TFolder *p2 = m_root;
            for (const QString &seg : segments) {
                cur = cur.isEmpty() ? seg : cur + QLatin1Char('/') + seg;
                if (auto *existing = getFolderByPath(cur)) { p2 = existing; continue; }
                auto folder = std::make_unique<TFolder>(this, cur);
                folder->parent = p2;
                p2->children.append(folder.get());
                TFolder *raw = folder.get();
                m_fileMap.emplace(cur, std::move(folder));
                Q_EMIT created(raw);
                p2 = raw;
            }
            parent = p2;
        }
    }

    auto owned = std::make_unique<TFile>(this, rel);
    FileStat st;
    st.exists    = true;
    st.isFile    = true;
    st.sizeBytes = body.size();
    st.mtimeMs   = nowMs;
    st.ctimeMs   = nowMs;
    owned->stat  = st;
    owned->parent = parent;
    parent->children.append(owned.get());
    TFile *raw = owned.get();
    m_fileMap.emplace(rel, std::move(owned));
    m_readCache.insert(rel, body);
    Q_EMIT created(raw);
    return raw;
}

TFile *Vault::createBinary(const QString &path, const QByteArray &body)
{
    return create(path, body);
}

TFolder *Vault::createFolder(const QString &path)
{
    if (!m_adapter) return nullptr;
    const QString rel = VaultPaths::normalize(path);
    if (existsCaseInsensitive(rel)) return nullptr;
    const QString abs = m_basePath + QLatin1Char('/') + rel;
    if (!m_adapter->mkpath(abs)) return nullptr;

    TFolder *parent = m_root;
    const int slash = rel.lastIndexOf(QLatin1Char('/'));
    if (slash > 0) {
        if (auto *p = getFolderByPath(rel.left(slash))) parent = p;
    }
    auto owned = std::make_unique<TFolder>(this, rel);
    owned->parent = parent;
    parent->children.append(owned.get());
    TFolder *raw = owned.get();
    m_fileMap.emplace(rel, std::move(owned));
    Q_EMIT created(raw);
    return raw;
}

bool Vault::rename(TAbstractFile *f, const QString &newPath)
{
    if (!f || !m_adapter) return false;
    const QString oldRel = f->path;
    const QString newRel = VaultPaths::normalize(newPath);
    if (oldRel == newRel) return true;
    if (m_fileMap.count(newRel)) return false;

    const QString oldAbs = m_basePath + QLatin1Char('/') + oldRel;
    const QString newAbs = m_basePath + QLatin1Char('/') + newRel;
    m_adapter->mkpath(QFileInfo(newAbs).absolutePath());
    if (!m_adapter->rename(oldAbs, newAbs)) return false;

    auto it = m_fileMap.find(oldRel);
    if (it == m_fileMap.end()) return false;
    std::unique_ptr<TAbstractFile> node = std::move(it->second);
    m_fileMap.erase(it);

    if (TFolder *p = node->parent) p->children.removeAll(node.get());
    TFolder *newParent = m_root;
    const int slash = newRel.lastIndexOf(QLatin1Char('/'));
    if (slash > 0) {
        if (auto *p = getFolderByPath(newRel.left(slash))) newParent = p;
    }
    node->parent = newParent;
    newParent->children.append(node.get());
    node->setPath(newRel);

    if (m_readCache.contains(oldRel)) {
        m_readCache.insert(newRel, m_readCache.take(oldRel));
    }

    // Open NoteDocument for the renamed file: rekey the cache and notify
    // the document so views holding it (FileView subclasses) can refresh
    // their title/header. Mirrors Obsidian's vault.on('rename') →
    // FileView.onload propagation. Folder renames are handled per-
    // descendant in the loop below.
    if (auto *doc = m_docs.take(oldRel)) {
        m_docs.insert(newRel, doc);
        doc->setRelativePath(newRel);
    }

    TAbstractFile *raw = node.get();
    m_fileMap.emplace(newRel, std::move(node));
    Q_EMIT renamed(raw, oldRel);

    // Folder rename: descendants in m_fileMap still hold stale paths
    // (Vault::rename only moves the renamed node itself). Walk them, fix
    // the map keys + node->path, migrate read-cache entries, and emit
    // per-descendant `renamed` so plugins listening for moves (link
    // rewriters, MetadataCache invalidators, file watchers) can react.
    if (dynamic_cast<TFolder *>(raw)) {
        const QString oldPrefix = oldRel + QLatin1Char('/');
        const QString newPrefix = newRel + QLatin1Char('/');
        std::vector<QString> oldDescPaths;
        oldDescPaths.reserve(m_fileMap.size());
        for (const auto &kv : m_fileMap) {
            if (kv.first.startsWith(oldPrefix)) oldDescPaths.push_back(kv.first);
        }
        for (const QString &oldDesc : oldDescPaths) {
            auto descIt = m_fileMap.find(oldDesc);
            if (descIt == m_fileMap.end()) continue;
            std::unique_ptr<TAbstractFile> dnode = std::move(descIt->second);
            m_fileMap.erase(descIt);
            const QString newDesc = newPrefix + oldDesc.mid(oldPrefix.size());
            dnode->setPath(newDesc);
            if (m_readCache.contains(oldDesc)) {
                m_readCache.insert(newDesc, m_readCache.take(oldDesc));
            }
            if (auto *descDoc = m_docs.take(oldDesc)) {
                m_docs.insert(newDesc, descDoc);
                descDoc->setRelativePath(newDesc);
            }
            TAbstractFile *draw = dnode.get();
            m_fileMap.emplace(newDesc, std::move(dnode));
            Q_EMIT renamed(draw, oldDesc);
        }
    }
    return true;
}

bool Vault::remove(TAbstractFile *f, bool recursive)
{
    if (!f || !m_adapter) return false;
    const QString rel = f->path;
    const QString abs = m_basePath + QLatin1Char('/') + rel;

    if (auto *folder = dynamic_cast<TFolder *>(f)) {
        if (!recursive && !folder->children.isEmpty()) return false;
        if (!m_adapter->rmdir(abs)) return false;
    } else {
        if (!m_adapter->remove(abs)) return false;
    }

    auto it = m_fileMap.find(rel);
    if (it == m_fileMap.end()) return false;
    std::unique_ptr<TAbstractFile> node = std::move(it->second);
    m_fileMap.erase(it);

    if (TFolder *p = node->parent) p->children.removeAll(node.get());
    node->deleted = true;
    m_readCache.remove(rel);
    if (auto *doc = m_docs.take(rel)) {
        doc->markDeleted();
        doc->deleteLater();
    }

    TAbstractFile *raw = node.get();
    m_pendingDelete.push_back(std::move(node));
    Q_EMIT deletedFile(raw);
    QTimer::singleShot(0, this, [this] { m_pendingDelete.clear(); });
    return true;
}

bool Vault::copy(TAbstractFile *f, const QString &newPath)
{
    if (!f || !m_adapter) return false;
    const QString newRel = VaultPaths::normalize(newPath);
    if (m_fileMap.count(newRel)) return false;

    if (auto *file = dynamic_cast<TFile *>(f)) {
        const QByteArray body = read(file);
        return create(newRel, body) != nullptr;
    }
    // Recursive folder copy is declared in the spec as scope-deferred.
    return false;
}

void Vault::setConfigDir(const QString &d)
{
    if (d.isEmpty() || d == QStringLiteral(".")) return;
    if (!d.startsWith(QLatin1Char('.'))) return;
    m_configDir = d;
}

namespace {
QString configJsonAbs(const QString &basePath, const QString &configDir,
                      const QString &name)
{
    QString stem = name;
    if (stem.endsWith(QStringLiteral(".json"))) stem.chop(5);
    return basePath + QLatin1Char('/') + configDir + QLatin1Char('/')
         + stem + QStringLiteral(".json");
}
}

QJsonValue Vault::readConfigJson(const QString &name) const
{
    if (!m_adapter || m_basePath.isEmpty()) return {};
    const QString abs = configJsonAbs(m_basePath, m_configDir, name);
    auto body = m_adapter->readBinary(abs);
    if (!body.has_value()) return {};
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(*body, &err);
    if (err.error != QJsonParseError::NoError) return {};
    if (doc.isObject()) return doc.object();
    if (doc.isArray())  return doc.array();
    return {};
}

bool Vault::writeConfigJson(const QString &name, const QJsonValue &value)
{
    if (!m_adapter || m_basePath.isEmpty()) return false;
    m_adapter->mkpath(m_basePath + QLatin1Char('/') + m_configDir);
    const QString abs = configJsonAbs(m_basePath, m_configDir, name);

    QJsonDocument doc;
    if (value.isObject())      doc = QJsonDocument(value.toObject());
    else if (value.isArray())  doc = QJsonDocument(value.toArray());
    else return false;

    return m_adapter->writeBinary(abs, VaultConfig::serializeObsidianStyle(doc));
}

bool Vault::deleteConfigJson(const QString &name)
{
    if (!m_adapter) return false;
    return m_adapter->remove(configJsonAbs(m_basePath, m_configDir, name));
}

bool Vault::trash(TAbstractFile *f, bool useSystem)
{
    if (!f || !m_adapter) return false;
    if (dynamic_cast<TFolder *>(f) && f->path == QStringLiteral("/")) return false;

    if (useSystem) {
        const QString abs = m_basePath + QLatin1Char('/') + f->path;
        if (m_adapter->moveToTrash(abs)) {
            const QString rel = f->path;
            auto it = m_fileMap.find(rel);
            if (it != m_fileMap.end()) {
                std::unique_ptr<TAbstractFile> node = std::move(it->second);
                m_fileMap.erase(it);
                if (TFolder *p = node->parent) p->children.removeAll(node.get());
                node->deleted = true;
                m_readCache.remove(rel);
                if (auto *doc = m_docs.take(rel)) {
                    doc->markDeleted();
                    doc->deleteLater();
                }
                TAbstractFile *raw = node.get();
                m_pendingDelete.push_back(std::move(node));
                Q_EMIT deletedFile(raw);
                QTimer::singleShot(0, this, [this] { m_pendingDelete.clear(); });
            }
            return true;
        }
        // Fall through to local trash.
    }

    const QString rel = f->path;
    const QFileInfo fi(rel);
    const QString base = fi.completeBaseName();
    const QString ext  = fi.suffix();
    const QString trashRoot = QStringLiteral(".trash");
    m_adapter->mkpath(m_basePath + QLatin1Char('/') + trashRoot);

    QString candidate = base + (ext.isEmpty() ? QString() : (QLatin1Char('.') + ext));
    QString candidateRel = trashRoot + QLatin1Char('/') + candidate;
    int n = 2;
    while (QFileInfo::exists(m_basePath + QLatin1Char('/') + candidateRel)) {
        candidate = base + QStringLiteral(" ") + QString::number(n)
                  + (ext.isEmpty() ? QString() : (QLatin1Char('.') + ext));
        candidateRel = trashRoot + QLatin1Char('/') + candidate;
        ++n;
    }

    if (!m_adapter->rename(m_basePath + QLatin1Char('/') + rel,
                           m_basePath + QLatin1Char('/') + candidateRel)) {
        return false;
    }

    auto it = m_fileMap.find(rel);
    if (it != m_fileMap.end()) {
        std::unique_ptr<TAbstractFile> node = std::move(it->second);
        m_fileMap.erase(it);
        if (TFolder *p = node->parent) p->children.removeAll(node.get());
        node->deleted = true;
        m_readCache.remove(rel);
        if (auto *doc = m_docs.take(rel)) {
            doc->markDeleted();
            doc->deleteLater();
        }
        TAbstractFile *raw = node.get();
        m_pendingDelete.push_back(std::move(node));
        Q_EMIT deletedFile(raw);
        QTimer::singleShot(0, this, [this] { m_pendingDelete.clear(); });
    }
    return true;
}

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
    qDeleteAll(m_docs);
    m_docs.clear();
    m_fileMap.clear();
    m_readCache.clear();
    auto root = std::make_unique<TFolder>(this, QStringLiteral("/"));
    m_root = root.get();
    m_fileMap.emplace(QStringLiteral("/"), std::move(root));
}

NoteDocument *Vault::openDocument(const QString &relPath)
{
    if (!m_loaded) return nullptr;
    const QString rel = VaultPaths::normalize(relPath);
    if (auto *existing = m_docs.value(rel)) return existing;

    auto *doc = new NoteDocument(m_basePath, rel, m_parsePool.get(), this);
    if (auto *tf = getFileByPath(rel)) {
        const QByteArray bytes = cachedRead(tf);
        doc->markoff()->resetContent(QString::fromUtf8(bytes),
                                     Markoff::Origin::FirstOpen);
    }
    // FirstOpen emits contentsChanged which sets modified=true; undo that.
    doc->setModified(false);
    m_docs.insert(rel, doc);
    return doc;
}

NoteDocument *Vault::cachedDocument(const QString &relPath) const
{
    if (relPath.isEmpty()) return nullptr;
    return m_docs.value(VaultPaths::normalize(relPath));
}

bool Vault::saveDocument(NoteDocument *doc)
{
    if (!doc) return false;
    const QString rel = VaultPaths::normalize(doc->relativePath());
    TFile *tf = getFileByPath(rel);
    if (!tf) return false;

    // Write canonical UTF-8 bytes verbatim — no QTextDocumentWriter, no
    // format coercion. toMarkdown() is the MarkoffDocument's canonical source.
    const QString markdown = doc->markoff()->toMarkdown();

    // Terminal guard (C8 Task 5.4): the canonical buffer must never contain
    // U+FFFC. If it does, a presentation-layer glyph leaked into the source
    // of truth. Refuse the write, emit saveFailed, and leave disk untouched.
    if (markdown.contains(QChar::ObjectReplacementCharacter)) {
        qCCritical(lcVaultSafety,
            "Vault::saveDocument REFUSED: canonical buffer contains U+FFFC "
            "for rel=\"%s\" (chars=%lld). Aborting write; file unchanged.",
            qUtf8Printable(rel), static_cast<long long>(markdown.size()));
        Q_EMIT doc->saveFailed();
        return false;
    }

    const QByteArray bytes = markdown.toUtf8();
    const QString abs = m_basePath + QLatin1Char('/') + rel;

    // Stamp echo-suppression BEFORE the write so the watcher's mtime-based
    // ledger already holds this path when the OS notification arrives.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    stampSelfWrite(rel, nowMs);

    QFile f(abs);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // Remove the stamp — write never happened.
        m_selfWriteMtimes.remove(rel);
        return false;
    }
    const qint64 written = f.write(bytes);
    f.close();

    if (written != static_cast<qint64>(bytes.size())) {
        m_selfWriteMtimes.remove(rel);
        return false;
    }

    // Keep the read-cache and TFile stat consistent with what we just wrote.
    m_readCache.insert(rel, bytes);
    if (tf->stat.has_value()) {
        tf->stat->sizeBytes = bytes.size();
        tf->stat->mtimeMs   = nowMs;
    }

    doc->setModified(false);
    Q_EMIT doc->saved();
    Q_EMIT documentSaved(rel);
    return true;
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

    // Defense-in-depth: even if the mtime-ledger leaks, a byte-equal read
    // is a no-op. Compare disk bytes against the canonical document bytes
    // so saves that land back the same content never trigger a reload.
    if (NoteDocument *doc = m_docs.value(rel)) {
        QFile diskFile(m_basePath + QLatin1Char('/') + rel);
        if (diskFile.open(QIODevice::ReadOnly)) {
            const QByteArray disk = diskFile.readAll();
            if (disk == doc->markoff()->toMarkdown().toUtf8()) {
                consumeSelfWrite(rel, mtimeMs);  // drain ledger entry if present
                return;  // byte-equal — not a real external change
            }
        }
    }

    if (consumeSelfWrite(rel, mtimeMs)) return;  // self-write echo, suppress

    // Update tree metadata + invalidate read cache regardless of whether an
    // open NoteDocument exists.
    FileStat fs;
    fs.exists    = true;
    fs.isFile    = true;
    fs.sizeBytes = fi.size();
    fs.mtimeMs   = mtimeMs;
    fs.ctimeMs   = fi.birthTime().toMSecsSinceEpoch();
    f->stat      = fs;
    m_readCache.remove(rel);
    Q_EMIT modified(f);

    // --- Origin dispatch (spec §6.2) ---
    NoteDocument *doc = m_docs.value(rel);
    if (!doc) return;  // no open document — tree + cache update above is sufficient

    QFile diskFile(m_basePath + QLatin1Char('/') + rel);
    if (!diskFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Vault::onExternalModified: cannot read" << rel;
        return;
    }
    const QString newContent = QString::fromUtf8(diskFile.readAll());

    if (!doc->isModified()) {
        // Clean case: apply wholesale, clear undo stack, emit documentReloaded.
        doc->markoff()->resetContent(newContent, Markoff::Origin::ExternalReloadClean);
        // resetContent emits documentReloaded, which fires NoteDocument's
        // handler and sets modified=true (because !d->modified was true before
        // the reload). Explicitly reset to false after the reload.
        doc->setModified(false);
    } else {
        // Dirty case: do NOT auto-apply; defer to UI via the conflict signal.
        Q_EMIT externalReloadConflict(doc, newContent);
    }
}

void Vault::resolveExternalReload(NoteDocument *doc, const QString &resolvedContent)
{
    if (!doc) return;
    doc->markoff()->resetContent(resolvedContent, Markoff::Origin::ExternalReloadResolved);
    // Same reasoning as the clean case: resetContent → documentReloaded →
    // NoteDocument sets modified=true; override to false post-merge.
    doc->setModified(false);
}

void Vault::onExternalRaw(const QString &relPath)
{
    const QString rel = VaultPaths::normalize(relPath);
    Q_EMIT raw(rel);
    if (rel.startsWith(QStringLiteral(".obsidian/"))
        && rel.endsWith(QStringLiteral(".json"))) {
        Q_EMIT configChanged(rel);
    }
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
    m_readCache.remove(rel);
    if (auto *doc = m_docs.take(rel)) {
        doc->markDeleted();
        doc->deleteLater();
    }
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
    m_readCache.remove(oldR);
    if (auto *doc = m_docs.take(oldR)) {
        m_docs.insert(newR, doc);
        // Notify views holding the doc that its path moved (used by
        // FileView subclasses to refresh their title chrome).
        doc->setRelativePath(newR);
    }
    node->setPath(newR);
    TAbstractFile *raw = node.get();
    m_fileMap.emplace(newR, std::move(node));
    Q_EMIT renamed(raw, oldR);
}

} // namespace Corbomite
