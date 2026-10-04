// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Hotkey.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QStringList>

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

namespace {

/// Obsidian's `Xw` (`app/own/0831-Xw.js:2-4`): only a four-character `KeyX`
/// code loses its prefix, so `KeyI` → `I` while `Digit3` and `ArrowLeft` reach
/// the matcher spelled as-is.
QString codeToMatchName(const QString &code)
{
    if (code.size() == 4 && code.startsWith(QLatin1String("Key"))) return code.mid(3);
    return code;
}

/// DOM `KeyboardEvent.key`/`.code` spellings that Qt's own key tables name
/// differently or not at all. Qt stays the parser — this renames, it does not
/// reimplement, so a `KKeySequenceWidget`-editable shortcut and a
/// `QKeySequence::fromString` round-trip stay in step with Qt.
const QHash<QString, QString> &qtSpellings()
{
    static const QHash<QString, QString> s_map = {
        {QStringLiteral("Enter"),        QStringLiteral("Return")},
        {QStringLiteral("ArrowLeft"),    QStringLiteral("Left")},
        {QStringLiteral("ArrowRight"),   QStringLiteral("Right")},
        {QStringLiteral("ArrowUp"),      QStringLiteral("Up")},
        {QStringLiteral("ArrowDown"),    QStringLiteral("Down")},
        {QStringLiteral("PageUp"),       QStringLiteral("PgUp")},
        {QStringLiteral("PageDown"),     QStringLiteral("PgDown")},
        {QStringLiteral("Minus"),        QStringLiteral("-")},
        {QStringLiteral("Equal"),        QStringLiteral("=")},
        {QStringLiteral("BracketLeft"),  QStringLiteral("[")},
        {QStringLiteral("BracketRight"), QStringLiteral("]")},
        {QStringLiteral("Backslash"),    QStringLiteral("\\")},
        {QStringLiteral("Semicolon"),    QStringLiteral(";")},
        {QStringLiteral("Quote"),        QStringLiteral("'")},
        {QStringLiteral("Comma"),        QStringLiteral(",")},
        {QStringLiteral("Period"),       QStringLiteral(".")},
        {QStringLiteral("Slash"),        QStringLiteral("/")},
        {QStringLiteral("Backquote"),    QStringLiteral("`")},
    };
    return s_map;
}

int qtKeyFor(const QString &name)
{
    if (name.isEmpty()) return 0;
    const QKeySequence ks = QKeySequence::fromString(qtSpellings().value(name, name));
    // Qt reports an unrecognised name as a *one-step* sequence holding
    // Qt::Key_unknown, not as an empty sequence — checking isEmpty() here
    // would let "NotAKey" through as a valid shortcut.
    if (ks.count() != 1 || ks[0].key() == Qt::Key_unknown) return 0;
    return ks[0].key();
}

Qt::KeyboardModifier qtModifier(HotkeyModifier m, bool macOs)
{
    switch (m) {
    case HotkeyModifier::Mod:   return macOs ? Qt::MetaModifier : Qt::ControlModifier;
    case HotkeyModifier::Ctrl:  return Qt::ControlModifier;
    case HotkeyModifier::Meta:  return Qt::MetaModifier;
    case HotkeyModifier::Shift: return Qt::ShiftModifier;
    case HotkeyModifier::Alt:   return Qt::AltModifier;
    }
    return Qt::NoModifier;
}

} // namespace

bool hotkeyPlatformIsMacOs()
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

QString Hotkey::resolvedKey() const
{
    return code.isEmpty() ? key : codeToMatchName(code);
}

int hotkeyCodeToQtKey(const QString &code)
{
    return qtKeyFor(codeToMatchName(code));
}

QKeySequence hotkeyToKeySequence(const Hotkey &h, bool macOs)
{
    if (!h.code.isEmpty()) return {};
    if (h.key.isEmpty()) return {};

    QStringList parts;
    Qt::KeyboardModifiers seen = Qt::NoModifier;
    for (const auto m : h.modifiers) {
        const auto qt = qtModifier(m, macOs);
        // `compileModifiers` sorts but never de-duplicates
        // (`0747-ly.js:112-115`), so `["Mod","Ctrl"]` compiles to "Ctrl,Ctrl"
        // and matches nothing in Obsidian. Qt would collapse it to one Ctrl, so
        // refuse the binding instead of inventing a shortcut Obsidian never had.
        if (seen & qt) return {};
        seen |= qt;
        switch (qt) {
        case Qt::ControlModifier: parts << QStringLiteral("Ctrl"); break;
        case Qt::MetaModifier:    parts << QStringLiteral("Meta"); break;
        case Qt::ShiftModifier:   parts << QStringLiteral("Shift"); break;
        case Qt::AltModifier:     parts << QStringLiteral("Alt"); break;
        default: break;
        }
    }
    // Qt's parser only accepts modifiers *before* the key: "Ctrl+Return+Shift"
    // resolves to Qt::Key_unknown, "Ctrl+Shift+Return" resolves correctly.
    parts << qtSpellings().value(h.key, h.key);
    const QKeySequence ks = QKeySequence::fromString(parts.join(QLatin1Char('+')));
    return ks.count() == 1 && ks[0].key() != Qt::Key_unknown ? ks : QKeySequence();
}

bool hotkeyMatches(const Hotkey &h, const QKeyEvent &e, bool macOs)
{
    Qt::KeyboardModifiers want = Qt::NoModifier;
    for (const auto m : h.modifiers) {
        const auto qt = qtModifier(m, macOs);
        if (want & qt) return false;
        want |= qt;
    }
    // Obsidian's Keymap has no keypad state to compare against; Qt reports
    // numpad keys as a modifier, and folding it into the set would make every
    // binding miss on a numpad press.
    if (want != (e.modifiers() & ~Qt::KeypadModifier)) return false;

    const QString name = h.resolvedKey();
    if (name.isEmpty()) return true;  // isMatch's `!i` wildcard arm

    const int qtKey = qtKeyFor(name);
    if (qtKey != 0) {
        if (e.key() == qtKey) return true;
        // The macOS caret case `onKeyEvent` special-cases
        // (`0747-ly.js:83`): Ctrl+Option+I reports "^" as its character, and a
        // `code` is what survives that. Match on the character only when the
        // binding actually carries a code, so a plain `key` binding stays
        // strictly key-based.
        return !h.code.isEmpty() && e.text().compare(name, Qt::CaseInsensitive) == 0;
    }
    // isMatch's third arm: no Qt key name for this spelling, so the character
    // is all that is left to compare.
    return !e.text().isEmpty() && e.text().compare(name, Qt::CaseInsensitive) == 0;
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
            hk.code = obj.value(QStringLiteral("code")).toString();
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
            out.append("\"");
            if (!hk.code.isEmpty()) {
                out.append(",\n      \"code\": \"");
                out.append(hk.code.toUtf8());
                out.append("\"");
            }
            out.append("\n    }");
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
