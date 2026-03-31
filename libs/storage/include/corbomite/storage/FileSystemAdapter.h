// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <optional>

namespace Corbomite {

class FileSystemAdapter {
public:
    std::optional<QString> readFile(const QString &absolutePath) const;
    bool writeFile(const QString &absolutePath, const QString &content);
    bool rename(const QString &oldPath, const QString &newPath);
    bool remove(const QString &absolutePath);
    bool moveToTrash(const QString &absolutePath);
    bool exists(const QString &absolutePath) const;
    bool mkpath(const QString &dirPath);
};

} // namespace Corbomite
