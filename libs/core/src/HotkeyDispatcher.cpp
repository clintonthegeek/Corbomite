// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/HotkeyDispatcher.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/ScopeManager.h"

#include <QAction>
#include <QDebug>
#include <QKeyEvent>

namespace Corbomite {

namespace {

/// Index of the first binding Qt can spell as a `QKeySequence`, or -1. That
/// binding goes onto the `QAction`; everything else is matched at event level.
int firstExpressibleBinding(const QList<Hotkey> &bindings)
{
    for (int i = 0; i < bindings.size(); ++i) {
        const QKeySequence seq = hotkeyToKeySequence(bindings.at(i));
        if (seq.count() == 1 && seq[0].key() != Qt::Key_unknown) return i;
    }
    return -1;
}

} // namespace

HotkeyDispatcher::HotkeyDispatcher(CommandRegistry *registry, QObject *parent)
    : QObject(parent), m_registry(registry)
{}

HotkeyDispatcher::~HotkeyDispatcher()
{
    uninstall();
    clearActions();
}

bool HotkeyDispatcher::load(const QByteArray &json)
{
    if (!m_hotkeys.load(json)) return false;
    bake();
    reconcile();
    return true;
}

void HotkeyDispatcher::bindAction(const QString &commandId, QAction *action)
{
    if (commandId.isEmpty() || !action) return;
    m_actions.insert(commandId, action);
    reconcile();
}

void HotkeyDispatcher::install()
{
    if (m_installed) return;
    if (!m_scope) m_scope = std::make_unique<Scope>();
    m_catchAll = m_scope->registerCatchAll([this](QKeyEvent *e) { return dispatch(e); });
    ScopeManager::instance()->setRootScope(m_scope.get());
    m_installed = true;
}

void HotkeyDispatcher::uninstall()
{
    if (!m_installed) return;
    // Only release the slot if it is still ours: two live dispatchers (two
    // MainWindows in one test process) must not have the younger one
    // unregistered by the older one's destruction.
    if (ScopeManager::instance()->rootScope() == m_scope.get())
        ScopeManager::instance()->setRootScope(nullptr);
    m_scope->unregister(m_catchAll);
    m_catchAll = KeyBinding();
    m_installed = false;
}

void HotkeyDispatcher::clearActions()
{
    const auto actions = m_actions.values();
    m_actions.clear();
    for (auto *act : actions) restoreAction(act);
}

void HotkeyDispatcher::bake()
{
    m_entries.clear();
    m_unresolved.clear();

    for (const auto &id : m_hotkeys.overriddenIds()) {
        const auto bindings = m_hotkeys.effectiveHotkeys(id);
        // No bound action means nothing can carry a shortcut text, so every
        // binding is matched here — otherwise an expressible binding for a
        // command with no QAction behind it would be dropped by both paths.
        const int actionIndex =
            m_actions.contains(id) ? firstExpressibleBinding(bindings) : -1;

        for (int i = 0; i < bindings.size(); ++i) {
            if (i == actionIndex) continue;
            const Hotkey &b = bindings.at(i);

            if (!b.code.isEmpty() && hotkeyCodeToQtKey(b.code) == 0) {
                // `code` is the whole point of the field: matching the
                // character instead would fire on the wrong key, and the user
                // would have no way to tell why their shortcut is dead.
                m_unresolved.append(QStringLiteral("%1: key code '%2' is not a key "
                                                   "Corbomite can match")
                                         .arg(id, b.code));
                continue;
            }
            if (b.code.isEmpty() && hotkeyToKeySequence(b).isEmpty()) {
                m_unresolved.append(
                    QStringLiteral("%1: key '%2' is not a key Qt can name (matched "
                                   "by character only)")
                        .arg(id, b.resolvedKey()));
            }
            m_entries.append(Entry{b, id});
        }
    }

    for (const auto &line : m_unresolved) {
        qWarning().noquote() << "hotkeys.json:" << line;
    }
}

void HotkeyDispatcher::reconcile()
{
    for (auto it = m_actions.begin(); it != m_actions.end(); ++it) {
        QAction *act = it.value();
        if (!act) continue;
        // Absent id: the action keeps whatever shortcut it had — its own KDE
        // default, or one the user set in Settings. Only a *present* id has an
        // opinion here.
        if (!m_hotkeys.customHotkeys(it.key())) {
            restoreAction(act);
            continue;
        }
        const auto bindings = m_hotkeys.effectiveHotkeys(it.key());
        const int actionIndex = firstExpressibleBinding(bindings);
        if (actionIndex < 0) {
            // `[]` — explicitly unbound, and the default is not restored
            // (0833-$w.js:73-74). Two other cases land here too and want the
            // same answer: a code-only binding (no QKeySequence spelling, and
            // the event-level matcher owns it, so leaving the action's stock
            // chord in place would make the override *additive* rather than
            // replacing), and an id whose every binding is one Qt cannot name
            // (still matched by character, but the user did speak, so the stock
            // chord must not stay live beside it).
            if (!m_originals.contains(act)) m_originals.insert(act, act->shortcut());
            act->setShortcut(QKeySequence());
            continue;
        }
        if (!m_originals.contains(act)) m_originals.insert(act, act->shortcut());
        act->setShortcut(hotkeyToKeySequence(bindings.at(actionIndex)));
    }
}

void HotkeyDispatcher::restoreAction(QAction *action)
{
    if (!action) return;
    const auto it = m_originals.constFind(action);
    if (it == m_originals.constEnd()) return;
    action->setShortcut(it.value());
    m_originals.remove(action);
}

bool HotkeyDispatcher::dispatch(QKeyEvent *e)
{
    for (const auto &entry : m_entries) {
        if (!hotkeyMatches(entry.binding, *e)) continue;
        // `onTrigger` consumes the key only when the command's own dispatch
        // succeeded (0833-$w.js:84-85), so an unavailable command leaves the
        // key to the rest of the chain instead of swallowing it.
        if (m_registry && m_registry->executeById(entry.commandId)) return true;
    }
    return false;
}

} // namespace Corbomite
