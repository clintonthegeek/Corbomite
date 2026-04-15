// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MenuSectionHelper.h"

#include <QAction>
#include <QMenu>

namespace Corbomite {

const QStringList &MenuSectionHelper::canonicalSectionOrder()
{
    static const QStringList order = {
        QStringLiteral("title"),
        QStringLiteral("open"),
        QStringLiteral("action-primary"),
        QStringLiteral("action"),
        QStringLiteral("info"),
        QStringLiteral("info.copy"),
        QStringLiteral("view"),
        QStringLiteral("system"),
        QStringLiteral(""),         // unset bucket — unknown sections funnel here
        QStringLiteral("danger"),
    };
    return order;
}

MenuSectionHelper::MenuSectionHelper(QMenu *menu)
    : m_menu(menu)
{
}

void MenuSectionHelper::addToSection(QAction *action, const QString &sectionId)
{
    if (!action) return;
    const QStringList &known = canonicalSectionOrder();
    const QString bucketKey = known.contains(sectionId) ? sectionId : QString();
    m_buckets[bucketKey].append(action);
}

void MenuSectionHelper::finalize()
{
    if (!m_menu) return;
    m_menu->clear();
    bool needSeparator = false;
    for (const QString &section : canonicalSectionOrder()) {
        const auto &actions = m_buckets.value(section);
        if (actions.isEmpty()) continue;
        if (needSeparator) m_menu->addSeparator();
        for (QAction *a : actions) m_menu->addAction(a);
        needSeparator = true;
    }
}

} // namespace Corbomite
