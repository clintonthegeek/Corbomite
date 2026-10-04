// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesQuery.h"

#include "markoff/parser/YamlValue.h"

#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVariantList>

namespace Corbomite::Bases {

namespace {

void emitValue(QString &out, const QVariant &v, int indent);

QString yamlQuoteIfNeeded(const QString &s)
{
    // Quote strings that could confuse the YAML parser (contain :, #, -,
    // start/end whitespace, look like bools/numbers/nulls, or contain
    // newlines). Otherwise emit bare.
    if (s.isEmpty()) return QStringLiteral("\"\"");
    const auto needs =
        s.contains(QLatin1Char(':')) || s.contains(QLatin1Char('#'))
        || s.contains(QLatin1Char('\n')) || s.contains(QLatin1Char('"'))
        || s.startsWith(QLatin1Char(' ')) || s.endsWith(QLatin1Char(' '))
        || s.startsWith(QLatin1Char('-')) || s.startsWith(QLatin1Char('['))
        || s.startsWith(QLatin1Char('{'))
        || s == QLatin1String("true") || s == QLatin1String("false")
        || s == QLatin1String("null") || s == QLatin1String("~");
    if (!needs) {
        bool allDigitish = !s.isEmpty();
        for (QChar c : s) {
            if (!c.isDigit() && c != QLatin1Char('.') && c != QLatin1Char('-')
                && c != QLatin1Char('+') && c != QLatin1Char('e')
                && c != QLatin1Char('E')) {
                allDigitish = false;
                break;
            }
        }
        if (!allDigitish) return s;
    }
    // Double-quoted with JSON-ish escape.
    QString q = QStringLiteral("\"");
    for (QChar c : s) {
        if (c == QLatin1Char('"'))       q.append(QStringLiteral("\\\""));
        else if (c == QLatin1Char('\\')) q.append(QStringLiteral("\\\\"));
        else if (c == QLatin1Char('\n')) q.append(QStringLiteral("\\n"));
        else q.append(c);
    }
    q.append(QLatin1Char('"'));
    return q;
}

void emitScalar(QString &out, const QVariant &v, int indent)
{
    Q_UNUSED(indent);
    switch (v.typeId()) {
    case QMetaType::Bool:
        out.append(v.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
        return;
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        out.append(QString::number(v.toLongLong()));
        return;
    case QMetaType::Double:
    case QMetaType::Float:
        out.append(QString::number(v.toDouble(), 'g', 15));
        return;
    case QMetaType::QString:
        out.append(yamlQuoteIfNeeded(v.toString()));
        return;
    default:
        out.append(yamlQuoteIfNeeded(v.toString()));
        return;
    }
}

// Iterate `m`'s keys in `keyOrder` first (skipping any not present in `m`),
// then any remaining keys in `m`'s natural (alphabetical) order. Returns the
// concatenated list, used to drive emit order without losing keys that the
// caller didn't explicitly enumerate (e.g. unrecognizedData).
QStringList orderedKeys(const QVariantMap &m, const QStringList &keyOrder)
{
    QStringList out;
    out.reserve(m.size());
    QSet<QString> placed;
    for (const QString &k : keyOrder) {
        if (m.contains(k) && !placed.contains(k)) {
            out.append(k);
            placed.insert(k);
        }
    }
    for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
        if (!placed.contains(it.key())) out.append(it.key());
    }
    return out;
}

void emitList(QString &out, const QVariantList &xs, int indent,
              const QStringList &itemKeyOrder = {})
{
    if (xs.isEmpty()) { out.append(QStringLiteral("[]")); return; }
    const QString pad(indent, QLatin1Char(' '));
    for (const QVariant &v : xs) {
        out.append(QLatin1Char('\n'));
        out.append(pad);
        out.append(QStringLiteral("- "));
        if (v.typeId() == QMetaType::QVariantMap) {
            // For `- {key: value, ...}`, emit on same line with the dash
            // then nested under `indent+2`.
            bool first = true;
            const auto m = v.toMap();
            for (const QString &key : orderedKeys(m, itemKeyOrder)) {
                if (!first) {
                    out.append(QLatin1Char('\n'));
                    out.append(QString(indent + 2, QLatin1Char(' ')));
                }
                first = false;
                out.append(key).append(QStringLiteral(": "));
                emitValue(out, m.value(key), indent + 2);
            }
        } else if (v.typeId() == QMetaType::QVariantList) {
            emitList(out, v.toList(), indent + 2);
        } else {
            emitScalar(out, v, indent);
        }
    }
}

void emitMap(QString &out, const QVariantMap &m, int indent,
             const QStringList &keyOrder = {},
             const QHash<QString, QStringList> &nestedItemOrder = {},
             const QHash<QString, QStringList> &nestedKeyOrder = {})
{
    const QString pad(indent, QLatin1Char(' '));
    bool first = true;
    for (const QString &key : orderedKeys(m, keyOrder)) {
        if (!first) out.append(QLatin1Char('\n'));
        first = false;
        out.append(pad).append(key).append(QStringLiteral(":"));
        const QVariant &v = m.value(key);
        if (v.typeId() == QMetaType::QVariantMap) {
            const QVariantMap sub = v.toMap();
            // An empty map must stay `{}` and not become a bare key, which
            // would re-parse as null. Obsidian's `summaries: {}` depends on
            // this.
            if (sub.isEmpty()) out.append(QStringLiteral(" {}"));
            else {
                out.append(QLatin1Char('\n'));
                // Per-key submap key order lets the caller preserve user-
                // authored insertion order for `properties:`/`formulas:`/
                // `summaries:` blocks even though the underlying QVariantMap
                // sorts alphabetically. Without this, every save reshuffles
                // user-keyed dicts (audit: bases.md §"On-disk `.base` format
                // compatibility").
                emitMap(out, sub, indent + 2, nestedKeyOrder.value(key));
            }
        } else if (v.typeId() == QMetaType::QVariantList) {
            // Per-key item key-order lookup lets the caller dictate the
            // canonical shape of map items inside specific lists (e.g.
            // `views:` items use BasesViewConfig's canonical order).
            emitList(out, v.toList(), indent + 2,
                     nestedItemOrder.value(key));
        } else {
            out.append(QLatin1Char(' '));
            emitScalar(out, v, indent);
        }
    }
}

void emitValue(QString &out, const QVariant &v, int indent)
{
    if (v.typeId() == QMetaType::QVariantMap) {
        const QVariantMap sub = v.toMap();
        if (sub.isEmpty()) { out.append(QStringLiteral(" {}")); return; }
        out.append(QLatin1Char('\n'));
        // Nested maps must be indented past the key that introduced them.
        // `indent` here is already the *content* indent (key column), so
        // the sub-map keys need indent+2 to satisfy YAML block-mapping rules.
        emitMap(out, sub, indent + 2);
    } else if (v.typeId() == QMetaType::QVariantList) {
        emitList(out, v.toList(), indent);
    } else {
        emitScalar(out, v, indent);
    }
}

}  // namespace

std::unique_ptr<BasesQuery> BasesQuery::fromString(const QString &text, QString *parseError)
{
    auto q = std::make_unique<BasesQuery>();

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        // Audit §3 invariant: empty file -> default 1-view "Table".
        q->views.push_back(std::make_unique<BasesViewConfig>());
        q->views.back()->type = QStringLiteral("table");
        q->views.back()->name = QStringLiteral("All");
        return q;
    }

