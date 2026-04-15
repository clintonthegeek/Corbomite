// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Hotkey.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Corbomite {

QString hotkeyModifierToString(HotkeyModifier m)
{
    switch (m) {
    case HotkeyModifier::Mod:   return QStringLiteral("Mod");
    case HotkeyModifier::Ctrl:  return QStringLiteral("Ctrl");
    case HotkeyModifier::Meta:  return QStringLiteral("Meta");
    case HotkeyModifier::Shift: return QStringLiteral("Shift");
    case HotkeyModifier::Alt:   return QStringLiteral("Alt");
    }
    return {};
}

bool hotkeyModifierFromString(const QString &s, HotkeyModifier *out)
{
    if (s == QLatin1String("Mod"))   { if (out) *out = HotkeyModifier::Mod;   return true; }
    if (s == QLatin1String("Ctrl"))  { if (out) *out = HotkeyModifier::Ctrl;  return true; }
    if (s == QLatin1String("Meta"))  { if (out) *out = HotkeyModifier::Meta;  return true; }
    if (s == QLatin1String("Shift")) { if (out) *out = HotkeyModifier::Shift; return true; }
    if (s == QLatin1String("Alt"))   { if (out) *out = HotkeyModifier::Alt;   return true; }
    return false;
}

HotkeyFile HotkeyFile::parse(const QByteArray &json)
{
    HotkeyFile out;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return out;
    }
    const auto root = doc.object();
    // QJsonObject iteration order is not guaranteed alphabetical — Qt6
    // preserves insertion order as parsed. Walk it in that order.
    const auto keys = root.keys();
    for (const auto &id : keys) {
        const auto val = root.value(id);
        if (!val.isArray()) continue; // malformed entries silently dropped
        const auto arr = val.toArray();
        QList<Hotkey> list;
        for (const auto &entry : arr) {
            if (!entry.isObject()) continue;
            const auto obj = entry.toObject();
            Hotkey hk;
            const auto mods = obj.value(QStringLiteral("modifiers")).toArray();
            for (const auto &m : mods) {
                HotkeyModifier parsed;
                if (hotkeyModifierFromString(m.toString(), &parsed)) {
                    hk.modifiers.append(parsed);
                }
            }
            hk.key = obj.value(QStringLiteral("key")).toString();
            list.append(hk);
        }
        out.order.append(id);
        out.bindings.insert(id, list);
    }
    return out;
}

QByteArray HotkeyFile::serialise() const
{
    // Build the JSON manually so we control key order and indent. Qt's
    // QJsonDocument::toJson(Indented) uses 4-space indent; Obsidian uses 2.
    QByteArray out;
    out.append("{");

    bool first = true;
    const auto emitEntry = [&](const QString &id) {
        auto it = bindings.constFind(id);
        if (it == bindings.constEnd()) return;
        if (!first) out.append(",");
        first = false;
        out.append("\n  \"");
        out.append(id.toUtf8());
        out.append("\": [");
        const auto &list = it.value();
        for (int i = 0; i < list.size(); ++i) {
            if (i > 0) out.append(",");
            out.append("\n    {\n      \"modifiers\": [");
            const auto &hk = list.at(i);
            for (int j = 0; j < hk.modifiers.size(); ++j) {
                if (j > 0) out.append(", ");
                out.append("\"");
                out.append(hotkeyModifierToString(hk.modifiers.at(j)).toUtf8());
                out.append("\"");
            }
            out.append("],\n      \"key\": \"");
            out.append(hk.key.toUtf8());
            out.append("\"\n    }");
        }
        if (!list.isEmpty()) {
            out.append("\n  ]");
        } else {
            out.append("]");
        }
    };

    // Walk in declared order; fall back to hash order for keys that are
    // in bindings but missing from order (shouldn't happen, but be lenient).
    QStringList walked;
    for (const auto &id : order) {
        if (bindings.contains(id)) {
            emitEntry(id);
            walked.append(id);
        }
    }
    const auto allKeys = bindings.keys();
    for (const auto &id : allKeys) {
        if (!walked.contains(id)) emitEntry(id);
    }

    out.append("\n}");
    return out;
}

} // namespace Corbomite
