// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite::PathUtils {

/// Emits an `obsidian://open?vault=<vault>&file=<path>` URL.
///
/// `vaultName` is typically the vault's folder basename. `subpath` is an
/// optional `#heading` or `#^block` suffix that becomes part of the
/// `file=` query value (percent-encoded as a unit). Empty subpath omits
/// the `#...` portion.
QString obsidianUrlFor(const QString &vaultName,
                       const QString &relativePath,
                       const QString &subpath = QString());

/// Corbomite-native variant (same shape, `corbomite://` scheme).
///
/// Used alongside `obsidianUrlFor` for interop: users pasting into
/// Corbomite get direct open; Obsidian users paste the obsidian:// one.
QString corbomiteUrlFor(const QString &vaultName,
                        const QString &relativePath,
                        const QString &subpath = QString());

}  // namespace Corbomite::PathUtils