    QString yerr;
    auto root = Markoff::YamlValue::parse(text, &yerr);
    if (!root.isMap()) {
        if (parseError)
            *parseError = yerr.isEmpty()
                ? QStringLiteral("root must be a map")
                : yerr;
        q->views.push_back(std::make_unique<BasesViewConfig>());
        q->views.back()->type = QStringLiteral("table");
        q->views.back()->name = QStringLiteral("All");
        return q;
    }

    QVariantMap legacyDisplay;

    root.forEach([&](const QString &key, const Markoff::YamlValue &v) {
        const QString k = key.toLower();
        if (k == QLatin1String("views") && v.isSeq()) {
            for (int i = 0, n = v.size(); i < n; ++i) {
                QString err;
                auto vc = BasesViewConfig::fromYaml(v.at(i), &err);
                if (vc) q->views.push_back(std::move(vc));
            }
        } else if (k == QLatin1String("filters")) {
            q->filters = parseFilter(v);
        } else if (k == QLatin1String("formulas") && v.isMap()) {
            v.forEach([&q](const QString &fname, const Markoff::YamlValue &fv) {
                if (!q->formulas.contains(fname))
                    q->formulaOrder.append(fname);
                q->formulas.insert(fname, Formula(fv.asString()));
            });
        } else if (k == QLatin1String("summaries") && v.isMap()) {
            q->summariesSeen = true;
            v.forEach([&q](const QString &fname, const Markoff::YamlValue &fv) {
                if (!q->summaryFormulas.contains(fname))
                    q->summaryFormulaOrder.append(fname);
                q->summaryFormulas.insert(fname, Formula(fv.asString()));
            });
        } else if (k == QLatin1String("properties") && v.isMap()) {
            v.forEach([&q](const QString &pk, const Markoff::YamlValue &pv) {
                PropertyConfig pc;
                pc.rawKey = pk;
                if (pv.isMap()) {
                    // Anything that is not a string `displayName` is
                    // unrecognized and round-trips (1764-jX.js:259-266).
                    pv.forEach([&pc](const QString &ck, const Markoff::YamlValue &cv) {
                        if (ck == QLatin1String("displayName") && cv.isString())
                            pc.displayName = cv.asString();
                        else
                            pc.unrecognizedData.insert(ck, yamlToVariant(cv));
                    });
                }
                const PropertyId id = parsePropertyId(pk);
                auto existing = q->properties.find(id);
                if (existing == q->properties.end()) {
                    q->propertyOrder.push_back(id);
                    q->properties.insert(id, pc);
                } else if (existing->rawKey.isEmpty()) {
                    existing->rawKey = pc.rawKey;
                }
            });
        } else if (k == QLatin1String("display") && v.isMap()) {
            // Legacy key — migrate into properties[*].displayName.
            v.forEach([&legacyDisplay](const QString &pk, const Markoff::YamlValue &pv) {
                legacyDisplay.insert(pk, pv.asString());
            });
        } else if (k == QLatin1String("newitemfolder")) {
            q->newItemFolder = v.asString();
        } else if (k == QLatin1String("newitemtemplate")) {
            q->newItemTemplate = v.asString();
        }
        // Unknown top-level keys -> unrecognizedData (preserved round-trip).
        // yamlToVariant recurses into nested maps/sequences so a future
        // Obsidian `.base` key with structured value isn't silently flattened
        // or dropped on save (the emit side already round-trips nested shapes).
        else {
            q->unrecognizedData.insert(key, yamlToVariant(v));
        }
    });

