// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_VAULTRESOURCEPROVIDER_H
#define CORBOMITE_CORE_VAULTRESOURCEPROVIDER_H

#include <markoff/core/vault/ResourceProvider.h>

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <optional>

namespace Corbomite::Core {

/// Narrow resolution interface used by renderers to turn vault-relative
/// names into resources. Inherits `Markoff::Vault::ResourceProvider`
/// (restored 2026-05-20 driven by port pull; class moved from
/// `Markoff::ResourceProvider` to `Markoff::Vault::ResourceProvider`).
class VaultResourceProvider : public Markoff::Vault::ResourceProvider
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
