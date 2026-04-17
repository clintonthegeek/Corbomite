// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/vault/TFolder.h"
#include "corbomite/vault/TFile.h"

namespace Corbomite {

TFolder::TFolder(Vault *v, const QString &p)
    : TAbstractFile(v, p)
{
}

QString TFolder::getParentPrefix() const
{
    return isRoot() ? QString() : (path + QLatin1Char('/'));
}

int TFolder::getFileCount() const
{
    int n = 0;
    for (const TAbstractFile *c : children) {
        if (const auto *sub = dynamic_cast<const TFolder *>(c)) {
            n += sub->getFileCount();
        } else if (dynamic_cast<const TFile *>(c)) {
            ++n;
        }
    }
    return n;
}

int TFolder::getFolderCount() const
{
    int n = 0;
    for (const TAbstractFile *c : children) {
        if (const auto *sub = dynamic_cast<const TFolder *>(c)) {
            ++n;
            n += sub->getFolderCount();
        }
    }
    return n;
}

} // namespace Corbomite
