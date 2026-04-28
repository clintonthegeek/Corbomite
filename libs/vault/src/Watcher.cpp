// SPDX-License-Identifier: GPL-3.0-or-later
#include "Watcher.h"

#include "corbomite/vault/Vault.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace Corbomite::detail {

Watcher::Watcher(Vault *vault, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
{
    m_drainTimer.setSingleShot(true);
    m_drainTimer.setInterval(50);
    connect(&m_drainTimer, &QTimer::timeout, this, &Watcher::drainPending);
    connect(&m_fsw, &QFileSystemWatcher::directoryChanged,
            this, &Watcher::onDirChanged);
    connect(&m_fsw, &QFileSystemWatcher::fileChanged,
            this, &Watcher::onFileChanged);
}

Watcher::~Watcher() = default;

namespace {
// Tree-visible exclusion (skipped from created/modified/deleted/renamed
// signals). `.obsidian/` is excluded from the tree but NOT from the
// rawChange signal — Cluster B (Vault.raw + configChanged) consumes it
// for plugin event surface.
bool isTreeExcluded(const QString &rel)
{
    return rel.startsWith(QStringLiteral(".obsidian/")) ||
           rel == QStringLiteral(".obsidian") ||
           rel.startsWith(QStringLiteral(".corbomite/")) ||
           rel == QStringLiteral(".corbomite") ||
           rel.startsWith(QStringLiteral(".trash/")) ||
           rel == QStringLiteral(".trash") ||
           rel.startsWith(QStringLiteral(".git/")) ||
           rel == QStringLiteral(".git");
}

// Watcher-coverage exclusion (paths we don't bother monitoring at all).
// `.git` and `.trash` get arbitrarily large and have no plugin or vault
// relevance; `.corbomite/` is reserved for future host-internal state.
// `.obsidian/` IS monitored — plugin config is observable via rawChange.
bool isWatchExcluded(const QString &rel)
{
    return rel.startsWith(QStringLiteral(".corbomite/")) ||
           rel == QStringLiteral(".corbomite") ||
           rel.startsWith(QStringLiteral(".trash/")) ||
           rel == QStringLiteral(".trash") ||
           rel.startsWith(QStringLiteral(".git/")) ||
           rel == QStringLiteral(".git");
}
}

void Watcher::start(const QString &basePath)
{
    stop();
    m_basePath = QDir::cleanPath(basePath);
    if (m_basePath.isEmpty()) return;

    m_fsw.addPath(m_basePath);
    snapshotDirectory(m_basePath);

    QDirIterator it(m_basePath,
                    QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString d = it.next();
        const QString rel = toRel(d);
        if (isWatchExcluded(rel)) continue;
        m_fsw.addPath(d);
        snapshotDirectory(d);
    }
}

void Watcher::stop()
{
    const auto dirs = m_fsw.directories();
    if (!dirs.isEmpty()) m_fsw.removePaths(dirs);
    const auto files = m_fsw.files();
    if (!files.isEmpty()) m_fsw.removePaths(files);
    m_knownFiles.clear();
    m_pending.clear();
    m_drainTimer.stop();
    m_basePath.clear();
}

void Watcher::onDirChanged(const QString &absDir)
{
    m_pending.append(absDir);
    m_drainTimer.start();
}

void Watcher::onFileChanged(const QString &absPath)
{
    m_pending.append(absPath);
    m_drainTimer.start();
}

void Watcher::snapshotDirectory(const QString &absDir)
{
    QDirIterator it(absDir, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        const QString rel = toRel(fi.absoluteFilePath());
        if (isWatchExcluded(rel)) continue;
        m_knownFiles.insert(rel, fi.lastModified().toMSecsSinceEpoch());
        // Watch the file itself so content-only modifications (no dir
        // entry churn) trigger fileChanged. Directory-level watchers do not
        // fire on content mutation across all filesystems.
        m_fsw.addPath(fi.absoluteFilePath());
    }
}

void Watcher::drainPending()
{
    if (m_basePath.isEmpty()) {
        m_pending.clear();
        return;
    }

    // Re-snapshot the tree and diff against m_knownFiles.
    QHash<QString, qint64> fresh;
    QDirIterator it(m_basePath,
                    QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        const QString rel = toRel(fi.absoluteFilePath());
        if (isWatchExcluded(rel)) continue;
        fresh.insert(rel, fi.lastModified().toMSecsSinceEpoch());
    }

    // Separate into deleted + created lists; emit modifieds directly.
    // Tree-visible signals (created/modified/deleted/renamed) skip
    // .obsidian/ paths; the rawChange signal fires for every change.
    QStringList createdRels;
    QStringList deletedRels;
    for (auto fit = fresh.cbegin(); fit != fresh.cend(); ++fit) {
        const auto known = m_knownFiles.constFind(fit.key());
        if (known == m_knownFiles.cend()) {
            createdRels.append(fit.key());
        } else if (known.value() != fit.value()) {
            Q_EMIT rawChange(fit.key());
            if (!isTreeExcluded(fit.key())) Q_EMIT modified(fit.key());
        }
    }
    for (auto kit = m_knownFiles.cbegin(); kit != m_knownFiles.cend(); ++kit) {
        if (!fresh.contains(kit.key())) deletedRels.append(kit.key());
    }

    // Pair delete+create by matching mtime within this drain (best-effort
    // rename detection). Unpaired entries emit as plain delete / create.
    QStringList unpairedCreates = createdRels;
    for (const QString &oldRel : deletedRels) {
        const auto knownIt = m_knownFiles.constFind(oldRel);
        const qint64 oldMtime =
            knownIt != m_knownFiles.cend() ? knownIt.value() : 0;
        bool matched = false;
        for (int i = 0; i < unpairedCreates.size(); ++i) {
            const QString &newRel = unpairedCreates[i];
            const qint64 newMtime = fresh.value(newRel);
            if (oldMtime != 0 && newMtime == oldMtime) {
                Q_EMIT rawChange(newRel);
                if (!isTreeExcluded(oldRel) && !isTreeExcluded(newRel)) {
                    Q_EMIT renamed(oldRel, newRel);
                }
                unpairedCreates.removeAt(i);
                matched = true;
                break;
            }
        }
        if (!matched) {
            Q_EMIT rawChange(oldRel);
            if (!isTreeExcluded(oldRel)) Q_EMIT deleted(oldRel);
        }
    }
    for (const QString &newRel : unpairedCreates) {
        Q_EMIT rawChange(newRel);
        if (!isTreeExcluded(newRel)) Q_EMIT created(newRel);
    }

    // Re-add subdirs + files to the watcher so newly-created entries
    // join the watch list. QFileSystemWatcher dedups addPath internally.
    QDirIterator dit(m_basePath,
                     QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                     QDirIterator::Subdirectories);
    while (dit.hasNext()) {
        const QString d = dit.next();
        const QString rel = toRel(d);
        if (isWatchExcluded(rel)) continue;
        if (!m_fsw.directories().contains(d)) m_fsw.addPath(d);
    }
    for (auto fit = fresh.cbegin(); fit != fresh.cend(); ++fit) {
        const QString abs = m_basePath + QLatin1Char('/') + fit.key();
        if (!m_fsw.files().contains(abs)) m_fsw.addPath(abs);
    }

    m_knownFiles = std::move(fresh);
    m_pending.clear();
}

QString Watcher::toRel(const QString &abs) const
{
    if (m_basePath.isEmpty()) return abs;
    return QDir(m_basePath).relativeFilePath(abs);
}

} // namespace Corbomite::detail
