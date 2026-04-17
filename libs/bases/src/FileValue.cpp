// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/Values.h"

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/TFile.h"

#include <QDir>
#include <QFileInfo>

namespace Corbomite::Bases {

// ----- FileValue -----

FileValue::FileValue(TFile *file, MetadataCache *cache)
    : m_file(file), m_cache(cache)
{
}

FileValue::~FileValue() = default;

QString FileValue::toString() const
{
    return m_file ? m_file->name : QString{};
}

bool FileValue::equals(const Value &other) const
{
    auto *rhs = dynamic_cast<const FileValue *>(&other);
    if (!rhs) return false;
    return m_file == rhs->m_file;
}

bool FileValue::looseEquals(const Value &other) const
{
    if (equals(other)) return true;
    // Compare path against a StringValue or LinkValue per audit §4.1.
    if (auto *s = dynamic_cast<const StringValue *>(&other))
        return m_file && m_file->path == s->data();
    return false;
}

const QStringList &FileValue::filePropertyMembers()
{
    // Per audit §2: 14 built-in file.* members.
    static const QStringList kMembers {
        QStringLiteral("file"),     QStringLiteral("name"),
        QStringLiteral("basename"), QStringLiteral("fullname"),
        QStringLiteral("path"),     QStringLiteral("folder"),
        QStringLiteral("ext"),      QStringLiteral("ctime"),
        QStringLiteral("mtime"),    QStringLiteral("size"),
        QStringLiteral("links"),    QStringLiteral("backlinks"),
        QStringLiteral("embeds"),   QStringLiteral("tags"),
    };
    return kMembers;
}

QStringList FileValue::keys() const
{
    return filePropertyMembers();
}

ValuePtr FileValue::objectAccess(const QString &key) const
{
    if (!m_file) return NullValue::instance();
    const QString k = key.toLower();

    if (k == QLatin1String("file")) {
        // Self-reference — return a shared_ptr to this instance.
        return std::const_pointer_cast<Value>(
            std::static_pointer_cast<const Value>(shared_from_this()));
    }
    if (k == QLatin1String("name"))
        return std::make_shared<StringValue>(m_file->name);
    if (k == QLatin1String("basename"))
        return std::make_shared<StringValue>(m_file->basename);
    if (k == QLatin1String("fullname")) {
        // `fullname` is name + "." + extension in the audit — same as `name`
        // today (TFile::name already includes extension). Keeping the
        // accessor distinct matches Obsidian surface.
        return std::make_shared<StringValue>(m_file->name);
    }
    if (k == QLatin1String("path"))
        return std::make_shared<StringValue>(m_file->path);
    if (k == QLatin1String("folder")) {
        QFileInfo fi(m_file->path);
        return std::make_shared<StringValue>(fi.dir().path());
    }
    if (k == QLatin1String("ext"))
        return std::make_shared<StringValue>(m_file->extension);
    if (k == QLatin1String("ctime")) {
        const qint64 ms = m_file->stat ? m_file->stat->ctimeMs : 0;
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
        return std::make_shared<DateValue>(dt, true);
    }
    if (k == QLatin1String("mtime")) {
        const qint64 ms = m_file->stat ? m_file->stat->mtimeMs : 0;
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
        return std::make_shared<DateValue>(dt, true);
    }
    if (k == QLatin1String("size"))
        return std::make_shared<NumberValue>(
            m_file->stat ? static_cast<double>(m_file->stat->sizeBytes) : 0.0);
    if (k == QLatin1String("links"))       return getLinks();
    if (k == QLatin1String("backlinks"))   return getBacklinks();
    if (k == QLatin1String("embeds"))      return getEmbeds();
    if (k == QLatin1String("tags"))        return getTags();
    if (k == QLatin1String("properties"))  return getProperties();

    // Fallthrough — frontmatter access happens through `note.<key>` not
    // `file.<key>`, so unknown members return null rather than falling
    // back to frontmatter lookup.
    return NullValue::instance();
}

// --- aggregate accessors ---

std::shared_ptr<ListValue> FileValue::getLinks() const
{
    if (m_cachedLinks) return m_cachedLinks;
    QVector<ValuePtr> items;
    if (m_cache && m_file) {
        const auto c = m_cache->getFileCache(m_file->path);
        if (c && c->links) {
            for (const auto &l : *c->links) {
                items.push_back(std::make_shared<LinkValue>(
                    l.link, m_file->path, l.displayText.value_or(QString{})));
            }
        }
    }
    m_cachedLinks = std::make_shared<ListValue>(items);
    return m_cachedLinks;
}

std::shared_ptr<ListValue> FileValue::getBacklinks() const
{
    if (m_cachedBacklinks) return m_cachedBacklinks;
    QVector<ValuePtr> items;
    if (m_cache && m_file) {
        // O(vault) reverse scan — audit §11 warns this is performance-heavy.
        const QString target = m_file->path;
        for (const QString &p : m_cache->allPaths()) {
            if (p == target) continue;
            const auto c = m_cache->getFileCache(p);
            if (!c || !c->links) continue;
            for (const auto &l : *c->links) {
                if (l.link == target || l.link == m_file->basename) {
                    items.push_back(std::make_shared<LinkValue>(
                        target, p, l.displayText.value_or(QString{})));
                    break;  // one backlink per source path max
                }
            }
        }
    }
    m_cachedBacklinks = std::make_shared<ListValue>(items);
    return m_cachedBacklinks;
}

std::shared_ptr<ListValue> FileValue::getEmbeds() const
{
    if (m_cachedEmbeds) return m_cachedEmbeds;
    QVector<ValuePtr> items;
    if (m_cache && m_file) {
        const auto c = m_cache->getFileCache(m_file->path);
        if (c && c->embeds) {
            for (const auto &e : *c->embeds) {
                items.push_back(std::make_shared<LinkValue>(
                    e.link, m_file->path, e.displayText.value_or(QString{})));
            }
        }
    }
    m_cachedEmbeds = std::make_shared<ListValue>(items);
    return m_cachedEmbeds;
}

std::shared_ptr<ListValue> FileValue::getTags() const
{
    if (m_cachedTags) return m_cachedTags;
    QVector<ValuePtr> items;
    if (m_cache && m_file) {
        const auto c = m_cache->getFileCache(m_file->path);
        if (c && c->tags) {
            for (const auto &t : *c->tags) {
                items.push_back(std::make_shared<TagValue>(t.tag));
            }
        }
    }
    m_cachedTags = std::make_shared<ListValue>(items);
    return m_cachedTags;
}

std::shared_ptr<ObjectValue> FileValue::getProperties() const
{
    if (m_cachedProperties) return m_cachedProperties;
    QJsonObject fm;
    if (m_cache && m_file) {
        const auto c = m_cache->getFileCache(m_file->path);
        if (c && c->frontmatter) fm = *c->frontmatter;
    }
    m_cachedProperties = ObjectValue::fromFrontMatter(fm);
    return m_cachedProperties;
}

// --- predicates ---

bool FileValue::hasLink(const ValuePtr &other) const
{
    if (!m_file || !other) return false;
    QString target;
    if (auto *f = dynamic_cast<FileValue *>(other.get()))
        target = f->file() ? f->file()->path : QString{};
    else
        target = other->toString();
    auto links = getLinks();
    for (const auto &l : links->data()) {
        if (auto *lv = dynamic_cast<LinkValue *>(l.get())) {
            if (lv->data() == target) return true;
        }
    }
    return false;
}

bool FileValue::inFolder(const QString &folderPath) const
{
    if (!m_file) return false;
    if (folderPath.isEmpty() || folderPath == QLatin1String(".")
        || folderPath == QLatin1String("/"))
        return true;
    QString prefix = folderPath;
    if (!prefix.endsWith(QLatin1Char('/'))) prefix += QLatin1Char('/');
    return m_file->path.startsWith(prefix);
}

bool FileValue::hasTag(const QStringList &tags) const
{
    auto actual = getTags();
    for (const QString &wanted : tags) {
        for (const auto &v : actual->data()) {
            auto *tv = dynamic_cast<TagValue *>(v.get());
            if (tv && tv->tagMatches(wanted)) return true;
        }
    }
    return false;
}

bool FileValue::hasProperty(const QString &name) const
{
    auto props = getProperties();
    return props->getInsensitive(name) != nullptr;
}

// ----- ThisFileValue -----

ThisFileValue::ThisFileValue(TFile *file, MetadataCache *cache, Forwarder forwarder)
    : FileValue(file, cache), m_forwarder(std::move(forwarder))
{
}

ValuePtr ThisFileValue::objectAccess(const QString &key) const
{
    // Forward to the enclosing entry first; fall through to regular
    // FileValue::objectAccess if the forwarder returns nullptr.
    if (m_forwarder) {
        if (auto v = m_forwarder(key)) return v;
    }
    return FileValue::objectAccess(key);
}

}  // namespace Corbomite::Bases
