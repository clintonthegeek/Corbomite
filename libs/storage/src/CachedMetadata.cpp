// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/CachedMetadata.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonValue>

namespace Corbomite {

namespace {

// ---------------------------------------------------------------------------
// Pos / Position
// ---------------------------------------------------------------------------

QJsonObject posToJson(const Pos &p)
{
    QJsonObject o;
    o.insert(QStringLiteral("line"), p.line);
    o.insert(QStringLiteral("col"), p.col);
    o.insert(QStringLiteral("offset"), p.offset);
    return o;
}

Pos posFromJson(const QJsonObject &o)
{
    Pos p;
    p.line = o.value(QStringLiteral("line")).toInt();
    p.col = o.value(QStringLiteral("col")).toInt();
    p.offset = o.value(QStringLiteral("offset")).toInt();
    return p;
}

QJsonObject positionToJson(const Position &pos)
{
    QJsonObject o;
    o.insert(QStringLiteral("start"), posToJson(pos.start));
    o.insert(QStringLiteral("end"), posToJson(pos.end));
    return o;
}

Position positionFromJson(const QJsonObject &o)
{
    Position p;
    p.start = posFromJson(o.value(QStringLiteral("start")).toObject());
    p.end = posFromJson(o.value(QStringLiteral("end")).toObject());
    return p;
}

// ---------------------------------------------------------------------------
// LinkCache
// ---------------------------------------------------------------------------

QJsonObject linkToJson(const LinkCache &l)
{
    QJsonObject o;
    o.insert(QStringLiteral("link"), l.link);
    o.insert(QStringLiteral("original"), l.original);
    if (l.displayText.has_value())
        o.insert(QStringLiteral("displayText"), *l.displayText);
    o.insert(QStringLiteral("position"), positionToJson(l.position));
    return o;
}

LinkCache linkFromJson(const QJsonObject &o)
{
    LinkCache l;
    l.link = o.value(QStringLiteral("link")).toString();
    l.original = o.value(QStringLiteral("original")).toString();
    if (o.contains(QStringLiteral("displayText")))
        l.displayText = o.value(QStringLiteral("displayText")).toString();
    l.position = positionFromJson(o.value(QStringLiteral("position")).toObject());
    return l;
}

// ---------------------------------------------------------------------------
// TagCache
// ---------------------------------------------------------------------------

QJsonObject tagToJson(const TagCache &t)
{
    QJsonObject o;
    o.insert(QStringLiteral("tag"), t.tag);
    o.insert(QStringLiteral("position"), positionToJson(t.position));
    return o;
}

TagCache tagFromJson(const QJsonObject &o)
{
    TagCache t;
    t.tag = o.value(QStringLiteral("tag")).toString();
    t.position = positionFromJson(o.value(QStringLiteral("position")).toObject());
    return t;
}

// ---------------------------------------------------------------------------
// HeadingCache
// ---------------------------------------------------------------------------

QJsonObject headingToJson(const HeadingCache &h)
{
    QJsonObject o;
    o.insert(QStringLiteral("heading"), h.heading);
    o.insert(QStringLiteral("level"), h.level);
    o.insert(QStringLiteral("position"), positionToJson(h.position));
    return o;
}

HeadingCache headingFromJson(const QJsonObject &o)
{
    HeadingCache h;
    h.heading = o.value(QStringLiteral("heading")).toString();
    h.level = o.value(QStringLiteral("level")).toInt();
    h.position = positionFromJson(o.value(QStringLiteral("position")).toObject());
    return h;
}

// ---------------------------------------------------------------------------
// SectionCache
// ---------------------------------------------------------------------------

QString sectionTypeToString(SectionCache::Type t)
{
    switch (t) {
    case SectionCache::Type::Paragraph: return QStringLiteral("paragraph");
    case SectionCache::Type::Heading: return QStringLiteral("heading");
    case SectionCache::Type::List: return QStringLiteral("list");
    case SectionCache::Type::Code: return QStringLiteral("code");
    case SectionCache::Type::Callout: return QStringLiteral("callout");
    case SectionCache::Type::Yaml: return QStringLiteral("yaml");
    case SectionCache::Type::Table: return QStringLiteral("table");
    case SectionCache::Type::Math: return QStringLiteral("math");
    case SectionCache::Type::Html: return QStringLiteral("html");
    case SectionCache::Type::Blockquote: return QStringLiteral("blockquote");
    case SectionCache::Type::ThematicBreak: return QStringLiteral("thematicBreak");
    case SectionCache::Type::Unknown: return QString();
    }
    return QString();
}

// Returns Unknown if `s` doesn't match any canonical name.
SectionCache::Type sectionTypeFromString(const QString &s)
{
    if (s == QLatin1String("paragraph")) return SectionCache::Type::Paragraph;
    if (s == QLatin1String("heading")) return SectionCache::Type::Heading;
    if (s == QLatin1String("list")) return SectionCache::Type::List;
    if (s == QLatin1String("code")) return SectionCache::Type::Code;
    if (s == QLatin1String("callout")) return SectionCache::Type::Callout;
    if (s == QLatin1String("yaml")) return SectionCache::Type::Yaml;
    if (s == QLatin1String("table")) return SectionCache::Type::Table;
    if (s == QLatin1String("math")) return SectionCache::Type::Math;
    if (s == QLatin1String("html")) return SectionCache::Type::Html;
    if (s == QLatin1String("blockquote")) return SectionCache::Type::Blockquote;
    if (s == QLatin1String("thematicBreak")) return SectionCache::Type::ThematicBreak;
    return SectionCache::Type::Unknown;
}

QJsonObject sectionToJson(const SectionCache &s)
{
    QJsonObject o;
    QString typeStr;
    if (s.type == SectionCache::Type::Unknown) {
        // Fall back to raw string; empty if nothing was captured.
        typeStr = s.rawType;
    } else {
        typeStr = sectionTypeToString(s.type);
    }
    o.insert(QStringLiteral("type"), typeStr);
    o.insert(QStringLiteral("position"), positionToJson(s.position));
    if (s.id.has_value())
        o.insert(QStringLiteral("id"), *s.id);
    return o;
}

SectionCache sectionFromJson(const QJsonObject &o)
{
    SectionCache s;
    const QString typeStr = o.value(QStringLiteral("type")).toString();
    const SectionCache::Type parsed = sectionTypeFromString(typeStr);
    s.type = parsed;
    if (parsed == SectionCache::Type::Unknown)
        s.rawType = typeStr;
    s.position = positionFromJson(o.value(QStringLiteral("position")).toObject());
    if (o.contains(QStringLiteral("id")))
        s.id = o.value(QStringLiteral("id")).toString();
    return s;
}

// ---------------------------------------------------------------------------
// ListItemCache
// ---------------------------------------------------------------------------

QJsonObject listItemToJson(const ListItemCache &li)
{
    QJsonObject o;
    o.insert(QStringLiteral("position"), positionToJson(li.position));
    o.insert(QStringLiteral("parent"), li.parent);
    if (li.task.has_value())
        o.insert(QStringLiteral("task"), *li.task);
    if (li.id.has_value())
        o.insert(QStringLiteral("id"), *li.id);
    return o;
}

ListItemCache listItemFromJson(const QJsonObject &o)
{
    ListItemCache li;
    li.position = positionFromJson(o.value(QStringLiteral("position")).toObject());
    li.parent = o.value(QStringLiteral("parent")).toInt(-1);
    if (o.contains(QStringLiteral("task")))
        li.task = o.value(QStringLiteral("task")).toString();
    if (o.contains(QStringLiteral("id")))
        li.id = o.value(QStringLiteral("id")).toString();
    return li;
}

// ---------------------------------------------------------------------------
// FootnoteCache
// ---------------------------------------------------------------------------

QJsonObject footnoteToJson(const FootnoteCache &f)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), f.id);
    o.insert(QStringLiteral("position"), positionToJson(f.position));
    return o;
}

