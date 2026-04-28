// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/StatusBarRegistry.h"

#include <QStatusBar>
#include <QWidget>

namespace Corbomite {

StatusBarRegistry::StatusBarRegistry(QStatusBar *bar, QObject *parent)
    : QObject(parent), m_bar(bar)
{}

bool StatusBarRegistry::addItem(const QString &id, QWidget *widget)
{
    if (!m_bar || !widget || id.isEmpty()) return false;
    if (m_items.contains(id)) return false;
    m_bar->addPermanentWidget(widget);
    m_items.insert(id, QPointer<QWidget>(widget));
    return true;
}

bool StatusBarRegistry::removeItem(const QString &id)
{
    auto it = m_items.find(id);
    if (it == m_items.end()) return false;
    if (auto *w = it.value().data()) {
        if (m_bar) m_bar->removeWidget(w);
        w->deleteLater();
    }
    m_items.erase(it);
    return true;
}

bool StatusBarRegistry::hasItem(const QString &id) const
{
    return m_items.contains(id);
}

} // namespace Corbomite
