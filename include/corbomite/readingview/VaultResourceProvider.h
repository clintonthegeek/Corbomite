// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_READINGVIEW_VAULTRESOURCEPROVIDER_H
#define CORBOMITE_READINGVIEW_VAULTRESOURCEPROVIDER_H

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <optional>

namespace Corbomite::ReadingView {

/// Narrow resolution interface used by the ReadingView renderer to turn
/// vault-relative names into resources.
///
/// Mirrors `Markoff::ResourceProvider`'s surface (same method signatures)
/// so an application already hosting a Markoff-compatible provider can
/// wire a trivial adapter. This library cannot depend on Markoff (they are
/// peer widgets) — hence the parallel interface.
///
/// A provider implementation is expected to be cheap to call repeatedly;
/// SectionLayout hits `resolveImage` once per image embed and
/// `resolveWikiLink` / `wikiLinkExists` once per wiki-link activation.
class VaultResourceProvider
{
public:
    virtual ~VaultResourceProvider() = default;

    /// Resolve an image path `![alt](name)` to a file URL, empty URL if
    /// the resource cannot be found.
    virtual QUrl resolveImage(const QString &name) const = 0;

    /// Load an image's raw bytes (PNG/JPG/SVG). Empty QByteArray on miss
    /// — SectionLayout falls back to the alt text in that case.
    virtual QByteArray loadImageBytes(const QString &name) const = 0;

    /// Transcluded note body for `![[note]]`. `std::nullopt` on miss.
    virtual std::optional<QString> resolveEmbed(const QString &name) const = 0;

    /// Resolve a wiki-link target `[[Target]]` to a file URL. Empty if
    /// the target does not exist.
    virtual QUrl resolveWikiLink(const QString &target) const = 0;

    /// Fast-path existence check. Used to pick "exists" vs. "unresolved"
    /// styling in the wiki-link format.
    virtual bool wikiLinkExists(const QString &target) const = 0;
};

} // namespace Corbomite::ReadingView

#endif // CORBOMITE_READINGVIEW_VAULTRESOURCEPROVIDER_H
