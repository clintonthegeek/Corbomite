// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterTree.h"
#include "PropertyId.h"

#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <memory>
#include <optional>

namespace Markoff {
class YamlValue;
}

namespace Corbomite::Bases {

/// Recursively convert a parsed YAML node into a QVariant, preserving nested
/// map/sequence shapes. Used to round-trip forward-compat keys (the query's
/// `unrecognizedData` and each view's `data`) without flattening or dropping
/// structure.
QVariant yamlToVariant(const Markoff::YamlValue &v);

struct SortKey
{
    PropertyId property;
    QString direction;  ///< "ASC" or "DESC".
};

struct GroupBy
{
    PropertyId property;
    QString direction;
};

/// Per-property settings (optional).
struct PropertyConfig
{
    QString displayName;
    /// The key exactly as it appeared in the file's `properties:` map.
    /// Obsidian stores the map under the *raw* YAML key
    /// (`1766-lQ.js:127-132`) and writes it back verbatim (`:223-227`), only
    /// falling back to the canonical `note.x` form for configs it created
    /// itself via `getPropertyConfig`. Empty = created in memory, so emit the
    /// canonical spelling.
    QString rawKey;
    QVariantMap unrecognizedData;
};

/// One named view within a `.base` file.
///
/// Serialisation rule for property ids — see `serializePropertyId()` in
/// PropertyId.h for the full statement and its justification. In short: the
/// four key sites below (`order`, `sort[].property`, `groupBy.property`,
/// `summaries` keys) are canonicalised to Obsidian's `hQ` short form, and
/// everything in `data` is passed through byte-for-byte, because Obsidian
/// does exactly that and any other split flips the file between the two apps.
///
/// Key order is fixed by Obsidian's `serialize()`
/// (`RE/obsidian/1.13.7/renamed/src/app/own/1766-lQ.js:9-32`): type, name,
/// filters, groupBy, order, sort, limit, summaries, then the free-form `data`
/// keys last.
class BasesViewConfig
{
public:
    QString type;  ///< "table" or plugin-registered view-type name.
    QString name;  ///< unique within `BasesQuery::views`.

    FilterPtr filters;                     ///< optional per-view filters (AND-merged with global).
    QVector<PropertyId> order;             ///< visible-column order.
    QVector<SortKey> sort;                 ///< multi-key sort.
    std::optional<GroupBy> groupBy;
    int limit = 0;                         ///< 0 == unlimited (audit invariant).
    QHash<PropertyId, QString> summaries;  ///< propId → summary-fn-name.
    /// Free-form view-type-specific options — Obsidian's `data` map, and the
    /// only place unknown per-view keys live. Obsidian has no
    /// `unrecognizedData` on a view (`1770-dQ.js:96-97`); naming it `data`
    /// also matches what `BasesViewConfig::get`/`set` read there.
    QVariantMap data;

    /// Parse one YAML map node into a view config.
    static std::unique_ptr<BasesViewConfig> fromYaml(const Markoff::YamlValue &node,
                                                     QString *errorOut);

    /// Emit as a YAML-ready QVariantMap (canonical key order, `data` last).
    QVariantMap toMap() const;
};

}  // namespace Corbomite::Bases
