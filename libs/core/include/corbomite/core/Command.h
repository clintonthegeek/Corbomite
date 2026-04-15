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

    /// Remove a command by id. Returns true if it was registered.
    bool removeCommand(const QString &id);

    /// Look up by id. Returns nullptr if unknown.
    Command *findCommand(const QString &id);
    const Command *findCommand(const QString &id) const;

    /// All registered commands, insertion order.
    QVector<Command *> listCommands();
    QVector<const Command *> listCommands() const;

    /// Subset of listCommands() that are currently available (i.e.
    /// a non-check callback is set, or the check returns true).
    QVector<Command *> listAvailable();

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
};

} // namespace Corbomite
