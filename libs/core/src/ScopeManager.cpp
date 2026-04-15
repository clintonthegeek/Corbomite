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

bool ScopeManager::defaultBypass(QWidget *w)
{
    if (!w) return false;
    if (qobject_cast<QLineEdit *>(w)) return true;
    if (qobject_cast<QTextEdit *>(w)) return true;
    if (qobject_cast<QPlainTextEdit *>(w)) return true;
    return false;
}

bool ScopeManager::dispatchKey(QKeyEvent *evt, QWidget *focusOverride)
{
    if (!evt) return false;
    if (m_stack.isEmpty()) return false;

    QWidget *focused = focusOverride ? focusOverride
                                     : (qApp ? qApp->focusWidget() : nullptr);
    if (focused && m_bypass && m_bypass(focused)) return false;

    // Walk top → bottom (LIFO). First consumed wins.
    for (int i = m_stack.size() - 1; i >= 0; --i) {
        Scope *s = m_stack.at(i);
        if (s && s->handleKey(evt)) return true;
    }
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
