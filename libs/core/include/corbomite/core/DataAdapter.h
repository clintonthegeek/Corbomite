// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Corbomite {

/// Stat-like snapshot returned by `DataAdapter::stat`.
struct FileStat {
    bool exists = false;
    bool isDirectory = false;
    bool isFile = false;
    qint64 sizeBytes = 0;
    /// Milliseconds since the Unix epoch; 0 when unknown.
    qint64 mtimeMs = 0;
    qint64 ctimeMs = 0;
};

/// Optional hints passed to write operations.
struct WriteHints {
    /// If set, the adapter stamps the file's mtime to this value after a
    /// successful commit. Mirrors Obsidian's mtime-hint contract used by the
    /// external-edit watcher to no-op on a self-write echo.
    std::optional<qint64> mtimeMs;
};

/// Abstract filesystem surface used across Corbomite. Implementations must
/// provide atomic writes (temp + fsync + rename-atop); callers depend on
/// partial failures never producing a truncated file.
///
/// The concrete production implementation is `FileSystemAdapter`. Tests may
/// substitute an in-memory implementation.
class DataAdapter
{
public:
    virtual ~DataAdapter() = default;

    // --- Read + query ---

    virtual bool exists(const QString &absolutePath) const = 0;

    virtual std::optional<QString> read(const QString &absolutePath) const = 0;

    virtual std::optional<QByteArray> readBinary(const QString &absolutePath) const = 0;

    virtual FileStat stat(const QString &absolutePath) const = 0;

    /// Direct entries (basenames, not absolute paths) in `dirPath`. Returns
    /// empty on non-directory or read failure.
    virtual QStringList list(const QString &dirPath) const = 0;

    // --- Write (atomic) ---

    virtual bool write(const QString &absolutePath,
                       const QString &content,
                       const WriteHints &hints = {}) = 0;

    virtual bool writeBinary(const QString &absolutePath,
                             const QByteArray &content,
                             const WriteHints &hints = {}) = 0;

    // --- Mutate ---

    virtual bool rename(const QString &fromPath, const QString &toPath) = 0;

    virtual bool remove(const QString &absolutePath) = 0;

    virtual bool rmdir(const QString &dirPath) = 0;

    virtual bool mkpath(const QString &dirPath) = 0;

    // --- Trash ---

    virtual bool moveToTrash(const QString &absolutePath) = 0;
};

} // namespace Corbomite
