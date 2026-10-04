// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/Hotkey.h"
#include "corbomite/core/HotkeyManager.h"
#include "corbomite/core/Scope.h"

#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class QAction;
class QKeyEvent;

namespace Corbomite {

class CommandRegistry;

/// Delivers `.obsidian/hotkeys.json` overrides to real shortcuts.
///
/// `HotkeyManager` merges the file over the defaults; this is the half that
/// makes the merge mean anything. Registering the Obsidian command ids is not
/// sufficient on its own — without a dispatcher, an override is inert.
///
/// Two delivery paths, one per binding, so nothing fires twice:
///
///   - A binding Qt can spell as a `QKeySequence` goes onto the command's
///     `QAction`. That keeps the shortcut visible in the menu, editable in
///     `Settings` → Shortcuts, and matchingable by Qt's own machinery. It also
///     covers the unbind: an empty override list clears the action's shortcut
///     rather than leaving the stock chord live behind it.
///   - Every other binding of an overridden id — a `code` binding (a physical
///     key, which `QKeySequence` has no syntax for) and any binding past the
///     first — is matched here, at event level, through `ScopeManager`'s root
///     slot, which is what the `Scope` class comment calls the root.
///
/// The unbinding rule is end-to-end by construction: `load()` runs
/// `reconcile()`, so a command whose id is present with `[]` has no shortcut on
/// its action *and* no entry in the dispatch table. It cannot fall back.
class HotkeyDispatcher : public QObject
{
    Q_OBJECT
public:
    explicit HotkeyDispatcher(CommandRegistry *registry, QObject *parent = nullptr);
    ~HotkeyDispatcher() override;

    /// Merge a `hotkeys.json` body over the defaults and re-arm both delivery
    /// paths. Returns false — leaving the previous table armed — when the body
    /// is not a JSON object, which is Obsidian's `typeof === "object"` gate
    /// (`0833-$w.js:38`).
    bool load(const QByteArray &json);

    /// Declare that `commandId` is driven by `action`, so an override has to
    /// win over the action's own shortcut (and an unbind has to clear it).
    /// Only ids a vault's `hotkeys.json` can name should be bound.
    void bindAction(const QString &commandId, QAction *action);

    /// Anchor this dispatcher to the process-wide key dispatcher: the
    /// dispatcher's own `Scope` becomes `ScopeManager`'s root slot. Until it is
    /// called, overrides only move `QAction` shortcuts.
    void install();

    /// Release the root slot and restore every bound action's shortcut. Safe to
    /// call more than once.
    void uninstall();

    /// Restore every bound action's own shortcut and forget the bindings,
    /// without touching the root slot. A host whose `QAction`s die before the
    /// dispatcher (MainWindow's `KActionCollection` is deleted by the
    /// `KXmlGuiWindow` base destructor, which runs *after* `QObject` has
    /// deleted this child) must call this while the actions are still alive.
    void clearActions();

    HotkeyManager &hotkeys() { return m_hotkeys; }
    const HotkeyManager &hotkeys() const { return m_hotkeys; }

    /// One `"<commandId>: <reason>"` line per binding that could not be
    /// delivered — an unmappable `code`, a modifier set that compiles to
    /// nothing, a key name Qt does not know. Empties on every successful
    /// `load()`. Reported rather than dropped: a binding silently mismatching
    /// (or matching the wrong key) is worse than one the user is told about.
    QStringList unresolved() const { return m_unresolved; }

    /// Bindings currently matched at event level, in table order. Exposed for
    /// tests; the delivery itself is private.
    int dispatchEntryCount() const { return m_entries.size(); }

private:
    struct Entry
    {
        Hotkey binding;
        QString commandId;
    };

    void bake();
    void reconcile();
    bool dispatch(QKeyEvent *e);
    void restoreAction(QAction *action);

    CommandRegistry *m_registry = nullptr;
    HotkeyManager m_hotkeys;
    std::unique_ptr<Scope> m_scope;
    KeyBinding m_catchAll;
    QList<Entry> m_entries;
    QStringList m_unresolved;
    QHash<QString, QAction *> m_actions;
    QHash<QAction *, QKeySequence> m_originals;
    bool m_installed = false;
};

} // namespace Corbomite
