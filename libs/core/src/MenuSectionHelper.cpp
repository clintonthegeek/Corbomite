// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/MenuSectionHelper.h"

#include <QAction>
#include <QMenu>

namespace Corbomite {

const QStringList &MenuSectionHelper::canonicalSectionOrder()
{
    static const QStringList order = {
        QStringLiteral("close"),
        QStringLiteral("pane"),
        QStringLiteral("open"),
        QStringLiteral("action"),
        QStringLiteral("find"),
        QStringLiteral("info"),
        QStringLiteral("info.copy"),
        QStringLiteral("view"),
        QStringLiteral("view.linked"),
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

MenuSectionHelper *MenuSectionHelper::addSubmenu(const QString &sectionId,
                                                  const QString &title,
                                                  const QIcon &icon)
{
    auto menu = std::make_shared<QMenu>(title);
    if (!icon.isNull()) menu->setIcon(icon);
    auto nested = std::make_shared<MenuSectionHelper>(menu.get());
    m_pendingSubmenus.append({sectionId, title, icon, menu, nested});
    return nested.get();
}

void MenuSectionHelper::finalize()
{
    if (!m_menu) return;
    m_menu->clear();

    // Snapshot the buckets before injecting submenu actions so successive
    // finalize() calls remain idempotent (we mutate a local copy, not
    // m_buckets — otherwise re-finalize would double-insert submenu entries).
    auto buckets = m_buckets;

    // First, finalize all submenus into their own QMenu objects, then
    // inject a QAction-with-menu into the outer bucket.
    const QStringList &known = canonicalSectionOrder();
    for (auto &ps : m_pendingSubmenus) {
        ps.nestedHelper->finalize();
        QAction *submenuAction = ps.menu->menuAction();
        submenuAction->setText(ps.title);
        if (!ps.icon.isNull()) submenuAction->setIcon(ps.icon);
        const QString bucketKey = known.contains(ps.sectionId)
            ? ps.sectionId : QString();
        buckets[bucketKey].append(submenuAction);
    }

    // Then flush top-level buckets in canonical order, separated by
    // QMenu::addSeparator() between non-empty sections.
    bool needSeparator = false;
    for (const QString &section : known) {
        const auto &actions = buckets.value(section);
        if (actions.isEmpty()) continue;
        if (needSeparator) m_menu->addSeparator();
        for (QAction *a : actions) m_menu->addAction(a);
        needSeparator = true;
    }
}

} // namespace Corbomite
