// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QIcon>
#include <QList>
#include <QString>
#include <QVector>

#include <memory>

class QAction;
class QMenu;

namespace Corbomite {

// Wraps a QMenu with Obsidian's section-ordering protocol
// (`docs/obsidian-audit/domains/ui-bundle.md §5`).
//
// Plugins (and core code) push QActions into named sections; on finalize() the
// helper flushes them into the wrapped QMenu in the canonical order with a
// QMenu::addSeparator() between each non-empty section. Within a section,
// insertion order is preserved.
//
// Canonical section IDs (in render order):
//   "close", "pane", "open", "action", "find", "info", "info.copy",
//   "view", "view.linked", "system", "" (unset), "danger"
//
// Unknown section IDs go into the "" (unset) bucket per Obsidian's behaviour.
class MenuSectionHelper {
public:
    explicit MenuSectionHelper(QMenu *menu);

    // Append `action` to the named section. Order within a section is the
    // order of addToSection calls.
    void addToSection(QAction *action, const QString &sectionId);

    // Returns a nested helper (non-owning pointer; outer helper owns the
    // nested helper through m_pendingSubmenus) whose contents flush as a
    // submenu QAction at the outer helper's finalize().
    MenuSectionHelper *addSubmenu(const QString &sectionId,
                                   const QString &title,
                                   const QIcon &icon = QIcon());

    // Flush all collected actions into the wrapped QMenu, in canonical
    // section order, separated by QMenu::addSeparator() between sections.
    // Idempotent — calling twice does not double the actions; subsequent
    // calls re-flush against the *current* bucket state.
    void finalize();

    // Canonical ordering of section IDs. Exposed for tests + the audit.
    static const QStringList &canonicalSectionOrder();

private:
    struct PendingSubmenu {
        QString sectionId;
        QString title;
        QIcon icon;
        std::shared_ptr<QMenu> menu;
        std::shared_ptr<MenuSectionHelper> nestedHelper;
    };

    QMenu *m_menu;
    QHash<QString, QVector<QAction *>> m_buckets;
    QList<PendingSubmenu> m_pendingSubmenus;
};

} // namespace Corbomite
