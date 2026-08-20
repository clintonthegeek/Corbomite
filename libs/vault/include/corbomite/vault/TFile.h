// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/core/DataAdapter.h"  // for Corbomite::FileStat

namespace Corbomite {

/// A file node in the Vault tree. Extends `TAbstractFile` with
/// basename/extension/stat/saving metadata.
class TFile : public TAbstractFile
{
public:
    QString                 basename;         ///< Name without extension.
    QString                 extension;        ///< Lowercase, no leading dot.
    std::optional<FileStat> stat;             ///< Nullopt until first reconcile.
    bool                    saving = false;   ///< Set during in-flight mutations.

    TFile(Vault *v, const QString &p);

    void setPath(const QString &newPath) override;

    /// Returns `basename` for `.md` files, else `name`. Used in UI chrome.
    QString getShortName() const;
};

} // namespace Corbomite
