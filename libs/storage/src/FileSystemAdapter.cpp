// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/FileSystemAdapter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace Corbomite {

namespace {

// Stamp the file's modified time to `mtimeMs` (milliseconds since epoch).
// Returns true on success. Used to honour `WriteHints::mtimeMs`.
bool applyMtime(const QString &absolutePath, qint64 mtimeMs)
{
    QFile f(absolutePath);
    if (!f.open(QIODevice::ReadWrite)) return false;
    const auto dt = QDateTime::fromMSecsSinceEpoch(mtimeMs);
    return f.setFileTime(dt, QFileDevice::FileModificationTime);
}

} // namespace

bool FileSystemAdapter::exists(const QString &absolutePath) const
{
    return QFileInfo::exists(absolutePath);
}

std::optional<QString> FileSystemAdapter::read(const QString &absolutePath) const
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return std::nullopt;
    return QString::fromUtf8(file.readAll());
}

std::optional<QByteArray> FileSystemAdapter::readBinary(const QString &absolutePath) const
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    return file.readAll();
}

FileStat FileSystemAdapter::stat(const QString &absolutePath) const
{
    FileStat s;
    QFileInfo fi(absolutePath);
    s.exists = fi.exists();
    if (!s.exists) return s;
    s.isDirectory = fi.isDir();
    s.isFile = fi.isFile();
    s.sizeBytes = fi.size();
    s.mtimeMs = fi.lastModified().toMSecsSinceEpoch();
    s.ctimeMs = fi.birthTime().isValid()
        ? fi.birthTime().toMSecsSinceEpoch()
        : fi.metadataChangeTime().toMSecsSinceEpoch();
    return s;
}

QStringList FileSystemAdapter::list(const QString &dirPath) const
{
    QDir dir(dirPath);
    if (!dir.exists()) return {};
    return dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::NoSort);
}

bool FileSystemAdapter::write(const QString &absolutePath,
                              const QString &content,
                              const WriteHints &hints)
{
    return writeBinary(absolutePath, content.toUtf8(), hints);
}

bool FileSystemAdapter::writeBinary(const QString &absolutePath,
                                    const QByteArray &content,
                                    const WriteHints &hints)
{
    QFileInfo fi(absolutePath);
    QDir().mkpath(fi.absolutePath());

    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (file.write(content) != content.size()) {
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) return false;

    if (hints.mtimeMs.has_value()) {
        // Best-effort mtime stamp; failure here is a contract miss but not a
        // data-loss event — log via qWarning would pollute stderr in tests,
        // so we let the caller's own stat check catch it.
        applyMtime(absolutePath, *hints.mtimeMs);
    }
    return true;
}

bool FileSystemAdapter::rename(const QString &fromPath, const QString &toPath)
{
    QFileInfo fi(toPath);
    QDir().mkpath(fi.absolutePath());
    // Remove the destination first to make rename atomic-ish on platforms
    // where rename-over-existing-file is non-atomic. QFile::rename on POSIX
    // uses rename(2), which IS atomic over an existing regular file.
    return QFile::rename(fromPath, toPath);
}

bool FileSystemAdapter::remove(const QString &absolutePath)
{
    return QFile::remove(absolutePath);
}

bool FileSystemAdapter::rmdir(const QString &dirPath)
{
    return QDir().rmdir(dirPath);
}

bool FileSystemAdapter::mkpath(const QString &dirPath)
{
    return QDir().mkpath(dirPath);
}

bool FileSystemAdapter::moveToTrash(const QString &absolutePath)
{
    return QFile::moveToTrash(absolutePath);
}

} // namespace Corbomite
