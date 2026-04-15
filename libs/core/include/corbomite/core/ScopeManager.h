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
/// Focus-gating: if the currently-focused widget is a text-input
/// (QLineEdit/QTextEdit/QPlainTextEdit subclasses, or any widget for
/// which `shouldBypassFor(w)` returns true), key presses bypass the
/// scope stack so the user can still type.
///
/// Event handling:
///   - `QEvent::KeyPress` — walk scope stack top→bottom; first hit
///     (callback returned true) consumes the event.
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
    bool m_installed = false;
    BypassPredicate m_bypass;

    static bool defaultBypass(QWidget *w);
};

} // namespace Corbomite