FootnoteCache footnoteFromJson(const QJsonObject &o)
{
    FootnoteCache f;
    f.id = o.value(QStringLiteral("id")).toString();
    f.position = positionFromJson(o.value(QStringLiteral("position")).toObject());
    return f;
}

// ---------------------------------------------------------------------------
// BlockCache
// ---------------------------------------------------------------------------

QJsonObject blockToJson(const BlockCache &b)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), b.id);
    o.insert(QStringLiteral("position"), positionToJson(b.position));
    return o;
}

BlockCache blockFromJson(const QJsonObject &o)
{
    BlockCache b;
    b.id = o.value(QStringLiteral("id")).toString();
    b.position = positionFromJson(o.value(QStringLiteral("position")).toObject());
    return b;
}

// ---------------------------------------------------------------------------
// FrontmatterLinkCache
// ---------------------------------------------------------------------------

QJsonObject frontmatterLinkToJson(const FrontmatterLinkCache &l)
{
    QJsonObject o;
    o.insert(QStringLiteral("link"), l.link);
    o.insert(QStringLiteral("original"), l.original);
    if (l.displayText.has_value())
        o.insert(QStringLiteral("displayText"), *l.displayText);
    o.insert(QStringLiteral("key"), l.key);
    return o;
}

FrontmatterLinkCache frontmatterLinkFromJson(const QJsonObject &o)
{
    FrontmatterLinkCache l;
    l.link = o.value(QStringLiteral("link")).toString();
    l.original = o.value(QStringLiteral("original")).toString();
    if (o.contains(QStringLiteral("displayText")))
        l.displayText = o.value(QStringLiteral("displayText")).toString();
    l.key = o.value(QStringLiteral("key")).toString();
    return l;
}

