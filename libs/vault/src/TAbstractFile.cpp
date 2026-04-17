// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/TAbstractFile.h"

#include <QFileInfo>
#include <QRegularExpression>

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

    QString cleaned = newName;
    cleaned.remove(QRegularExpression(QStringLiteral("[\\x00-\\x1F]")));
    cleaned = cleaned.trimmed();
    if (cleaned.isEmpty()) return {};

    // Parent-prefix computation deferred to Task 1.4 (requires TFolder
    // complete type). Until then, return empty; parent-backed rename is
    // exercised by tst_vault_tree once TFolder exists.
    return {};
}

} // namespace Corbomite
