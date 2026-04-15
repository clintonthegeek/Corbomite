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
struct Hotkey
{
    QList<HotkeyModifier> modifiers;
    QString key;

    bool operator==(const Hotkey &o) const
    {
        return modifiers == o.modifiers && key == o.key;
    }
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
    /// dropped (Obsidian's parser is similarly lenient).
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
