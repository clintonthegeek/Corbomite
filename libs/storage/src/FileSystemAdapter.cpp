// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/FileSystemAdapter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace Corbomite {

std::optional<QString> FileSystemAdapter::readFile(const QString &absolutePath) const
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    return QString::fromUtf8(file.readAll());
}

bool FileSystemAdapter::writeFile(const QString &absolutePath, const QString &content)
{
    // Ensure parent directory exists
    QFileInfo fi(absolutePath);
    QDir().mkpath(fi.absolutePath());

    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(content.toUtf8());
    return file.commit();
}

bool FileSystemAdapter::rename(const QString &oldPath, const QString &newPath)
{
    // Ensure target directory exists
    QFileInfo fi(newPath);
    QDir().mkpath(fi.absolutePath());

    return QFile::rename(oldPath, newPath);
}

bool FileSystemAdapter::remove(const QString &absolutePath)
{
    return QFile::remove(absolutePath);
}

bool FileSystemAdapter::moveToTrash(const QString &absolutePath)
{
    return QFile::moveToTrash(absolutePath);
}

bool FileSystemAdapter::exists(const QString &absolutePath) const
{
    return QFileInfo::exists(absolutePath);
}

bool FileSystemAdapter::mkpath(const QString &dirPath)
{
    return QDir().mkpath(dirPath);
}

} // namespace Corbomite
