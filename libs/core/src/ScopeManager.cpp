// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/ScopeManager.h"
#include "corbomite/core/Scope.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QWidget>

namespace Corbomite {

ScopeManager::ScopeManager()
    : QObject(nullptr), m_bypass(&ScopeManager::defaultBypass)
    , m_rootGate(&ScopeManager::defaultRootGate)
{}

ScopeManager *ScopeManager::instance()
{
    static ScopeManager *s_instance = new ScopeManager();
    return s_instance;
}

void ScopeManager::installOnApplication()
{
    if (m_installed) return;
    if (auto *app = qApp) {
        app->installEventFilter(this);
        // Reparent so we're cleaned up with the app.
        setParent(app);
        m_installed = true;
    }
}

void ScopeManager::pushScope(Scope *scope)
{
    if (!scope) return;
    m_stack.append(scope);
}

void ScopeManager::removeScope(Scope *scope)
{
    m_stack.removeAll(scope);
}

void ScopeManager::popScope()
{
    if (!m_stack.isEmpty()) m_stack.removeLast();
}

void ScopeManager::setBypassPredicate(BypassPredicate p)
{
    m_bypass = p ? std::move(p) : &ScopeManager::defaultBypass;
}

void ScopeManager::setRootScope(Scope *scope)
{
    m_root = scope;
}

void ScopeManager::setRootGate(RootGate gate)
{
    m_rootGate = gate ? std::move(gate) : &ScopeManager::defaultRootGate;
}

bool ScopeManager::defaultBypass(QWidget *w)
{
    if (!w) return false;
    if (qobject_cast<QLineEdit *>(w)) return true;
    if (qobject_cast<QTextEdit *>(w)) return true;
    if (qobject_cast<QPlainTextEdit *>(w)) return true;
    return false;
}

bool ScopeManager::defaultRootGate()
{
    // A modal widget owns input: its key presses must not fire the main
    // window's commands (addendum §E — a modal runs in its own shortcut
    // context).
    return !(qApp && qApp->activeModalWidget());
}

bool ScopeManager::dispatchKey(QKeyEvent *evt, QWidget *focusOverride)
{
    if (!evt) return false;
    if (m_stack.isEmpty() && !m_root) return false;

    QWidget *focused = focusOverride ? focusOverride
                                     : (qApp ? qApp->focusWidget() : nullptr);
    // Focus-gating covers the pushed scopes only — see the class comment. A
    // text input must not swallow keys a modal scope claims, but it must not
    // silence the process-wide command table either.
    const bool bypass = !m_stack.isEmpty() && focused && m_bypass
                        && m_bypass(focused);
    if (!bypass) {
        for (int i = m_stack.size() - 1; i >= 0; --i) {
            Scope *s = m_stack.at(i);
            if (s && s->handleKey(evt)) return true;
        }
    }
    // Root last, so a modal or suggester scope masks the command table and a
    // scope that declines a key falls through to it. Obsidian produces the
    // same ordering; what it does not do is keep the table reachable from a
    // parentless scope (addendum §E), which is the behaviour this walk drops.
    if (m_root && (!m_rootGate || m_rootGate())) return m_root->handleKey(evt);
    return false;
}

bool ScopeManager::eventFilter(QObject *obj, QEvent *e)
{
    Q_UNUSED(obj);
    if (e->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(e);
        if (dispatchKey(ke)) {
            e->accept();
            return true;
        }
    }
    // ShortcutOverride and others: let Qt continue its normal flow
    // (IME composition, KActionCollection, widget keyPressEvent).
    return false;
}

} // namespace Corbomite
