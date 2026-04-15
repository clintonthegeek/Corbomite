// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>
#include <QVector>

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
//   "title", "open", "action-primary", "action", "info", "info.copy",
//   "view", "system", "" (unset), "danger"
//
// Unknown section IDs go into the "" (unset) bucket per Obsidian's behaviour.
class MenuSectionHelper {
public:
    explicit MenuSectionHelper(QMenu *menu);

    // Append `action` to the named section. Order within a section is the
    // order of addToSection calls.
    void addToSection(QAction *action, const QString &sectionId);

    // Flush all collected actions into the wrapped QMenu, in canonical
    // section order, separated by QMenu::addSeparator() between sections.
    // Idempotent — calling twice does not double the actions; subsequent
    // calls re-flush against the *current* bucket state.
    void finalize();

    // Canonical ordering of section IDs. Exposed for tests + the audit.
    static const QStringList &canonicalSectionOrder();

private:
    QMenu *m_menu;
    QHash<QString, QVector<QAction *>> m_buckets;
};

} // namespace Corbomite
