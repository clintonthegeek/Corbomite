// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QVector>

#include <functional>
#include <variant>

namespace Corbomite {

/// Opaque editor handle. A later Cluster (G — Views hierarchy) will replace
/// this with a real type (`MarkdownEditor *` / `TextFileView *` pair).
/// For now the registry is editor-agnostic so plugin-like callers can
/// declare editor-aware commands without a hard dependency.
using EditorLike = void *;

/// Command declaration, modeled on Obsidian's `Command` type
/// (domains/plugin.md §10, domains/core.md §7).
///
/// A command carries exactly one callback in its variant — which
/// callback is set determines how the command integrates with the
/// palette and hotkey system:
///
///   - `callback` — fires always; no availability check.
///   - `checkCallback(checking)` — when `checking=true`, returns
///     availability without side effects; when `checking=false`,
///     executes if available.
///   - `editorCallback(editor)` — fires only when an active editor
///     is present.
///   - `editorCheckCallback(checking, editor)` — like checkCallback,
///     but with editor context.
///
/// Corbomite preserves Obsidian's "unset callbacks" quirk: if none
/// are set, the command still registers but cannot execute.
struct Command
{
    using SimpleCallback = std::function<void()>;
    using CheckCallback = std::function<bool(bool checking)>;
    using EditorCallback = std::function<void(EditorLike editor)>;
    using EditorCheckCallback = std::function<bool(bool checking, EditorLike editor)>;

    QString id;            ///< Canonical id (plugins namespace as "pluginId:localId")
    QString name;          ///< Human-readable label for command palette
    QString icon;          ///< Optional icon name (Qt theme lookup)
    bool mobileOnly = false;

    /// Keep this entry out of the command palette.
    ///
    /// Set for entries that exist only so an Obsidian command id is
    /// addressable — hotkey dispatch, `.obsidian/hotkeys.json`, view hamburger
    /// menus — while the same verb is *already* offered by a KAction in the
    /// window's collection. The palette builds one group from the actions and
    /// one from the registry, so without this the identical label appears
    /// twice (`workspace:move-to-new-window` under Tabs and again under
    /// Commands, seen live 2026-10-18). Obsidian has no such second layer: its
    /// palette lists commands, and a command *is* the action.
    bool paletteHidden = false;

    /// Non-empty ⇒ this entry is a second name for `aliasOf`, not a command of
    /// its own. Set only by `CommandRegistry::addAlias`; a spec that sets it by
    /// hand gets its callbacks ignored.
    QString aliasOf;

    SimpleCallback callback;
    CheckCallback checkCallback;
    EditorCallback editorCallback;
    EditorCheckCallback editorCheckCallback;
};

/// Central registry for all registered commands. Owned by the App-level
/// service; `KCommandBar` (command palette) pulls its rows from
/// `listAvailable()`; hotkey dispatch calls `executeById()`.
class CommandRegistry
{
public:
    CommandRegistry() = default;
    ~CommandRegistry() = default;

    CommandRegistry(const CommandRegistry &) = delete;
    CommandRegistry &operator=(const CommandRegistry &) = delete;

    /// Register (or replace) a command. Replacement uses `id` as key;
    /// name/icon/callbacks are all swapped.
    void addCommand(const Command &cmd);

    /// Register `aliasId` as a second name for the already-registered
    /// `targetId`.
    ///
    /// `.obsidian/hotkeys.json` is keyed by Obsidian's command id
    /// (`docs/obsidian-audit/addenda/2026-10-03-core-command-and-hotkey-contracts.md`
    /// §D), so a vault's override only lands if the Obsidian id is registered.
    /// Corbomite's built-ins predate that contract and are registered under
    /// Corbomite-native ids (`corbomite-backlinks:open`), which nothing in a
    /// vault's config can name. An alias closes the gap without duplicating the
    /// callbacks — the two ids cannot drift apart, because there is only one
    /// callback — and without adding a palette row, since aliases are excluded
    /// from `listAvailable()`.
    ///
    /// Fails (returns false, registers nothing) if `aliasId` is empty, equals
    /// `targetId`, is already registered, or names an unregistered `targetId`.
    /// Chaining one alias onto another is refused too. Unlike `addCommand`,
    /// which mirrors Obsidian's own missing duplicate check, this is strict:
    /// an alias id is the name a vault's config binds, so two plugins claiming
    /// one is a bug to surface rather than a last-writer-wins to absorb.
    bool addAlias(const QString &aliasId, const QString &targetId);

    /// True if `id` is registered as an alias rather than a real command.
    bool isAlias(const QString &id) const;

    /// Remove a command by id. Returns true if it was registered.
    bool removeCommand(const QString &id);

    /// Look up by id. Returns nullptr if unknown.
    Command *findCommand(const QString &id);
    const Command *findCommand(const QString &id) const;

    /// All registered commands, insertion order. Includes aliases; use
    /// `listAvailable()` for the palette.
    QVector<Command *> listCommands();
    QVector<const Command *> listCommands() const;

    /// Subset of listCommands() that are currently available (i.e.
    /// a non-check callback is set, or the check returns true).
    QVector<Command *> listAvailable();

    /// The command palette's rows: `listAvailable()` minus the entries that
    /// exist only to make an Obsidian id addressable (`paletteHidden`). The
    /// palette also builds groups from the window's KActions, and a mirrored
    /// command carries that action's own label — so without this the same verb
    /// is listed twice (`workspace:move-to-new-window` under Tabs and again
    /// under Commands; seen live 2026-10-18). Use this in the palette and
    /// `listAvailable()` everywhere else.
    QVector<Command *> listPaletteCommands();

    /// True if the command is registered and currently available.
    bool isAvailable(const QString &id) const;

    /// Execute the command by id. Returns true if it ran, false if
    /// unknown or unavailable.
    bool executeById(const QString &id);

    /// Set the editor context consulted by editorCallback /
    /// editorCheckCallback commands. Nullptr clears.
    void setActiveEditor(EditorLike editor) { m_activeEditor = editor; }
    EditorLike activeEditor() const { return m_activeEditor; }

private:
    // Insertion-ordered storage (QHash alone doesn't preserve order).
    QList<QString> m_order;
    QHash<QString, Command> m_byId;
    EditorLike m_activeEditor = nullptr;

    bool isAvailableFor(const Command &cmd) const;

    /// Follow `aliasOf` to the underlying command. `hops` bounds the walk so a
    /// hand-built cycle degrades to "unavailable" instead of spinning.
    const Command *resolve(const Command &cmd, int hops = 8) const;
};

} // namespace Corbomite
