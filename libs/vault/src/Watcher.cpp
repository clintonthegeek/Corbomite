// SPDX-License-Identifier: GPL-3.0-or-later
#include "Watcher.h"

#include "corbomite/vault/Vault.h"

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

// Task 2.2 ships the skeleton only; Task 2.4 fills these in.
void Watcher::start(const QString &basePath)
{
    stop();
    m_basePath = basePath;
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

void Watcher::drainPending()
{
    // Task 2.4: implement directory-diff + signal emission.
    m_pending.clear();
}

void Watcher::snapshotDirectory(const QString & /*absDir*/)
{
    // Task 2.4: populate m_knownFiles with (relPath, mtime) pairs.
}

QString Watcher::toRel(const QString &abs) const
{
    if (m_basePath.isEmpty()) return abs;
    if (abs.startsWith(m_basePath)) {
        return abs.mid(m_basePath.size() + 1);
    }
    return abs;
}

} // namespace Corbomite::detail
