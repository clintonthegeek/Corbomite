// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_MARKOFF_ADAPTERS_H
#define CORBOMITE_MARKOFF_ADAPTERS_H

// TODO(port-foundation-exploration): Markoff::LinkResolver, Markoff::Vault::*
// were retired with the old leaves. The entire adapter layer (Phase C1/C2 DI
// seam) is disabled until the vault seam is restored on the Markoff side
// (planned restoration would mirror master's shape — see draft reference at
// libs/markoff-family/docs/specs/2026-05-20-markoff-core-freeze-shape-design.md
// §D7). Consumers (MainWindow, HoverPopover tests) need their own stubs.

#if 0  // disabled pending Markoff::Vault::* restoration

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

namespace MarkoffAdapters {

class LinkResolverAdapter final : public Markoff::LinkResolver
{
public:
    explicit LinkResolverAdapter(const Corbomite::LinkResolver *inner);
    QString resolve(const QString &linkText, const QString &fromPath) const override;
private:
    const Corbomite::LinkResolver *m_inner;
};

class MetadataCacheAdapter final : public Markoff::Vault::MetadataCache
{
public:
    explicit MetadataCacheAdapter(Corbomite::MetadataCache *inner);
    const Markoff::Vault::CachedMetadata *getFileCache(const QString &path) const override;
private:
    Corbomite::MetadataCache *m_inner;
    mutable QHash<QString, std::shared_ptr<Markoff::Vault::CachedMetadata>> m_converted;
    mutable std::mutex m_mutex;
};

class MetadataParserImpl final : public Markoff::Vault::MetadataParser
{
public:
    explicit MetadataParserImpl(const Corbomite::LinkResolver *resolver);
    Markoff::Vault::MetadataParseResult
    parse(const QByteArray &content, const QString &path,
          const Markoff::LinkResolver &resolver) const override;
private:
    const Corbomite::LinkResolver *m_resolver;
};

} // namespace MarkoffAdapters
} // namespace Corbomite

#endif // 0 — disabled pending Markoff::Vault::* restoration

#endif // CORBOMITE_MARKOFF_ADAPTERS_H
