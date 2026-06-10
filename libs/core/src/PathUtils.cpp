// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/PathUtils.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

namespace Corbomite::PathUtils {

namespace {

QString buildUrl(const QString &scheme,
                 const QString &vaultName,
                 const QString &relativePath,
                 const QString &subpath)
{
    const QString vault = QString::fromUtf8(
        QUrl::toPercentEncoding(vaultName));
    // `file=` is the concat of path + subpath, encoded as a single unit so
    // a literal '#' in subpath becomes '%23' in the URL and doesn't get
    // mis-parsed as a URL fragment.
    const QString file = QString::fromUtf8(
        QUrl::toPercentEncoding(relativePath + subpath));
    return QStringLiteral("%1://open?vault=%2&file=%3")
        .arg(scheme, vault, file);
}

}  // namespace

QString obsidianUrlFor(const QString &vaultName,
                       const QString &relativePath,
                       const QString &subpath)
{
    return buildUrl(QStringLiteral("obsidian"),
                    vaultName, relativePath, subpath);
}

QString corbomiteUrlFor(const QString &vaultName,
                        const QString &relativePath,
                        const QString &subpath)
{
    return buildUrl(QStringLiteral("corbomite"),
                    vaultName, relativePath, subpath);
}

QString vaultId(const QString &vaultRoot)
{
    if (vaultRoot.isEmpty()) return {};
    const QByteArray vaultHash =
        QCryptographicHash::hash(vaultRoot.toUtf8(), QCryptographicHash::Sha256)
            .toHex().left(12);
    return QFileInfo(vaultRoot).fileName() + QLatin1Char('-')
           + QString::fromLatin1(vaultHash);
}

QString vaultLocalDataDir(const QString &vaultRoot)
{
    const QString id = vaultId(vaultRoot);
    if (id.isEmpty()) return {};
    const QString dataRoot =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataRoot.isEmpty()) return {};
    return dataRoot + QStringLiteral("/index/") + id;
}

}  // namespace Corbomite::PathUtils
