// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite::VaultPaths {

/// NFC-normalize + collapse backslashes to forward + collapse consecutive
/// slashes + trim trailing slash (except at root). Mirrors Obsidian's
/// normalizePath. Namespace is `VaultPaths` rather than `Vault::Paths`
/// to avoid collision with the `Corbomite::Vault` class.
QString normalize(const QString &input);

} // namespace Corbomite::VaultPaths
