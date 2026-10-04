// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/Hotkey.h"

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace Corbomite {

/// Merges `.obsidian/hotkeys.json` over the built-in defaults the way
/// Obsidian's `HotkeyManager.bake()` does
/// (`RE/obsidian/1.13.7/renamed/src/app/own/0833-$w.js:57-76`):
///
/// ```js
/// for (var o in t) t.hasOwnProperty(o) && r(o, t[o]);                     // user keys
/// for (var o in e) e.hasOwnProperty(o) && !t.hasOwnProperty(o) && r(o, e[o]); // defaults, absent ids only
/// ```
///
/// The gate is `hasOwnProperty`, not "the value is non-empty", so the three
/// cases are:
///
///   - id absent        → the default applies;
///   - id → `[binding]` → only that binding applies;
///   - id → `[]`        → **no** binding at all, and the default is *not*
///                        restored. An empty array is Obsidian's only way to
///                        express an unbind.
///
/// A reader that treats `[]` as "no opinion" silently resurrects the default
/// the user just deleted.
class HotkeyManager
{
public:
    HotkeyManager() = default;

    void addDefaultHotkeys(const QString &commandId, const QList<Hotkey> &bindings);
    void removeDefaultHotkeys(const QString &commandId);
    QList<Hotkey> defaultHotkeys(const QString &commandId) const;

    /// The user's override list for `commandId`. An empty optional means the
    /// id is **absent** from the file — which is not the same as an empty
    /// list, which means "unbound".
    std::optional<QList<Hotkey>> customHotkeys(const QString &commandId) const;
    void setCustomHotkeys(const QString &commandId, const QList<Hotkey> &bindings);
    void removeCustomHotkeys(const QString &commandId);

    /// Replace the override map from a `hotkeys.json` body. Returns false and
    /// leaves the map untouched when the body is not a JSON object — Obsidian
    /// gates `load()` on `typeof === "object"` (`0833-$w.js:38`) and silently
    /// keeps the previous map otherwise.
    ///
    /// Deliberate divergence: a JSON *array* is rejected too. Obsidian accepts
    /// it (JS `typeof [] === "object"`) and then bakes index-named commands
    /// ("0", "1", …) out of it; a top-level array in `hotkeys.json` is
    /// corruption, not a binding table.
    bool load(const QByteArray &json);

    /// The `hotkeys.json` body for the overrides only. Never merges the
    /// defaults in: Obsidian's `save()` writes `customKeys` verbatim
    /// (`0833-$w.js:24`), and defaults written back would be read as user
    /// overrides on the next load.
    QByteArray save() const;

    /// Effective bindings for `commandId` — the override list when the id is
    /// present (empty list included), otherwise the default list.
    QList<Hotkey> effectiveHotkeys(const QString &commandId) const;

    bool isBound(const QString &commandId) const
    {
        return !effectiveHotkeys(commandId).isEmpty();
    }

    /// Command ids carrying a user override, in file order.
    QStringList overriddenIds() const;

private:
    QHash<QString, QList<Hotkey>> m_defaults;
    QHash<QString, QList<Hotkey>> m_custom;
    QStringList m_customOrder;
};

/// Obsidian 1.13.7's built-in default hotkeys: the 41 command ids that carry
/// a `hotkeys:` field, in 1.13.7 that is every such spec reachable from
/// `Commands.addCommand`, which is the only thing that ever calls
/// `addDefaultHotkeys` (`app/own/1963-s5.js:22`):
///
///   - 16 from `App.registerCommands` (`app/own/2075-Vie.js:609-1729`),
///   - 21 from `Workspace` (`app/own/1932-r4.js:311-770`, counting
///     `workspace:goto-tab-1..8`),
///   - 4 from core plugins (`global-search:open`, `graph:open`,
///     `command-palette:open`, `switcher:open`).
///
/// The plugin half is conditional in Obsidian — `registerGlobalCommand`
/// output only reaches `app.commands` when the owning plugin is enabled
/// (`app/own/1939-m4.js:560`) — but all four owning plugins are `defaultOn`,
/// so the table is the stock-vault set. This function takes no enabled-plugin
/// list, so a vault that switches one off keeps the default; that divergence
/// only ever adds a binding Obsidian would have dropped.
///
/// Four ids are platform-conditional, so a faithful reader cannot assume one
/// default per id and `macOs` selects between them:
/// `editor:open-search-replace` (`Zp ? Mod+Alt+F : Mod+H`),
/// `workspace:next-tab` and `workspace:previous-tab` (macOS swaps the second
/// binding for Meta+Shift+`]` / Meta+Shift+`[`).
QHash<QString, QList<Hotkey>> obsidianDefaultHotkeys(bool macOs);

} // namespace Corbomite
