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

    QString cleaned;
    cleaned.reserve(newName.size());
    for (QChar ch : newName) {
        if (ch.unicode() >= 0x20) cleaned.append(ch);
    }
    cleaned = cleaned.trimmed();
    if (cleaned.isEmpty()) return {};

    return parent->getParentPrefix() + cleaned;
}

} // namespace Corbomite
