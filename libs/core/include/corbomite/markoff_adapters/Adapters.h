// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_MARKOFF_ADAPTERS_H
#define CORBOMITE_MARKOFF_ADAPTERS_H

// Phase C1 DI-seam adapters. Corbomite's native types
// (`Corbomite::Core::EmbedRegistry`, `Corbomite::Core::CodeBlockProcessorRegistry`,
// `Corbomite::LinkResolver`, `Corbomite::MetadataCache`) keep their
// richer Corbomite-flavored APIs. These wrappers present them through
// the narrower `Markoff::*` / `Markoff::Vault::*` abstract interfaces
// that `Markoff::Reading::ReadingView` accepts via its new setters.
//
// Lifecycle: the adapter holds a non-owning pointer back to its
// underlying Corbomite object. Caller keeps both alive for the same
// lifetime window.

#include <markoff/CodeBlockProcessorRegistry.h>
#include <markoff/EmbedRegistry.h>
#include <markoff/LinkResolver.h>
#include <markoff/vault/MetadataCache.h>
#include <markoff/vault/MetadataParser.h>

#include <QHash>
#include <QString>

#include <memory>
#include <mutex>

namespace Corbomite {

class LinkResolver;
class MetadataCache;

namespace Core {
class CodeBlockProcessorRegistry;
class EmbedRegistry;
} // namespace Core

namespace MarkoffAdapters {

/// Wraps `Corbomite::Core::EmbedRegistry` as a `Markoff::EmbedRegistry`.
/// `dispatch` forwards to the native Corbomite registry; `registerExtension`
/// forwards and discards the returned Handle (Markoff callers don't need
/// the unregister path).
class EmbedRegistryAdapter final : public Markoff::EmbedRegistry
{
public:
    explicit EmbedRegistryAdapter(Core::EmbedRegistry *inner);

    void registerExtension(const QString &ext,
                           Markoff::EmbedFactory factory) override;
    std::unique_ptr<Markoff::MarkdownRenderChild>
    dispatch(const Markoff::EmbedRequest &req) const override;

private:
    Core::EmbedRegistry *m_inner;
};

/// Wraps `Corbomite::Core::CodeBlockProcessorRegistry` as a Markoff
/// registry. Converts between the two `CodeBlockContext` shapes at the
/// dispatch boundary (Corbomite's version carries resources + depth;
/// Markoff's carries sourcePath + language).
class CodeBlockRegistryAdapter final
    : public Markoff::CodeBlockProcessorRegistry
{
public:
    explicit CodeBlockRegistryAdapter(Core::CodeBlockProcessorRegistry *inner);

    void registerLanguage(const QString &language,
                          Markoff::CodeBlockProcessor proc) override;
    bool dispatch(const QString &language,
                  const QString &source,
                  void *node,
                  const Markoff::CodeBlockContext &ctx) const override;
    bool hasLanguage(const QString &language) const override;

private:
    Core::CodeBlockProcessorRegistry *m_inner;
};

/// Wraps `Corbomite::LinkResolver` as a `Markoff::LinkResolver`.
/// Corbomite's algorithm is a 6-step Obsidian-compatible search; Markoff's
/// interface is the minimal `resolve(linkText, fromPath) -> QString`. The
/// adapter extracts the `path` field of Corbomite's ResolvedLink on hit;
/// empty string on miss.
class LinkResolverAdapter final : public Markoff::LinkResolver
{
public:
    explicit LinkResolverAdapter(const Corbomite::LinkResolver *inner);

    QString resolve(const QString &linkText,
                    const QString &fromPath) const override;

private:
    const Corbomite::LinkResolver *m_inner;
};

/// Wraps `Corbomite::MetadataCache` as a `Markoff::Vault::MetadataCache`.
/// The interface returns `const CachedMetadata*`; since Corbomite and
/// Markoff use different `CachedMetadata` struct shapes, the adapter
/// lazy-converts on each miss and caches the result keyed by path so the
/// returned pointer stays valid for the adapter's lifetime.
class MetadataCacheAdapter final : public Markoff::Vault::MetadataCache
{
public:
    explicit MetadataCacheAdapter(Corbomite::MetadataCache *inner);

    const Markoff::Vault::CachedMetadata *
    getFileCache(const QString &path) const override;

private:
    Corbomite::MetadataCache *m_inner;
    // Converted caches. Pointer stability: entries live for the lifetime
    // of the adapter (not recycled). getFileCache(): one entry per
    // distinct `path` passed in. std::shared_ptr so the QHash rehash
    // can move the entry without invalidating outstanding pointers.
    mutable QHash<QString, std::shared_ptr<Markoff::Vault::CachedMetadata>>
        m_converted;
    mutable std::mutex m_mutex;
};

/// Wraps `Corbomite::MetadataParser::parse` (static) as a
/// `Markoff::Vault::MetadataParser` instance. The injected resolver is
/// ignored (Corbomite's parser takes its own `Corbomite::LinkResolver`
/// from the ctor); markoff-reading only uses the parser for heading/block
/// slicing, which doesn't touch link resolution.
class MetadataParserImpl final : public Markoff::Vault::MetadataParser
{
public:
    explicit MetadataParserImpl(const Corbomite::LinkResolver *resolver);

    Markoff::Vault::MetadataParseResult
    parse(const QByteArray &content,
          const QString &path,
          const Markoff::LinkResolver &resolver) const override;

private:
    const Corbomite::LinkResolver *m_resolver;
};

} // namespace MarkoffAdapters
} // namespace Corbomite

#endif // CORBOMITE_MARKOFF_ADAPTERS_H
