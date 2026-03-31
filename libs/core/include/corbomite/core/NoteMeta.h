// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QFileInfo>
#include <QString>

namespace Corbomite {

struct NoteMeta {
    QString relativePath;
    QDateTime modified;
    qint64 sizeBytes = 0;

    QString nameFromPath() const;
    QString absolutePath(const QString &vaultRoot) const;

    static NoteMeta fromFileInfo(const QFileInfo &fi, const QString &vaultRoot);
    static NoteMeta fromRelativePath(const QString &path);
};

} // namespace Corbomite
