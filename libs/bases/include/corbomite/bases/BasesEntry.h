// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesQuery.h"
#include "EvalContext.h"
#include "PropertyId.h"
#include "Values.h"

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QString>

namespace Corbomite {
class MetadataCache;
class TFile;
class Vault;
}  // namespace Corbomite

namespace Corbomite::Bases {

class FunctionRegistry;
class VaultResolver;

/// A `BasesEntry` is one vault note projected into a query result — a
/// row in a BasesView's table. Holds a borrowed `TFile *`, a borrowed
/// `Vault *` + `MetadataCache *`, and a back-reference to the enclosing
/// `BasesQuery` (for formula lookup).
class BasesEntry : public EvalContext
{
public:
    BasesEntry(Vault *vault,
               MetadataCache *cache,
               TFile *file,
               TFile *localFile,
               const BasesQuery &query,
               FunctionRegistry *funcs = nullptr,
               const VaultResolver *resolver = nullptr);
    ~BasesEntry() override;

    TFile *file() const { return m_file; }
    TFile *localFile() const { return m_local; }

    /// Frontmatter snapshot from the MetadataCache (empty if no cache entry).
    /// Returned BY VALUE: getFileCache yields a temporary, so a reference
    /// would dangle. QJsonObject is implicitly shared, so the copy is cheap.
    QJsonObject frontmatter() const;

    /// Raw frontmatter keys.
    QStringList getPropertyKeys() const;

    /// Identifier dispatch.
    ValuePtr getByIdentifier(const QString &name) const override;
    QStringList keys() const override;

    const VaultResolver *vault() const override { return m_resolver; }

    /// PropertyId-keyed accessor (dispatches by kind).
    ValuePtr getValue(const PropertyId &id) const;

    /// Evaluate a named formula under this entry, memoised; detects
    /// cycles through `m_inProgressFormulas` and returns FormulaErrorValue.
    ValuePtr formulaValue(const QString &name) const;

private:
    std::shared_ptr<FileValue> implicitFile() const;
    std::shared_ptr<ObjectValue> noteObject() const;

    Vault *m_vault;
    MetadataCache *m_cache;
    TFile *m_file;
    TFile *m_local;
    const BasesQuery &m_query;
    FunctionRegistry *m_funcs;
    const VaultResolver *m_resolver = nullptr;

    mutable std::shared_ptr<FileValue> m_implicit;
    mutable std::shared_ptr<ObjectValue> m_note;
    mutable QHash<QString, ValuePtr> m_formulaCache;
    mutable QSet<QString> m_inProgressFormulas;
};

}  // namespace Corbomite::Bases
