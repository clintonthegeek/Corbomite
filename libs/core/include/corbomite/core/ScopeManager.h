// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QVector>

#include <functional>

class QKeyEvent;
class QWidget;

namespace Corbomite {

class Scope;

/// Singleton that walks an LIFO stack of active `Scope`s for every
/// `QKeyEvent` seen by the application. Designed to be installed as an
/// event filter on `QApplication` at startup (see `install()`), so that
/// `Scope` dispatch happens *before* widget-local `QShortcut` matching
/// and `KActionCollection`. That ordering is why modal scopes can mask
/// global shortcuts without disconnecting them.
///
/// Shape, and why: the stack is **flat** — a scope pushed on top never stops
/// the scopes below it from being consulted — plus one explicit **root slot**
/// that is always walked last. Obsidian gets the same "modal first, command
/// table last" ordering from a per-window scope stack plus `Scope.parent`
/// (`0747-ly.js:44-53`, `:27`), and pays for it with a bug: a parentless
/// `Modal` scope makes the root — the only place `HotkeyManager` registers its
/// catch-all (`0833-$w.js:7`) — unreachable, so **no** `hotkeys.json` binding
/// fires while any modal is up (addendum §E). Flat reproduces the ordering
/// without the bug, and the root slot replaces Obsidian's accident of *where*
/// the table happens to live with a stated position.
///
/// What the flat walk must not do is let the command table into a window that
/// does not own it, so the root slot carries its own gate (`setRootGate`),
/// defaulting to "no modal widget owns input": a `QDialog` runs in its own
/// shortcut context (addendum §E) and its key presses must not fire the main
/// window's commands.
///
/// Focus-gating: if the currently-focused widget is a text-input
/// (QLineEdit/QTextEdit/QPlainTextEdit subclasses, or any widget for
/// which `shouldBypassFor(w)` returns true), key presses bypass the *pushed*
/// scopes so the user can still type. The root slot is not focus-gated:
/// Obsidian's catch-all has no focus condition at all, and gating it would
/// kill every global shortcut the moment a search box, quick switcher or file
/// filter took focus.
///
/// Event handling:
///   - `QEvent::KeyPress` — walk pushed scopes top→bottom, then the root
///     slot; first hit (callback returned true) consumes the event.
///   - `QEvent::ShortcutOverride` — return false unconditionally.
///     Letting this pass keeps Qt's IME composition + dead-key flow
///     intact; dispatch happens on the subsequent `KeyPress`.
///
/// The stack is plain QObject; install/push/pop are not thread-safe
/// (Qt event dispatch is single-threaded by contract).
class ScopeManager : public QObject
{
    Q_OBJECT
public:
    /// Accessor for the process-wide singleton. Created lazily.
    static ScopeManager *instance();

    /// Install this singleton as a `QApplication` event filter.
    /// Safe to call more than once (no-op on subsequent calls).
    void installOnApplication();

    /// Push a scope onto the active stack (LIFO).
    void pushScope(Scope *scope);

    /// Remove a scope from the stack. If it's the top, equivalent
    /// to `popScope()`; otherwise removes from anywhere in the stack.
    void removeScope(Scope *scope);

    /// Pop the top scope. No-op on empty stack.
    void popScope();

    int stackDepth() const { return m_stack.size(); }

    /// The single bottom-most scope, or null. Set by whoever owns the
    /// process-wide command table (`HotkeyDispatcher`); consulted after
    /// every pushed scope and subject to the root gate.
    void setRootScope(Scope *scope);
    Scope *rootScope() const { return m_root; }

    /// Predicate deciding whether the root slot may be consulted at all;
    /// truthy permits it. Default: no modal widget owns input. Falsy restore
    /// removes it.
    using RootGate = std::function<bool()>;
    void setRootGate(RootGate gate);

    /// Directly dispatch a key event through the stack, honouring
    /// focus-gating rules. Public for testing. Pass `focusOverride`
    /// to bypass the `qApp->focusWidget()` lookup (useful in tests
    /// where offscreen Qt doesn't propagate focus reliably).
    bool dispatchKey(QKeyEvent *evt, QWidget *focusOverride = nullptr);

    /// Override the predicate used to decide whether a focused widget
    /// should bypass scope (e.g. text-edits). Default inspects the
    /// widget's class for QLineEdit / QAbstractScrollArea-with-text.
    /// Tests install their own predicate.
    using BypassPredicate = std::function<bool(QWidget *)>;
    void setBypassPredicate(BypassPredicate p);

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;

private:
    ScopeManager();
    QVector<QPointer<QObject>> m_lifetime; // holds dummy markers
    QVector<Scope *> m_stack;
    Scope *m_root = nullptr;
    RootGate m_rootGate;
    bool m_installed = false;
    BypassPredicate m_bypass;

    static bool defaultBypass(QWidget *w);
    static bool defaultRootGate();
};

} // namespace Corbomite
