// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/DataAdapter.h"

namespace Corbomite {

/// Production `DataAdapter` backed by the native filesystem.
///
/// All writes are atomic via `QSaveFile` (temp + fsync + rename-atop) and
/// optionally stamp an mtime via `WriteHints::mtimeMs`.
class FileSystemAdapter : public DataAdapter
{
public:
    // --- Read + query ---
    bool exists(const QString &absolutePath) const override;
    std::optional<QString> read(const QString &absolutePath) const override;
    std::optional<QByteArray> readBinary(const QString &absolutePath) const override;
    FileStat stat(const QString &absolutePath) const override;
    QStringList list(const QString &dirPath) const override;

    // --- Write (atomic) ---
    bool write(const QString &absolutePath,
               const QString &content,
               const WriteHints &hints = {}) override;
    bool writeBinary(const QString &absolutePath,
                     const QByteArray &content,
                     const WriteHints &hints = {}) override;

    // --- Mutate ---
    bool rename(const QString &fromPath, const QString &toPath) override;
    bool remove(const QString &absolutePath) override;
    bool rmdir(const QString &dirPath) override;
    bool mkpath(const QString &dirPath) override;

    // --- Trash ---
    bool moveToTrash(const QString &absolutePath) override;

    // --- Legacy aliases (existing callers) ---
    std::optional<QString> readFile(const QString &absolutePath) const
    {
        return read(absolutePath);
    }
    bool writeFile(const QString &absolutePath, const QString &content)
    {
        return write(absolutePath, content);
    }
};

} // namespace Corbomite
