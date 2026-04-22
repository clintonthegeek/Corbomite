// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_VAULTRESOURCEPROVIDER_H
#define CORBOMITE_CORE_VAULTRESOURCEPROVIDER_H

#include <markoff/ResourceProvider.h>

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <optional>

namespace Corbomite::Core {

/// Narrow resolution interface used by renderers (ReadingView, Markoff,
/// EmbedRenderer, HoverPopover) to turn vault-relative names into
/// resources. Promoted from `libs/readingview/` in Cluster J Phase 1.
///
/// Phase C1: now inherits `Markoff::ResourceProvider` — the
/// abstract surface consumed by markoff-reading's DI seam. Corbomite
/// subclasses get transparent markoff-reading compatibility; the
/// Markoff side only sees the abstract-interface methods.
class VaultResourceProvider : public Markoff::ResourceProvider
{
public:
    ~VaultResourceProvider() override = default;

    QUrl resolveImage(const QString &name) const override = 0;
    QByteArray loadImageBytes(const QString &name) const override = 0;
    std::optional<QString> resolveEmbed(const QString &name) const override = 0;
    QUrl resolveWikiLink(const QString &target) const override = 0;
    bool wikiLinkExists(const QString &target) const override = 0;
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_VAULTRESOURCEPROVIDER_H
