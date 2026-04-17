// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesEntry.h"

#include "corbomite/bases/FunctionRegistry.h"

#include "corbomite/storage/CachedMetadata.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

#include <KLocalizedString>

namespace Corbomite::Bases {

BasesEntry::BasesEntry(Vault *vault, MetadataCache *cache,
                       TFile *file, TFile *localFile,
                       const BasesQuery &query, FunctionRegistry *funcs)
    : m_vault(vault),
      m_cache(cache),
      m_file(file),
      m_local(localFile),
      m_query(query),
      m_funcs(funcs ? funcs : &FunctionRegistry::global())
{
}

BasesEntry::~BasesEntry() = default;

const QJsonObject &BasesEntry::frontmatter() const
{
    if (m_cache && m_file) {
        const auto c = m_cache->getFileCache(m_file->path);
        if (c && c->frontmatter) return *c->frontmatter;
    }
    return m_emptyFm;
}

QStringList BasesEntry::getPropertyKeys() const
{
    return frontmatter().keys();
}

std::shared_ptr<FileValue> BasesEntry::implicitFile() const
{
    if (!m_implicit) m_implicit = std::make_shared<FileValue>(m_file, m_cache);
    return m_implicit;
}

std::shared_ptr<ObjectValue> BasesEntry::noteObject() const
{
    if (!m_note) m_note = ObjectValue::fromFrontMatter(frontmatter());
    return m_note;
}

ValuePtr BasesEntry::getByIdentifier(const QString &name) const
{
    const QString lower = name.toLower();
    if (lower == QLatin1String("this")) {
        return std::make_shared<ThisFileValue>(m_local, m_cache,
            [this](const QString &n) -> ValuePtr {
                // Avoid infinite recursion for "this" itself.
                if (n.toLower() == QLatin1String("this")) return nullptr;
                return getByIdentifier(n);
            });
    }
    if (lower == QLatin1String("note")) return noteObject();
    if (lower == QLatin1String("file")) return implicitFile();
    if (lower == QLatin1String("formula")) {
        return std::make_shared<LambdaObjectValue>(
            [this](const QString &fname) -> ValuePtr { return formulaValue(fname); });
    }
    // Default: frontmatter property, case-insensitive.
    return noteObject()->getInsensitive(name);
}

QStringList BasesEntry::keys() const
{
    QStringList out = getPropertyKeys();
    out << QStringLiteral("this") << QStringLiteral("note")
        << QStringLiteral("file") << QStringLiteral("formula");
    return out;
}

ValuePtr BasesEntry::getValue(const PropertyId &id) const
{
    switch (id.kind) {
    case PropertyKind::Note:
        return noteObject()->getInsensitive(id.name);
    case PropertyKind::File:
        return implicitFile()->objectAccess(id.name);
    case PropertyKind::Formula:
        return formulaValue(id.name);
    }
    return NullValue::instance();
}

ValuePtr BasesEntry::formulaValue(const QString &name) const
{
    auto cached = m_formulaCache.constFind(name);
    if (cached != m_formulaCache.constEnd()) return *cached;
    if (m_inProgressFormulas.contains(name)) {
        return std::make_shared<FormulaErrorValue>(
            i18n("formula '%1' has a cycle", name));
    }
    auto it = m_query.formulas.constFind(name);
    if (it == m_query.formulas.constEnd())
        return NullValue::instance();
    m_inProgressFormulas.insert(name);
    auto v = it->getValue(*this, m_funcs);
    m_inProgressFormulas.remove(name);
    if (!v) v = NullValue::instance();
    m_formulaCache.insert(name, v);
    return v;
}

}  // namespace Corbomite::Bases
