// SPDX-License-Identifier: GPL-3.0-or-later
#include "RibbonToolBar.h"

#include <QAction>
#include <QMainWindow>

namespace Corbomite {

RibbonToolBar::RibbonToolBar(QWidget *parent)
    : KToolBar(parent)
{
    commonInit();
}

RibbonToolBar::RibbonToolBar(const QString &objectName, QMainWindow *parent)
    : KToolBar(objectName, parent)
{
    commonInit();
}

void RibbonToolBar::commonInit()
{
    setToolButtonStyle(Qt::ToolButtonIconOnly);
}

RibbonToolBar::Handle RibbonToolBar::addRibbonIcon(const Handle &id,
                                                   const QIcon &icon,
                                                   const QString &title,
                                                   std::function<void()> onActivated)
{
    if (id.isEmpty()) return {};
    if (m_actions.contains(id)) return {};

    auto *action = new QAction(icon, title, this);
    action->setToolTip(title);
    if (onActivated) {
        QObject::connect(action, &QAction::triggered,
                         this, std::move(onActivated));
    }
    addAction(action);
    m_actions.insert(id, action);
    m_order.append(id);

    Q_EMIT iconAdded(id);
    return id;
}

bool RibbonToolBar::removeRibbonIcon(const Handle &id)
{
    auto it = m_actions.find(id);
    if (it == m_actions.end()) return false;
    QAction *action = it.value();
    removeAction(action);
    action->deleteLater();
    m_actions.erase(it);
    m_order.removeAll(id);

    Q_EMIT iconRemoved(id);
    return true;
}

int RibbonToolBar::iconCount() const { return m_actions.size(); }

bool RibbonToolBar::hasIcon(const Handle &id) const
{
    return m_actions.contains(id);
}

QStringList RibbonToolBar::iconIdsInOrder() const { return m_order; }

void RibbonToolBar::setIconVisible(const Handle &id, bool visible)
{
    auto it = m_actions.find(id);
    if (it == m_actions.end()) return;
    if (it.value()->isVisible() == visible) return;
    it.value()->setVisible(visible);
    Q_EMIT iconVisibilityChanged(id, visible);
}

bool RibbonToolBar::isIconVisible(const Handle &id) const
{
    auto it = m_actions.find(id);
    if (it == m_actions.end()) return false;
    return it.value()->isVisible();
}

QAction *RibbonToolBar::actionForId(const Handle &id) const
{
    return m_actions.value(id, nullptr);
}

} // namespace Corbomite
