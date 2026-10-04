// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFolder.h"

#include <QFileInfo>

namespace Corbomite {

TAbstractFile::TAbstractFile(Vault *v, QString p)
    : path(std::move(p))
    , m_vault(v)
{
    name = QFileInfo(path).fileName();
}

void TAbstractFile::setPath(const QString &newPath)
{
    path = newPath;
    name = QFileInfo(path).fileName();
}

QString TAbstractFile::getNewPathAfterRename(const QString &newName) const
{
    if (!parent) return {};

    // Obsidian replaces control chars with a space and then trims
    // (app/own/1129-UD.js:9-10); stripping would fuse `a\tb` into `ab`.
    QString cleaned;
    cleaned.reserve(newName.size());
    for (QChar ch : newName) {
        cleaned.append(ch.unicode() < 0x20 ? QLatin1Char(' ') : ch);
    }
    cleaned = cleaned.trimmed();
    if (cleaned.isEmpty()) return {};

    return parent->getParentPrefix() + cleaned;
}

} // namespace Corbomite