// ---------------------------------------------------------------------------
// Generic vector <-> JSON array helpers
// ---------------------------------------------------------------------------

template <typename T, typename F>
QJsonArray vectorToJson(const QVector<T> &v, F toJsonFn)
{
    QJsonArray arr;
    for (const T &item : v)
        arr.append(toJsonFn(item));
    return arr;
}

template <typename T, typename F>
QVector<T> vectorFromJson(const QJsonArray &arr, F fromJsonFn)
{
    QVector<T> v;
    v.reserve(arr.size());
    for (const QJsonValue &val : arr)
        v.append(fromJsonFn(val.toObject()));
    return v;
}

// ---------------------------------------------------------------------------
// Main toJson / fromJson, shared by both in-memory + persisted variants.
// The persisted variant differs only in the frontmatterPosition key.
// ---------------------------------------------------------------------------

QJsonObject cachedMetadataToJsonImpl(const CachedMetadata &c, const QString &frontmatterPosKey)
{
    QJsonObject o;

    if (c.links.has_value())
        o.insert(QStringLiteral("links"), vectorToJson(*c.links, linkToJson));
    if (c.embeds.has_value())
        o.insert(QStringLiteral("embeds"), vectorToJson(*c.embeds, linkToJson));
    if (c.tags.has_value())
        o.insert(QStringLiteral("tags"), vectorToJson(*c.tags, tagToJson));
    if (c.headings.has_value())
        o.insert(QStringLiteral("headings"), vectorToJson(*c.headings, headingToJson));
    if (c.sections.has_value())
        o.insert(QStringLiteral("sections"), vectorToJson(*c.sections, sectionToJson));
    if (c.listItems.has_value())
        o.insert(QStringLiteral("listItems"), vectorToJson(*c.listItems, listItemToJson));
    if (c.footnoteRefs.has_value())
        o.insert(QStringLiteral("footnoteRefs"), vectorToJson(*c.footnoteRefs, footnoteToJson));
    if (c.footnotes.has_value())
        o.insert(QStringLiteral("footnotes"), vectorToJson(*c.footnotes, footnoteToJson));

    if (c.blocks.has_value()) {
        QJsonObject blocksObj;
        // Sort keys lexicographically for deterministic ordering.
        QList<QString> keys = c.blocks->keys();
        std::sort(keys.begin(), keys.end());
        for (const QString &key : keys)
            blocksObj.insert(key, blockToJson(c.blocks->value(key)));
        o.insert(QStringLiteral("blocks"), blocksObj);
    }

    if (c.frontmatter.has_value())
        o.insert(QStringLiteral("frontmatter"), *c.frontmatter);

    if (c.frontmatterLinks.has_value())
        o.insert(QStringLiteral("frontmatterLinks"),
                 vectorToJson(*c.frontmatterLinks, frontmatterLinkToJson));

    if (c.frontmatterPosition.has_value())
        o.insert(frontmatterPosKey, positionToJson(*c.frontmatterPosition));

    return o;
}

