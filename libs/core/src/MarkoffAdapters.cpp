// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#include "corbomite/markoff_adapters/Adapters.h"

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/MetadataParser.h"

namespace Corbomite::MarkoffAdapters {

namespace {

/// Convert a `Corbomite::CachedMetadata` into its `Markoff::Vault::`
/// counterpart. Only the fields markoff-reading's EmbedRenderer actually
/// reads (headings, blocks, sections) carry over; the rest are dropped
/// because `Markoff::Vault::CachedMetadata` has no slots for them.
Markoff::Vault::CachedMetadata
convertCache(const Corbomite::CachedMetadata &src)
{
    Markoff::Vault::CachedMetadata out;

    if (src.headings) {
        QVector<Markoff::Vault::HeadingCache> headings;
        headings.reserve(src.headings->size());
        for (const auto &h : *src.headings) {
            Markoff::Vault::HeadingCache mh;
            mh.heading = h.heading;
            mh.level = h.level;
            mh.position.start.line = h.position.start.line;
            mh.position.start.col = h.position.start.col;
            mh.position.start.offset = h.position.start.offset;
            mh.position.end.line = h.position.end.line;
            mh.position.end.col = h.position.end.col;
            mh.position.end.offset = h.position.end.offset;
            headings.append(std::move(mh));
        }
        out.headings = std::move(headings);
    }

    if (src.blocks) {
        QHash<QString, Markoff::Vault::BlockCache> blocks;
        blocks.reserve(src.blocks->size());
        for (auto it = src.blocks->constBegin();
             it != src.blocks->constEnd(); ++it) {
            Markoff::Vault::BlockCache mb;
            mb.id = it->id;
            mb.position.start.line = it->position.start.line;
            mb.position.start.col = it->position.start.col;
            mb.position.start.offset = it->position.start.offset;
            mb.position.end.line = it->position.end.line;
            mb.position.end.col = it->position.end.col;
            mb.position.end.offset = it->position.end.offset;
            blocks.insert(it.key(), std::move(mb));
        }
        out.blocks = std::move(blocks);
    }

    if (src.sections) {
        QVector<Markoff::Vault::SectionCache> sections;
        sections.reserve(src.sections->size());
        for (const auto &s : *src.sections) {
            Markoff::Vault::SectionCache ms;
            ms.type = s.rawType; // Corbomite enum-typed; rawType carries
                                 // the string form the audit layer stored.
            ms.position.start.line = s.position.start.line;
            ms.position.start.col = s.position.start.col;
            ms.position.start.offset = s.position.start.offset;
            ms.position.end.line = s.position.end.line;
            ms.position.end.col = s.position.end.col;
            ms.position.end.offset = s.position.end.offset;
            sections.append(std::move(ms));
        }
        out.sections = std::move(sections);
    }

    return out;
}

} // namespace

// ---- LinkResolverAdapter --------------------------------------------

LinkResolverAdapter::LinkResolverAdapter(const Corbomite::LinkResolver *inner)
    : m_inner(inner)
{
}

QString LinkResolverAdapter::resolve(const QString &linkText,
                                     const QString &fromPath) const
{
    if (!m_inner) return QString();
    const auto r = m_inner->resolve(fromPath, linkText);
    return r.resolved ? r.path : QString();
}

// ---- MetadataCacheAdapter -------------------------------------------

MetadataCacheAdapter::MetadataCacheAdapter(Corbomite::MetadataCache *inner)
    : m_inner(inner)
{
}

const Markoff::Vault::CachedMetadata *
MetadataCacheAdapter::getFileCache(const QString &path) const
{
    if (!m_inner) return nullptr;
    const auto maybe = m_inner->getFileCache(path);
    if (!maybe) return nullptr;

    std::lock_guard<std::mutex> lock(m_mutex);
    auto converted = std::make_shared<Markoff::Vault::CachedMetadata>(
        convertCache(*maybe));
    auto [it, inserted] = std::make_pair(
        m_converted.insert(path, converted), true);
    Q_UNUSED(inserted);
    return it.value().get();
}

// ---- MetadataParserImpl ---------------------------------------------

MetadataParserImpl::MetadataParserImpl(const Corbomite::LinkResolver *resolver)
    : m_resolver(resolver)
{
}

Markoff::Vault::MetadataParseResult
MetadataParserImpl::parse(const QByteArray &content,
                          const QString &path,
                          const Markoff::LinkResolver & /*resolver*/) const
{
    Markoff::Vault::MetadataParseResult out;
    if (!m_resolver) return out;
    const Corbomite::ParsedNote parsed =
        Corbomite::MetadataParser::parse(content, path, *m_resolver);
    out.cache = convertCache(parsed.cache);
    return out;
}

} // namespace Corbomite::MarkoffAdapters
