// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace Corbomite::Bases {

enum class PropertyKind
{
    Note,     ///< `note.<key>` — frontmatter property (case-insensitive).
    File,     ///< `file.<member>` — one of 14 built-in file metadata names.
    Formula,  ///< `formula.<name>` — user-defined formula result.
};

struct PropertyId
{
    PropertyKind kind = PropertyKind::Note;
    QString name;

    bool operator==(const PropertyId &o) const noexcept
    {
        return kind == o.kind && name == o.name;
    }

    QString toString() const;  // e.g. "note.status", "file.name"
};

inline size_t qHash(const PropertyId &p, size_t seed = 0) noexcept
{
    return qHash(p.name, seed) ^ static_cast<size_t>(p.kind);
}

/// Parse a property-id string. Unprefixed → Note (per audit §11:
/// "status:" unprefixed → `note.status`).
PropertyId parsePropertyId(const QString &s);

/// Inverse of parsePropertyId.
QString buildPropertyId(const PropertyId &id);

/// Audit §2: the 14 built-in file.* members.
const QStringList &filePropertyMembers();

}  // namespace Corbomite::Bases