CachedMetadata cachedMetadataFromJsonImpl(const QJsonObject &o, const QString &frontmatterPosKey)
{
    CachedMetadata c;

    if (o.contains(QStringLiteral("links")))
        c.links = vectorFromJson<LinkCache>(
            o.value(QStringLiteral("links")).toArray(), linkFromJson);
    if (o.contains(QStringLiteral("embeds")))
        c.embeds = vectorFromJson<LinkCache>(
            o.value(QStringLiteral("embeds")).toArray(), linkFromJson);
    if (o.contains(QStringLiteral("tags")))
        c.tags = vectorFromJson<TagCache>(
            o.value(QStringLiteral("tags")).toArray(), tagFromJson);
    if (o.contains(QStringLiteral("headings")))
        c.headings = vectorFromJson<HeadingCache>(
            o.value(QStringLiteral("headings")).toArray(), headingFromJson);
    if (o.contains(QStringLiteral("sections")))
        c.sections = vectorFromJson<SectionCache>(
            o.value(QStringLiteral("sections")).toArray(), sectionFromJson);
    if (o.contains(QStringLiteral("listItems")))
        c.listItems = vectorFromJson<ListItemCache>(
            o.value(QStringLiteral("listItems")).toArray(), listItemFromJson);
    if (o.contains(QStringLiteral("footnoteRefs")))
        c.footnoteRefs = vectorFromJson<FootnoteCache>(
            o.value(QStringLiteral("footnoteRefs")).toArray(), footnoteFromJson);
    if (o.contains(QStringLiteral("footnotes")))
        c.footnotes = vectorFromJson<FootnoteCache>(
            o.value(QStringLiteral("footnotes")).toArray(), footnoteFromJson);

    if (o.contains(QStringLiteral("blocks"))) {
        QHash<QString, BlockCache> blocks;
        const QJsonObject blocksObj = o.value(QStringLiteral("blocks")).toObject();
        for (auto it = blocksObj.constBegin(); it != blocksObj.constEnd(); ++it)
            blocks.insert(it.key(), blockFromJson(it.value().toObject()));
        c.blocks = blocks;
    }

    if (o.contains(QStringLiteral("frontmatter")))
        c.frontmatter = o.value(QStringLiteral("frontmatter")).toObject();

    if (o.contains(QStringLiteral("frontmatterLinks")))
        c.frontmatterLinks = vectorFromJson<FrontmatterLinkCache>(
            o.value(QStringLiteral("frontmatterLinks")).toArray(),
            frontmatterLinkFromJson);

    if (o.contains(frontmatterPosKey))
        c.frontmatterPosition = positionFromJson(o.value(frontmatterPosKey).toObject());

    return c;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QJsonObject toJson(const CachedMetadata &cache)
{
    return cachedMetadataToJsonImpl(cache, QStringLiteral("frontmatterPosition"));
}

CachedMetadata fromJson(const QJsonObject &obj)
{
    return cachedMetadataFromJsonImpl(obj, QStringLiteral("frontmatterPosition"));
}

QJsonObject toPersistedJson(const CachedMetadata &cache)
{
    return cachedMetadataToJsonImpl(cache, QStringLiteral("frontmatterPos"));
}

CachedMetadata fromPersistedJson(const QJsonObject &obj)
{
    return cachedMetadataFromJsonImpl(obj, QStringLiteral("frontmatterPos"));
}

} // namespace Corbomite
