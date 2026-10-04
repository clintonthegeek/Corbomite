// SPDX-License-Identifier: GPL-3.0-or-later
#include "Watcher.h"

#include "HiddenPaths.h"
#include "corbomite/vault/Vault.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <iterator>

namespace Corbomite::detail {

namespace {
// Obsidian hands a watcher-reported ENOENT a grace period before it removes
// the node: the first miss schedules one more full reconcile, and only that
// second pass runs delete-authorised (`app/own/0376-_l.js:829-833`, reached
// from `onFileChange` → `reconcileFile(real, path, false)` at `:667-670`;
// adapter-initiated misses pass the default `true` and delete at once).
// Write-to-temp-then-rename — what `FileSystemAdapter`'s QSaveFile writes are,
// and what most editors and sync clients do — produces exactly the ENOENT
// blip that a delete-on-first-miss reconciler turns into a phantom
// delete/create pair on every save.
constexpr int kEnoentGraceMs = 100;
}

Watcher::Watcher(Vault *vault, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
{
    m_drainTimer.setSingleShot(true);
    m_drainTimer.setInterval(50);
    connect(&m_drainTimer, &QTimer::timeout, this, &Watcher::drainPending);
    m_graceTimer.setSingleShot(true);
    connect(&m_graceTimer, &QTimer::timeout, this, &Watcher::drainPending);
    connect(&m_fsw, &QFileSystemWatcher::directoryChanged,
            this, &Watcher::onDirChanged);
    connect(&m_fsw, &QFileSystemWatcher::fileChanged,
            this, &Watcher::onFileChanged);
}

Watcher::~Watcher() = default;

namespace {
// Tree-visible exclusion (skipped from created/modified/deleted/renamed
// signals). Any dot-segment hides a path, with no configDir exemption —
// `.obsidian/` is hidden for the same reason `.git/` is. `.obsidian/` is
// excluded from the tree but NOT from the rawChange signal — Cluster B
// (Vault.raw + configChanged) consumes it for plugin event surface.
bool isTreeExcluded(const QString &rel)
{
    return VaultPaths::isHiddenPath(rel);
}

// Watcher-coverage exclusion (paths we don't bother monitoring at all).
// This is about inotify/watch-descriptor cost, not tree visibility, so it
// stays a literal list rather than the dot-segment predicate: `.obsidian/`
// IS monitored — plugin config is observable via rawChange.
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

    m_graceClock.start();
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
    m_pendingDelete.clear();
    m_pending.clear();
    m_drainTimer.stop();
    m_graceTimer.stop();
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
    // dot-segment paths; the rawChange signal fires for every change.
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

    // A path still inside its ENOENT grace reappeared, so it was a touch, not
    // a delete-then-create: forget the pending entry before anything below
    // can pair it against a create and emit a rename.
    for (auto it = m_pendingDelete.begin(); it != m_pendingDelete.end();) {
        it = fresh.contains(it.key()) ? m_pendingDelete.erase(it)
                                      : std::next(it);
    }

    // Pair delete+create by matching mtime within this drain (best-effort
    // rename detection). Unpaired entries go to the ENOENT grace queue.
    //
    // A folder rename does not touch its files' mtimes (only the parent
    // directory entry moves), so renaming a folder with 2+ files that
    // happen to share an mtime — extremely common, since mtime is only
    // millisecond-granular here and files created back-to-back (e.g. a
    // bulk import, or two files touched in the same test/script) collide
    // easily — makes mtime alone ambiguous: multiple deleted paths tie
    // against multiple created paths, and picking "first available" can
    // cross-wire unrelated files (A's old path pairs with B's new path).
    // Disambiguate by basename first — a file that's genuinely just moved
    // (not renamed) keeps its filename — and only fall back to the first
    // mtime-only match when no basename match exists (an actual rename,
    // where old and new basenames legitimately differ).
    QStringList unpairedCreates = createdRels;
    QStringList unpairedDeletes;
    for (const QString &oldRel : deletedRels) {
        const auto knownIt = m_knownFiles.constFind(oldRel);
        const qint64 oldMtime =
            knownIt != m_knownFiles.cend() ? knownIt.value() : 0;
        const QString oldBasename = QFileInfo(oldRel).fileName();
        int matchIndex = -1;
        if (oldMtime != 0) {
            for (int i = 0; i < unpairedCreates.size(); ++i) {
                const QString &newRel = unpairedCreates[i];
                if (fresh.value(newRel) != oldMtime) continue;
                if (matchIndex < 0) matchIndex = i;
                if (QFileInfo(newRel).fileName() == oldBasename) {
                    matchIndex = i;
                    break;
                }
            }
        }
        if (matchIndex >= 0) {
            const QString newRel = unpairedCreates[matchIndex];
            Q_EMIT rawChange(newRel);
            if (!isTreeExcluded(oldRel) && !isTreeExcluded(newRel)) {
                Q_EMIT renamed(oldRel, newRel);
            }
            unpairedCreates.removeAt(matchIndex);
            m_pendingDelete.remove(oldRel);
        } else {
            unpairedDeletes.append(oldRel);
        }
    }

    // ENOENT grace: a first miss only arms the clock. The deletion is emitted
    // by whichever drain first runs with the path still gone after
    // kEnoentGraceMs has elapsed since the miss was recorded.
    const qint64 now = m_graceClock.isValid() ? m_graceClock.elapsed() : 0;
    int recheckIn = -1;
    const auto arm = [&recheckIn](int ms) {
        recheckIn = (recheckIn < 0) ? ms : qMin(recheckIn, ms);
    };
    for (const QString &rel : unpairedDeletes) {
        auto it = m_pendingDelete.find(rel);
        if (it == m_pendingDelete.end()) {
            m_pendingDelete.insert(rel, now);
            arm(kEnoentGraceMs);
            continue;
        }
        const int remaining =
            static_cast<int>(kEnoentGraceMs - (now - it.value()));
        if (remaining > 0) {
            arm(remaining);
            continue;
        }
        m_pendingDelete.erase(it);
        Q_EMIT rawChange(rel);
        if (!isTreeExcluded(rel)) Q_EMIT deleted(rel);
    }
    if (recheckIn > 0) m_graceTimer.start(recheckIn);

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

    // Keep a gracing path in m_knownFiles, at its pre-ENOENT mtime: that is
    // what makes a reappearance inside the window diff as `modified` (the
    // atomic-save case) rather than as a create paired against a delete.
    QHash<QString, qint64> retained;
    for (auto it = m_pendingDelete.cbegin(); it != m_pendingDelete.cend(); ++it) {
        retained.insert(it.key(), m_knownFiles.value(it.key()));
    }
    m_knownFiles = std::move(fresh);
    for (auto it = retained.cbegin(); it != retained.cend(); ++it) {
        m_knownFiles.insert(it.key(), it.value());
    }
    m_pending.clear();
}

QString Watcher::toRel(const QString &abs) const
{
    if (m_basePath.isEmpty()) return abs;
    return QDir(m_basePath).relativeFilePath(abs);
}

} // namespace Corbomite::detail
