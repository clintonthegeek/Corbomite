// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <markoff/core/vault/ResourceProvider.h>

namespace Corbomite {

class Vault;

class VaultResourceProvider : public Markoff::Vault::ResourceProvider {
public:
    VaultResourceProvider(Vault *vault, const QString &noteRelativePath);

    QUrl resolveImage(const QString &name) const override;
    QByteArray loadImageBytes(const QString &name) const override;
    std::optional<QString> resolveEmbed(const QString &name) const override;
    QUrl resolveWikiLink(const QString &target) const override;
    bool wikiLinkExists(const QString &target) const override;

private:
    QString resolveTarget(const QString &target) const;

    Vault *m_vault;
    QString m_vaultPath;
    QString m_noteDir; // directory containing the current note
};

} // namespace Corbomite
