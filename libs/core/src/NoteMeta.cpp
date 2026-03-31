// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/NoteMeta.h"
#include <QDir>

namespace Corbomite {

QString NoteMeta::nameFromPath() const
{
    QString fileName = relativePath.mid(relativePath.lastIndexOf(QLatin1Char('/')) + 1);
    int dotPos = fileName.lastIndexOf(QLatin1Char('.'));
    if (dotPos > 0) {
        return fileName.left(dotPos);
    }
    return fileName;
}

QString NoteMeta::absolutePath(const QString &vaultRoot) const
{
    return vaultRoot + QLatin1Char('/') + relativePath;
}

NoteMeta NoteMeta::fromFileInfo(const QFileInfo &fi, const QString &vaultRoot)
{
    NoteMeta meta;
    QDir vault(vaultRoot);
    meta.relativePath = vault.relativeFilePath(fi.absoluteFilePath());
    meta.relativePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    meta.modified = fi.lastModified();
    meta.sizeBytes = fi.size();
    return meta;
}

NoteMeta NoteMeta::fromRelativePath(const QString &path)
{
    NoteMeta meta;
    meta.relativePath = path;
    meta.relativePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (meta.relativePath.startsWith(QLatin1Char('/'))) {
        meta.relativePath = meta.relativePath.mid(1);
    }
    return meta;
}

} // namespace Corbomite