    // Apply legacy display -> properties.displayName migration.
    for (auto it = legacyDisplay.constBegin(); it != legacyDisplay.constEnd(); ++it) {
        const PropertyId id = parsePropertyId(it.key());
        auto pit = q->properties.find(id);
        if (pit == q->properties.end()) {
            PropertyConfig pc;
            pc.displayName = it.value().toString();
            q->properties.insert(id, pc);
            q->propertyOrder.push_back(id);
        } else {
            if (pit->displayName.isEmpty()) pit->displayName = it.value().toString();
        }
    }

    if (q->views.empty()) {
        auto def = std::make_unique<BasesViewConfig>();
        def->type = QStringLiteral("table");
        def->name = QStringLiteral("All");
        q->views.push_back(std::move(def));
    }
    return q;
}

QString BasesQuery::toString() const
{
    // Emission order is Obsidian's `getSerializable()` order
    // (RE/obsidian/1.13.7/renamed/src/app/own/1766-lQ.js:204-230):
    // unrecognizedData → filters → formulas → summaries → properties → views
    // → newItemFolder → newItemTemplate. `unrecognizedData` is placed first
    // here rather than last because it seeds the object there and every later
    // assignment overwrites it; the visible order of those keys is unchanged.
    static const QStringList kRootOrder = {
        QStringLiteral("filters"),
        QStringLiteral("formulas"),
        QStringLiteral("summaries"),
        QStringLiteral("properties"),
        QStringLiteral("views"),
        QStringLiteral("newItemFolder"),
        QStringLiteral("newItemTemplate"),
    };
    // Per-view order from `BasesViewConfig::serialize()` (1766-lQ.js:9-32).
    // Free-form view keys (BasesViewConfig::data) follow via the tail.
    static const QStringList kViewOrder = {
        QStringLiteral("type"),
        QStringLiteral("name"),
        QStringLiteral("filters"),
        QStringLiteral("groupBy"),
        QStringLiteral("order"),
        QStringLiteral("sort"),
        QStringLiteral("limit"),
        QStringLiteral("summaries"),
    };

    QVariantMap root;

    if (filters) root.insert(QStringLiteral("filters"), filters->serialize());

    if (!views.empty()) {
        QVariantList vs;
        for (const auto &v : views) if (v) vs.append(v->toMap());
        root.insert(QStringLiteral("views"), vs);
    }

    // `properties:` keys are emitted exactly as authored (PropertyConfig::rawKey)
    // and the whole block is dropped when every config would serialise empty
    // (1766-lQ.js:223-227 + 1764-jX.js:267-269).
    if (!properties.isEmpty()) {
        QVariantMap props;
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            QVariantMap pm;
            for (auto u = it->unrecognizedData.constBegin();
                 u != it->unrecognizedData.constEnd(); ++u)
                pm.insert(u.key(), u.value());
            if (!it->displayName.isEmpty())
                pm.insert(QStringLiteral("displayName"), it->displayName);
            if (pm.isEmpty()) continue;
            props.insert(it->rawKey.isEmpty() ? buildPropertyId(it.key()) : it->rawKey, pm);
        }
        if (!props.isEmpty()) root.insert(QStringLiteral("properties"), props);
    }

    if (!formulas.isEmpty()) {
        QVariantMap fs;
        for (auto it = formulas.constBegin(); it != formulas.constEnd(); ++it)
            fs.insert(it.key(), it.value().source());
        root.insert(QStringLiteral("formulas"), fs);
    }
    // `formulas:` is length-guarded above but `summaries:` is not: an emptied
    // summary map still emits (1766-lQ.js:206-222).
    if (!summaryFormulas.isEmpty() || summariesSeen) {
        QVariantMap ss;
        for (auto it = summaryFormulas.constBegin(); it != summaryFormulas.constEnd(); ++it)
            ss.insert(it.key(), it.value().source());
        root.insert(QStringLiteral("summaries"), ss);
    }

    if (newItemFolder) root.insert(QStringLiteral("newItemFolder"), *newItemFolder);
    if (newItemTemplate) root.insert(QStringLiteral("newItemTemplate"), *newItemTemplate);

    // Preserve unrecognizedData for forward-compat round-trip. Keys not named
    // in kRootOrder land after the canonical keys via orderedKeys()'s
    // natural-order tail.
    for (auto it = unrecognizedData.constBegin(); it != unrecognizedData.constEnd(); ++it) {
        if (!root.contains(it.key())) root.insert(it.key(), it.value());
    }

    // Per-key ordering for the user-keyed dicts. Tracked at parse time in
    // formulaOrder/summaryFormulaOrder/propertyOrder so round-trip
    // preserves the author's insertion order rather than alphabetising.
    QHash<QString, QStringList> nestedKeyOrder;
    if (!propertyOrder.empty()) {
        QStringList propKeys;
        propKeys.reserve(int(propertyOrder.size()));
        for (const PropertyId &id : propertyOrder) {
            auto it = properties.constFind(id);
            propKeys.append(it != properties.constEnd() && !it->rawKey.isEmpty()
                                ? it->rawKey
                                : buildPropertyId(id));
        }
        nestedKeyOrder.insert(QStringLiteral("properties"), propKeys);
    }
    if (!formulaOrder.isEmpty())
        nestedKeyOrder.insert(QStringLiteral("formulas"), formulaOrder);
    if (!summaryFormulaOrder.isEmpty())
        nestedKeyOrder.insert(QStringLiteral("summaries"), summaryFormulaOrder);

    QString out;
    if (!root.isEmpty())
        emitMap(out, root, 0, kRootOrder,
                {{ QStringLiteral("views"), kViewOrder }},
                nestedKeyOrder);

    if (!out.isEmpty()) out.append(QLatin1Char('\n'));
    return out;
}

std::unique_ptr<BasesQuery> BasesQuery::clone() const
{
    return fromString(toString());
}

BasesViewConfig *BasesQuery::getViewConfig(const QString &name) const
{
    if (views.empty()) return nullptr;
    if (name.isEmpty()) return views.front().get();
    for (const auto &v : views)
        if (v && v->name == name) return v.get();
    return views.front().get();
}

}  // namespace Corbomite::Bases
