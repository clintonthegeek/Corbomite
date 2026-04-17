// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>

#include "corbomite/vault/TAbstractFile.h"

namespace Corbomite {

/// A folder node in the Vault tree.
class TFolder : public TAbstractFile
{
public:
    QList<TAbstractFile *> children;  ///< Non-owning; Vault owns entries.

    TFolder(Vault *v, const QString &p);

    bool    isRoot() const { return path == QStringLiteral("/"); }
    QString getParentPrefix() const;  ///< "" for root, else path+"/".

    int getFileCount() const;          ///< Recursive count of TFile descendants.
    int getFolderCount() const;        ///< Recursive count of TFolder descendants.
};

} // namespace Corbomite
