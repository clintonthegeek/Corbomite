// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/VaultTrash.h"

#include "corbomite/storage/DataAdapter.h"

#include <QFileInfo>

namespace Corbomite {

namespace {

// Split a basename into (stem, extension). Extension includes the leading '.'.
// "doomed.md" → ("doomed", ".md"); "no-ext" → ("no-ext", "").
std::pair<QString, QString> splitExt(const QString &basename)
{
    const int dot = basename.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0) return {basename, QString()};
    return {basename.left(dot), basename.mid(dot)};
}

QString joinVaultPath(const QString &vaultRoot, const QString &rel)
{
    QString base = vaultRoot;
    if (base.endsWith(QLatin1Char('/'))) base.chop(1);
    return base + QLatin1Char('/') + rel;
}

} // namespace

VaultTrash::VaultTrash(DataAdapter *fs, const QString &vaultRoot)
    : m_fs(fs), m_vaultRoot(vaultRoot)
{
}

QString VaultTrash::trashDir() const
{
    QString base = m_vaultRoot;
    if (base.endsWith(QLatin1Char('/'))) base.chop(1);
    return base + QStringLiteral("/.trash");
}

QString VaultTrash::moveToTrash(const QString &relativePath)
{
    if (!m_fs || relativePath.isEmpty()) return {};

    const QString source = joinVaultPath(m_vaultRoot, relativePath);
    if (!m_fs->exists(source)) return {};

    if (!m_fs->mkpath(trashDir())) return {};

    const QFileInfo fi(source);
    const QString basename = fi.fileName();
    const auto [stem, ext] = splitExt(basename);

    // Find an available target name — first try "<basename>", then
    // "<stem> 2<ext>", "<stem> 3<ext>", …
    QString targetName = basename;
    QString targetPath = trashDir() + QLatin1Char('/') + targetName;
    int counter = 2;
    while (m_fs->exists(targetPath)) {
        targetName = stem + QStringLiteral(" ") + QString::number(counter) + ext;
        targetPath = trashDir() + QLatin1Char('/') + targetName;
        ++counter;
        if (counter > 10000) return {}; // sanity
    }

    if (!m_fs->rename(source, targetPath)) return {};
    return targetPath;
}

} // namespace Corbomite
