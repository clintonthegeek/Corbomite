// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

#include "corbomite/storage/CachedMetadata.h"

namespace Corbomite {

class LinkResolver; // forward decl from libs/storage/include/corbomite/storage/LinkResolver.h

/// Result of parsing a single note.
struct ParsedNote
{
    /// 64-char lowercase hex SHA-256 of the UTF-8 content.
    QString hash;
    CachedMetadata cache;
};

/// Parses a single note into `CachedMetadata`.
///
/// Pure function — no state, no I/O, no threading. Safe to call on any
/// thread provided `resolver` is not mutated concurrently.
class MetadataParser
{
public:
    static ParsedNote parse(const QByteArray &content,
                            const QString &path,
                            const LinkResolver &resolver);
};

} // namespace Corbomite
