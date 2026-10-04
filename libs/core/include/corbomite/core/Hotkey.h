// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace Corbomite {

/// Modifier mnemonics as they appear in `.obsidian/hotkeys.json`.
/// "Mod" is Obsidian's platform-abstract token (resolves to Meta on
/// macOS, Ctrl elsewhere). The serialised form MUST preserve "Mod"
/// as-is — resolution is a runtime concern, not a serialisation one.
enum class HotkeyModifier {
    Mod,
    Ctrl,
    Meta,
    Shift,
    Alt,
};

/// A single hotkey binding. `key` matches Obsidian's on-disk spelling
/// (e.g. "b", "Enter", "ArrowUp"); translation to `Qt::Key` happens
/// at dispatch time in the Scope layer.
///
/// `code` is a DOM `KeyboardEvent.code` ("KeyI", "ArrowLeft"). Obsidian
/// stores it alongside `key` and lets it win when matching, so a
/// mac-authored override survives the layout where Ctrl+Option+I reports
/// "^" instead of "i".
struct Hotkey
{
    QList<HotkeyModifier> modifiers;
    QString key;
    QString code;

    /// The key a matcher compares against: `code` when set, else `key`.
    /// Obsidian's `Xw()` (`app/own/0831-Xw.js:2-4`) strips the `Key`
    /// prefix off a four-character `KeyX` code and passes anything else
    /// through untouched, so only a real letter key loses its prefix.
    QString resolvedKey() const;

    bool operator==(const Hotkey &o) const
    {
        return modifiers == o.modifiers && key == o.key && code == o.code;
    }
    bool operator!=(const Hotkey &o) const { return !(*this == o); }
};

/// Parsed representation of `.obsidian/hotkeys.json`.
///
///   { [commandId]: Hotkey[] }   // value [] = explicitly unbound
///
/// Preserves:
///   - Key insertion order (via `order`).
///   - Empty-array values (explicit unbind distinct from absent).
///   - Unknown command IDs (plugin may not be registered yet — we
///     still round-trip their bindings).
struct HotkeyFile
{
    QStringList order;                          ///< Command ids in file order.
    QHash<QString, QList<Hotkey>> bindings;     ///< id → hotkey list (may be empty).

    /// Parse a `hotkeys.json` body. Malformed entries are silently
    /// dropped. Obsidian's `load()` is even more permissive — it only
    /// checks `typeof === "object"` and never validates contents
    /// (`0833-$w.js:38`) — but its leniency is not free: `bake()` feeds
    /// `a.modifiers` straight to `compileModifiers`, which is
    /// `e.map(...)` (`0747-ly.js:112-115`), so a binding with no
    /// `modifiers` array throws inside the keydown handler.
    static HotkeyFile parse(const QByteArray &json);

    /// Serialise to Obsidian's canonical shape: 2-space indent, no
    /// trailing newline, keys in `order`.
    QByteArray serialise() const;
};

/// Stringify a single HotkeyModifier in its on-disk form. Exposed for
/// callers that want to build user-facing display strings.
QString hotkeyModifierToString(HotkeyModifier m);

/// Parse a modifier mnemonic. Returns false on unknown input.
bool hotkeyModifierFromString(const QString &s, HotkeyModifier *out);

} // namespace Corbomite
