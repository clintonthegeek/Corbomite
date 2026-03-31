// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileWatchReactor.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/storage/VaultScanner.h"
#include <QDir>
#include <QDirIterator>

namespace Corbomite {

FileWatchReactor::FileWatchReactor(VaultModel *vault, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
{
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(500);
    connect(&m_debounceTimer, &QTimer::timeout, this, &FileWatchReactor::processPendingChanges);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &FileWatchReactor::onDirectoryChanged);
}

void FileWatchReactor::startWatching(const QString &vaultRoot)
{
    stopWatching();
    m_vaultRoot = vaultRoot;

    // Watch vault root and all subdirectories
    QDirIterator it(vaultRoot, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    QStringList dirs;
    dirs << vaultRoot;
    while (it.hasNext()) {
        it.next();
        if (!shouldExclude(it.fileName())) {
            dirs << it.filePath();
        }
    }
    m_watcher.addPaths(dirs);

    // Build initial file set
    m_knownFiles.clear();
    for (const auto &note : m_vault->allNotes()) {
        m_knownFiles.insert(note.relativePath);
    }
}

void FileWatchReactor::stopWatching()
{
    auto dirs = m_watcher.directories();
    if (!dirs.isEmpty()) m_watcher.removePaths(dirs);
    auto files = m_watcher.files();
    if (!files.isEmpty()) m_watcher.removePaths(files);
    m_pendingDirs.clear();
    m_knownFiles.clear();
}

void FileWatchReactor::suppressPath(const QString &absolutePath)
{
    m_suppressedPaths.insert(absolutePath);
    // Clear after debounce period
    QTimer::singleShot(1000, this, [this, absolutePath]() {
        m_suppressedPaths.remove(absolutePath);
    });
}

void FileWatchReactor::onDirectoryChanged(const QString &path)
{
    m_pendingDirs.insert(path);
    m_debounceTimer.start();
}

void FileWatchReactor::processPendingChanges()
{
    VaultScanner scanner;
    auto currentNotes = scanner.scan(m_vaultRoot);
    QSet<QString> currentPaths;

    for (const auto &note : currentNotes) {
        currentPaths.insert(note.relativePath);

        if (!m_knownFiles.contains(note.relativePath)) {
            // New file
            QString absPath = m_vaultRoot + QLatin1Char('/') + note.relativePath;
            if (!m_suppressedPaths.contains(absPath)) {
                m_vault->addNote(note.relativePath);
                Q_EMIT fileCreatedExternally(note.relativePath);
            }
        }
    }

    // Check for deleted files
    for (const auto &known : m_knownFiles) {
        if (!currentPaths.contains(known)) {
            QString absPath = m_vaultRoot + QLatin1Char('/') + known;
            if (!m_suppressedPaths.contains(absPath)) {
                m_vault->removeNote(known);
                Q_EMIT fileDeletedExternally(known);
            }
        }
    }

    m_knownFiles = currentPaths;
    m_pendingDirs.clear();
}

bool FileWatchReactor::shouldExclude(const QString &name) const
{
    return name == QLatin1String(".obsidian")
        || name == QLatin1String(".corbomite")
        || name == QLatin1String(".git")
        || name == QLatin1String(".trash")
        || name == QLatin1String("node_modules");
}

} // namespace Corbomite
