// SPDX-License-Identifier: GPL-3.0-or-later
#include "RibbonSlot.h"

#include <KToolBar>
#include <QAction>
#include <QVBoxLayout>

namespace Corbomite {

RibbonSlot::RibbonSlot(QWidget *parent)
    : QWidget(parent)
    , m_toolbar(new KToolBar(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_toolbar);

    m_toolbar->setOrientation(Qt::Vertical);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
}

RibbonSlot::Handle RibbonSlot::addRibbonIcon(const QIcon &icon,
                                              const QString &title,
                                              std::function<void()> onActivated)
{
    if (title.isEmpty()) return {};
    if (m_actions.contains(title)) return {};   // first-wins per Obsidian quirk
    auto *action = new QAction(icon, title, m_toolbar);
    action->setToolTip(title);
    if (onActivated) {
        QObject::connect(action, &QAction::triggered, m_toolbar, std::move(onActivated));
    }
    m_toolbar->addAction(action);
    m_actions.insert(title, action);
    return title;
}

bool RibbonSlot::removeRibbonIcon(const Handle &handle)
{
    auto it = m_actions.find(handle);
    if (it == m_actions.end()) return false;
    QAction *action = it.value();
    m_toolbar->removeAction(action);
    action->deleteLater();
    m_actions.erase(it);
    return true;
}

int RibbonSlot::iconCount() const
{
    return m_actions.size();
}

bool RibbonSlot::hasIcon(const Handle &handle) const
{
    return m_actions.contains(handle);
}

} // namespace Corbomite
