// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesViewConfig.h"

#include "markoff-parser/YamlValue.h"

#include <QVariantList>

namespace Corbomite::Bases {

namespace {

QVariant yamlToVariant(const Markoff::YamlValue &v)
{
    using K = Markoff::YamlValue::Kind;
    switch (v.kind()) {
    case K::Null:   return QVariant{};
    case K::Bool:   return QVariant::fromValue(v.asBool());
    case K::Int:    return QVariant::fromValue(static_cast<qlonglong>(v.asInt()));
    case K::Double: return QVariant::fromValue(v.asDouble());
    case K::String: return QVariant::fromValue(v.asString());
    case K::Seq: {
        QVariantList out;
        for (int i = 0, n = v.size(); i < n; ++i)
            out.append(yamlToVariant(v.at(i)));
        return out;
    }
    case K::Map: {
        QVariantMap m;
        v.forEach([&m](const QString &k, const Markoff::YamlValue &cv) {
            m.insert(k, yamlToVariant(cv));
        });
        return m;
    }
    }
    return {};
}

QString variantToString(const QVariant &v)
{
    return v.toString();
}

}  // namespace

std::unique_ptr<BasesViewConfig> BasesViewConfig::fromYaml(const Markoff::YamlValue &node,
                                                            QString *errorOut)
{
    if (!node.isMap()) {
        if (errorOut) *errorOut = QStringLiteral("view entry must be a map");
        return nullptr;
    }
    auto cfg = std::make_unique<BasesViewConfig>();

    node.forEach([&](const QString &key, const Markoff::YamlValue &v) {
        const QString k = key.toLower();
        if (k == QLatin1String("type"))      cfg->type = v.asString();
        else if (k == QLatin1String("name")) cfg->name = v.asString();
        else if (k == QLatin1String("limit")) cfg->limit = static_cast<int>(v.asInt());
        else if (k == QLatin1String("filters")) cfg->filters = parseFilter(v);
        else if (k == QLatin1String("order") && v.isSeq()) {
            for (int i = 0, n = v.size(); i < n; ++i)
                cfg->order.push_back(parsePropertyId(v.at(i).asString()));
        }
        else if (k == QLatin1String("sort") && v.isSeq()) {
            for (int i = 0, n = v.size(); i < n; ++i) {
                const auto m = v.at(i);
                if (!m.isMap()) continue;
                SortKey sk;
                sk.property = parsePropertyId(m.get(QStringLiteral("property")).asString());
                sk.direction = m.get(QStringLiteral("direction")).asString().toUpper();
                if (sk.direction.isEmpty()) sk.direction = QStringLiteral("ASC");
                cfg->sort.push_back(sk);
            }
        }
        else if (k == QLatin1String("groupby") && v.isMap()) {
            GroupBy gb;
            gb.property = parsePropertyId(v.get(QStringLiteral("property")).asString());
            gb.direction = v.get(QStringLiteral("direction")).asString().toUpper();
            if (gb.direction.isEmpty()) gb.direction = QStringLiteral("ASC");
            cfg->groupBy = gb;
        }
        else if (k == QLatin1String("summaries") && v.isMap()) {
            v.forEach([&](const QString &pk, const Markoff::YamlValue &sv) {
                cfg->summaries.insert(parsePropertyId(pk), sv.asString());
            });
        }
        else {
            cfg->unrecognizedData.insert(key, yamlToVariant(v));
        }
    });

    // Sensible defaults for required fields.
    if (cfg->type.isEmpty()) cfg->type = QStringLiteral("table");
    if (cfg->name.isEmpty()) cfg->name = QStringLiteral("All");
    Q_UNUSED(variantToString);
    return cfg;
}

QVariantMap BasesViewConfig::toMap() const
{
    QVariantMap m;
    m.insert(QStringLiteral("type"), type);
    m.insert(QStringLiteral("name"), name);
    if (filters) m.insert(QStringLiteral("filters"), filters->serialize());
    if (!order.isEmpty()) {
        QVariantList xs;
        for (const auto &p : order) xs.append(buildPropertyId(p));
        m.insert(QStringLiteral("order"), xs);
    }
    if (!sort.isEmpty()) {
        QVariantList xs;
        for (const auto &sk : sort) {
            QVariantMap e;
            e.insert(QStringLiteral("property"), buildPropertyId(sk.property));
            e.insert(QStringLiteral("direction"), sk.direction);
            xs.append(e);
        }
        m.insert(QStringLiteral("sort"), xs);
    }
    if (groupBy) {
        QVariantMap gb;
        gb.insert(QStringLiteral("property"), buildPropertyId(groupBy->property));
        gb.insert(QStringLiteral("direction"), groupBy->direction);
        m.insert(QStringLiteral("groupBy"), gb);
    }
    if (limit > 0) m.insert(QStringLiteral("limit"), limit);
    if (!summaries.isEmpty()) {
        QVariantMap s;
        for (auto it = summaries.constBegin(); it != summaries.constEnd(); ++it)
            s.insert(buildPropertyId(it.key()), it.value());
        m.insert(QStringLiteral("summaries"), s);
    }
    // Preserve unrecognizedData last for forward-compat round-trip.
    for (auto it = unrecognizedData.constBegin(); it != unrecognizedData.constEnd(); ++it) {
        if (!m.contains(it.key())) m.insert(it.key(), it.value());
    }
    return m;
}

}  // namespace Corbomite::Bases
