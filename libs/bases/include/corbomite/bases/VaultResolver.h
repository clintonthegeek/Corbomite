// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ValuePtr.h"

#include <QString>

namespace Corbomite::Bases {

/// Narrow vault-access seam for vault-bound builtins (file(), asFile,
/// linksTo). Implemented by BasesVaultResolver; exposed via
/// EvalContext::vault(). Contexts without a vault return nullptr from
/// vault() and builtins fall back to non-vault behaviour.
class VaultResolver
{
public:
    virtual ~VaultResolver() = default;

    /// Resolve a vault-relative path (or bare/short name) to a FileValue;
    /// returns NullValue::instance() when nothing resolves.
    virtual ValuePtr fileAt(const QString &pathOrName) const = 0;

    /// Resolve a wiki/markdown link's target text to a canonical
    /// vault-relative path (Obsidian getLinkpathDest). Empty if unresolved.
    /// `sourcePath` is the link's origin note (for relative/short resolution).
    virtual QString resolveLinkTarget(const QString &linkData,
                                      const QString &sourcePath) const = 0;
};

}  // namespace Corbomite::Bases
