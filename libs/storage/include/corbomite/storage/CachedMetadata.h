// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Corbomite {

/// Byte/line/column position inside a markdown source buffer. `line` and
/// `col` are 0-based; `offset` is the byte offset into the source.
struct Pos
{
    int line = 0;
    int col = 0;
    int offset = 0;
};

inline bool operator==(const Pos &a, const Pos &b)
{
    return a.line == b.line && a.col == b.col && a.offset == b.offset;
}
inline bool operator!=(const Pos &a, const Pos &b) { return !(a == b); }

/// A (start, end) span in the source buffer.
struct Position
{
    Pos start;
    Pos end;
};

inline bool operator==(const Position &a, const Position &b)
{
    return a.start == b.start && a.end == b.end;
}
inline bool operator!=(const Position &a, const Position &b) { return !(a == b); }

/// Cached wikilink/markdown-link entry. `link` is the normalised link
/// target; `original` is the verbatim source substring; `displayText` is
/// the optional alias text (`[[link|display]]`).
struct LinkCache
{
    QString link;
    QString original;
    std::optional<QString> displayText;
    Position position;
};

/// Cached tag entry. The `tag` field includes the leading `#`.
struct TagCache
{
    QString tag;
    Position position;
};

/// Cached heading entry. `level` is 1..6.
struct HeadingCache
{
    QString heading;
    int level = 1;
    Position position;
};

/// Cached structural section (a top-level block in the markdown).
struct SectionCache
{
    enum class Type
    {
        Paragraph,
        Heading,
        List,
        Code,
        Callout,
        Yaml,
        Table,
        Math,
        Html,
        Blockquote,
        ThematicBreak,
        Unknown
    };

    Type type = Type::Unknown;
    /// Raw type string preserved for forward-compat. Non-empty iff
    /// `type == Unknown` or when the canonical enum value differs from
    /// the raw string.
    QString rawType;
    Position position;
    /// Block-id without the leading `^`.
    std::optional<QString> id;
};

/// Cached list item. `parent` is an index into the parent list's
/// `listItems` vector, or `-1` for top-level. `task` is a GFM task
/// marker character (" ", "x", "-", etc.) when the item is a task.
struct ListItemCache
{
    Position position;
    int parent = -1;
    std::optional<QString> task;
    std::optional<QString> id;
};

/// Cached footnote. Used for both footnote *definitions* (`footnotes`)
/// and inline *references* (`footnoteRefs`).
struct FootnoteCache
{
    QString id;
    Position position;
};

/// Cached block reference. `id` is the blockId with the leading `^`
/// stripped.
struct BlockCache
{
    QString id;
    Position position;
};

/// Link discovered inside YAML/JSON frontmatter. `key` is the dotted
/// path to the value in the frontmatter object (e.g. `"project"` or
/// `"related.0"`).
struct FrontmatterLinkCache
{
    QString link;
    QString original;
    std::optional<QString> displayText;
    QString key;
};

/// Mirrors Obsidian's `CachedMetadata` shape. Every field is optional;
/// absent fields are omitted from the JSON output entirely.
struct CachedMetadata
{
    std::optional<QVector<LinkCache>> links;
    std::optional<QVector<LinkCache>> embeds;
    std::optional<QVector<TagCache>> tags;
    std::optional<QVector<HeadingCache>> headings;
    std::optional<QVector<SectionCache>> sections;
    std::optional<QVector<ListItemCache>> listItems;
    std::optional<QVector<FootnoteCache>> footnoteRefs;
    std::optional<QVector<FootnoteCache>> footnotes;
    std::optional<QHash<QString, BlockCache>> blocks;
    std::optional<QJsonObject> frontmatter;
    std::optional<QVector<FrontmatterLinkCache>> frontmatterLinks;
    /// In-memory name; persisted to disk under the key `frontmatterPos`
    /// via `toPersistedJson` / `fromPersistedJson`.
    std::optional<Position> frontmatterPosition;
};

/// Serialise to the in-memory JSON shape. Fields that are
/// `std::nullopt` are omitted. An empty `CachedMetadata` produces
/// `QJsonObject{}`.
QJsonObject toJson(const CachedMetadata &cache);

/// Inverse of `toJson`. Missing keys leave the corresponding optional
/// field as `std::nullopt`.
CachedMetadata fromJson(const QJsonObject &obj);

/// Identical to `toJson` except the in-memory key `frontmatterPosition`
/// is renamed to `frontmatterPos` to match Obsidian's on-disk format.
QJsonObject toPersistedJson(const CachedMetadata &cache);

/// Identical to `fromJson` except the on-disk key `frontmatterPos` is
/// read into the in-memory field `frontmatterPosition`.
CachedMetadata fromPersistedJson(const QJsonObject &obj);

} // namespace Corbomite
