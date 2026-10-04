// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

class QKeyEvent;
class QKeySequence;

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

// --- Dispatch half: Hotkey → Qt ---------------------------------------------
//
// Everything below is the `code` problem made concrete. Obsidian's `bake()`
// folds `code` through `Xw` into the same `key` slot the character sits in
// (`0833-$w.js:69`) and then compares *that string*, so a binding with a
// `code` is matched as a name — but a name a `QKeySequence` cannot express
// (`Xw("KeyI")` is `"I"`, which it can; `NumpadAdd`, which it cannot).
// Qt's own key tables are the parser in both directions; the only translation
// Corbomite supplies is renaming the DOM spellings Qt calls something else.

/// True when "Mod" resolves to Meta rather than Ctrl. A function rather than a
/// `Q_OS_MACOS` default argument because that macro is undefined off macOS.
bool hotkeyPlatformIsMacOs();

/// The `Qt::Key` a binding's `code` matches, or 0 when `code` is not a key
/// Qt can name. 0 is load-bearing: a dispatcher must refuse such a binding and
/// say so rather than fall back to some other key.
int hotkeyCodeToQtKey(const QString &code);

/// The binding as a `QKeySequence` ("Ctrl+I", "Ctrl+Shift+F"), for handing to
/// a `QAction` so the shortcut shows in the menu and stays editable in
/// `KKeySequenceWidget`. Empty when the binding cannot be spelled as one:
/// a `code` (a physical key, which `QKeySequence` has no syntax for) or
/// duplicated modifiers (Obsidian compiles those to a set that matches
/// nothing — `0747-ly.js:112-115` never de-duplicates).
QKeySequence hotkeyToKeySequence(const Hotkey &h, bool macOs = hotkeyPlatformIsMacOs());

/// Obsidian's `Keymap.isMatch` (`0747-ly.js:130-133`): the modifier sets must
/// be equal (an extra held modifier breaks the match) and the key must match
/// the event's key or, failing that, its character case-insensitively.
bool hotkeyMatches(const Hotkey &h, const QKeyEvent &e,
                   bool macOs = hotkeyPlatformIsMacOs());

} // namespace Corbomite
