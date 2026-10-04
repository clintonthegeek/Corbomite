// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/core/DataAdapter.h"  // for Corbomite::FileStat

namespace Corbomite {

/// A file node in the Vault tree. Extends `TAbstractFile` with
/// basename/extension/stat/saving-depth metadata.
class TFile : public TAbstractFile
{
public:
    QString                 basename;         ///< Name without extension.
    QString                 extension;        ///< Lowercase, no leading dot.
    std::optional<FileStat> stat;             ///< Nullopt until first reconcile.

    /// Nesting depth of in-flight writes, not a flag: Obsidian `++`s on entry
    /// to each of its five write paths and `--`s in a `finally`
    /// (`app/own/1129-UD.js:441/451,462/470,481/489,500/508,520/528`), and its
    /// cache-invalidation guard is `saving || …` — i.e. falsy, firing only
    /// once the count is back to 0. A boolean breaks as soon as two writes
    /// overlap on one file, which `Vault::process` re-entrancy (frontmatter
    /// edits nested inside a larger process) makes routine.
    int                     savingDepth = 0;

    TFile(Vault *v, const QString &p);

    void setPath(const QString &newPath) override;

    /// Returns `basename` for `.md` files, else `name`. Used in UI chrome.
    QString getShortName() const;
};

} // namespace Corbomite
