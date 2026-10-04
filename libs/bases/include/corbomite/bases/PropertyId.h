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

/// Inverse of parsePropertyId: the canonical, fully-prefixed spelling
/// (`note.status`, `file.name`). This is what gets *stored* and what
/// `properties:` keys become when Corbomite creates them itself.
QString buildPropertyId(const PropertyId &id);

/// The `.base` writer's property-id spelling. Reproduces Obsidian's `hQ`
/// (`RE/obsidian/1.13.7/renamed/src/app/own/1768-hQ.js:2-9`) exactly, and is
/// applied to precisely the four places Obsidian applies it to:
/// `views[].order[]`, `views[].sort[].property`, `views[].groupBy.property`
/// and `views[].summaries` keys (all at `1766-lQ.js:15-26`).
///
/// The rule, and why it is the rule rather than a uniform one:
///
///  * `note.x` → `x`, `file.file` → `file`; `note.note.x`, `formula.x` and
///    `file.name` are left alone.
///  * Every *other* property-id key — the `properties:` map and the free-form
///    per-view `data:` block (which carries the table's `columnSize` map) —
///    is written **verbatim, as the author typed it** (`1766-lQ.js:223-227`
///    and `1972-Z5.js:1021-1029`; both hold keys Obsidian `pQ`-normalised or
///    never normalised, and neither runs `hQ`).
///
/// A uniform convention is not reachable without breaking Obsidian compat:
/// Obsidian itself writes `columnSize` keys in the canonical `note.x` form
/// while stripping `note.` from `order` in the same file, so a single spelling
/// per property cannot survive a hand-off between the two apps. Matching `hQ`
/// is the only choice that makes every key we rewrite a **fixed point of
/// Obsidian's writer** — emit `note.x` in `order:` instead and Obsidian
/// rewrites it to `x:` on its next save, so the two apps would flip the file
/// back and forth forever.
QString serializePropertyId(const PropertyId &id);

/// Audit §2: the 14 built-in file.* members.
const QStringList &filePropertyMembers();

}  // namespace Corbomite::Bases
