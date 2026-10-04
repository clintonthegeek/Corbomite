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

    // Deliberate divergence from Obsidian's `Keymap.pushScope`
    // (app/own/0747-ly.js:63-66), recorded so nobody "fixes" it by accident.
    // Obsidian keeps ONE scope per window (`getWindowStack` seeds
    // `{scope: rootScope, prevScopes: []}`, :48-53) and pushing replaces it,
    // so a parentless scope ends the walk at Scope.handleKey's
    // `if (this.parent) return this.parent.handleKey(...)`
    // (app/own/0747-ly.js:27) and the root scope — the only place
    // `HotkeyManager` registers its catch-all (0833-$w.js:7) — goes
    // unreachable for as long as the modal is up. That is why `Modal`
    // allocates `new Scope()` with no parent (0817-Yb.js:630) and
    // `SuggestModal` deliberately replaces it with a parented one
    // (1319-xR.js:29).
    //
    // This stack is flat: a parentless scope pushed on top does not stop the
    // scopes below it from being consulted. Two reasons that is fine today:
    // the KAction half of Obsidian's behaviour is already delivered by Qt
    // modality (a modal QDialog blocks the main window's window-scoped
    // shortcuts without any help from this stack), and no production code
    // pushes a Scope at all, so there is no root-scope catch-all to cut off.
    // Whoever adds the `hotkeys.json` dispatcher has to decide which of the two
    // shapes it wants — see the report in
    // docs/obsidian-audit/addenda/2026-10-03-core-command-and-hotkey-contracts.md §E.
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
