// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>

namespace Corbomite {

/// Obsidian spec §3: `Vault.read` strips a leading UTF-8 BOM (U+FEFF,
/// EF BB BF) before returning; `readBinary` preserves the bytes verbatim.
///
/// One helper, because the rule has to hold on *every* path that reads a
/// note's bytes, and it used to be carried as two identical private copies:
/// `Vault::read` and `MetadataCache::rebuildVault` each had their own
/// three lines. A BOM'd note must hash the same whichever path read it —
/// otherwise the index and the reader disagree about the content and the
/// note re-parses for ever.
///
/// This works on *bytes*, before decoding. `QString::fromUtf8` also drops
/// a leading BOM, so a path that decodes straight to a string needs no
/// call; this is for the paths that need the bytes.
QByteArray stripUtf8Bom(QByteArray bytes);

} // namespace Corbomite
