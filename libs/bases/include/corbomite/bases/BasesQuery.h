// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesViewConfig.h"
#include "Formula.h"

#include <QHash>
#include <QString>
#include <QVariantMap>

#include <memory>
#include <optional>
#include <vector>

namespace Corbomite::Bases {

/// In-memory representation of a `.base` file (audit §3 schema).
class BasesQuery
{
public:
    std::vector<std::unique_ptr<BasesViewConfig>> views;
    FilterPtr filters;                           ///< global filters.
    QHash<QString, Formula> formulas;            ///< name -> formula.
    QHash<QString, Formula> summaryFormulas;
    QHash<PropertyId, PropertyConfig> properties;
    /// Insertion order of the user-keyed dicts above. Tracked at parse time
    /// so `toString()` can emit each block in its source order rather than
    /// QVariantMap's alphabetical sort. Without these the round-trip
    /// reorders user-authored .base files on every save (audit:
    /// `bases.md` §"On-disk `.base` format compatibility").
    QStringList formulaOrder;
    QStringList summaryFormulaOrder;
    std::vector<PropertyId> propertyOrder;
    std::optional<QString> newItemFolder;
    std::optional<QString> newItemTemplate;
    QVariantMap unrecognizedData;                ///< forward-compat.

    QString filePath;  ///< attached after load by BasesView.

    /// Parse a YAML body. Empty input → default 1-view "Table" query.
    /// On parse error, returns a default query and populates `*parseError`.
    static std::unique_ptr<BasesQuery> fromString(const QString &text,
                                                   QString *parseError = nullptr);

    /// Serialise back to YAML. Preserves key ordering + unknown keys.
    QString toString() const;

    /// Deep-copy via fromString(toString()). Good enough for MVP.
    std::unique_ptr<BasesQuery> clone() const;

    /// `nullptr` `name` → `views[0]`. Null return only if `views` is empty
    /// (which never happens after `fromString` — always at least the
    /// default view).
    BasesViewConfig *getViewConfig(const QString &name = QString{}) const;
};

}  // namespace Corbomite::Bases
