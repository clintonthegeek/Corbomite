// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/VaultScanner.h"
#include <QDirIterator>

namespace Corbomite {

QVector<NoteMeta> VaultScanner::scan(const QString &vaultRoot) const
{
    QVector<NoteMeta> results;

    QDirIterator it(vaultRoot, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();

        // Check if any parent directory should be excluded
        QString relPath = QDir(vaultRoot).relativeFilePath(fi.absoluteFilePath());
        bool excluded = false;
        const auto parts = relPath.split(QLatin1Char('/'));
        for (const auto &part : parts) {
            if (shouldExcludeDir(part)) {
                excluded = true;
                break;
            }
        }
        if (excluded) {
            continue;
        }

        if (!isNoteFile(fi.suffix())) {
            continue;
        }

        results.append(NoteMeta::fromFileInfo(fi, vaultRoot));
    }

    return results;
}

bool VaultScanner::shouldExcludeDir(const QString &dirName) const
{
    return dirName == QLatin1String(".obsidian")
        || dirName == QLatin1String(".corbomite")
        || dirName == QLatin1String(".git")
        || dirName == QLatin1String(".trash")
        || dirName == QLatin1String("node_modules");
}

bool VaultScanner::isNoteFile(const QString &suffix) const
{
    return suffix == QLatin1String("md") || suffix == QLatin1String("canvas");
}

} // namespace Corbomite
