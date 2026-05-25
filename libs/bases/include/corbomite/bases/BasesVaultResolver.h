// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "VaultResolver.h"

#include <corbomite/storage/LinkResolver.h>

namespace Corbomite {
class Vault;
class MetadataCache;
}  // namespace Corbomite

namespace Corbomite::Bases {

/// VaultResolver backed by a live Vault + MetadataCache. Seeds an owned
/// LinkResolver from the vault's full path set at construction.
class BasesVaultResolver : public VaultResolver
{
public:
    BasesVaultResolver(Vault *vault, MetadataCache *cache);

    ValuePtr fileAt(const QString &pathOrName) const override;
    QString resolveLinkTarget(const QString &linkData,
                              const QString &sourcePath) const override;

private:
    Vault *m_vault;
    MetadataCache *m_cache;
    LinkResolver m_links;
};

}  // namespace Corbomite::Bases
