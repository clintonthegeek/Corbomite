// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesVaultResolver.h"

#include "corbomite/bases/Values.h"

#include <corbomite/vault/Vault.h>

namespace Corbomite::Bases {

BasesVaultResolver::BasesVaultResolver(Vault *vault, MetadataCache *cache)
    : m_vault(vault), m_cache(cache)
{
    QStringList paths;
    if (m_vault) {
        const QVector<TFile *> files = m_vault->getFiles();
        paths.reserve(files.size());
        for (TFile *f : files)
            if (f) paths << f->path;
    }
    m_links.setVaultPaths(paths);
}

ValuePtr BasesVaultResolver::fileAt(const QString &pathOrName) const
{
    if (!m_vault) return NullValue::instance();

    TFile *f = m_vault->getFileByPath(pathOrName);
    if (!f) {
        const ResolvedLink r = m_links.resolve(QString{}, pathOrName);
        if (r.resolved) f = m_vault->getFileByPath(r.path);
    }
    if (!f) return NullValue::instance();
    return std::make_shared<FileValue>(f, m_cache);
}

QString BasesVaultResolver::resolveLinkTarget(const QString &linkData,
                                              const QString &sourcePath) const
{
    const ResolvedLink r = m_links.resolve(sourcePath, linkData);
    return r.resolved ? r.path : QString{};
}

}  // namespace Corbomite::Bases
