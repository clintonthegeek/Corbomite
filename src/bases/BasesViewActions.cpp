// src/bases/BasesViewActions.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "BasesViewActions.h"

#include "corbomite/core/View.h"
#include "corbomite/bases/BasesView.h"

#include <KActionCollection>
#include <KLocalizedString>

#include <QAction>
#include <QCursor>
#include <QIcon>
#include <QMenu>

namespace Corbomite {

namespace {
const QString kBasesGuiXml = QStringLiteral(
    "<!DOCTYPE gui SYSTEM \"kpartgui.dtd\">"
    "<gui name=\"bases_view_actions\" version=\"1\">"
    "  <MenuBar>"
    "    <Menu name=\"bases\">"
    "      <text>&amp;Bases</text>"
    "      <Action name=\"bases_properties\"/>"
    "      <Action name=\"bases_sort_group\"/>"
    "      <Action name=\"bases_views\"/>"
    "      <Separator/>"
    "      <Action name=\"bases_filters\"/>"
    "      <Action name=\"bases_properties_pane\"/>"
    "      <Separator/>"
    "      <Action name=\"bases_new_entry\"/>"
    "      <Action name=\"bases_results\"/>"
    "    </Menu>"
    "  </MenuBar>"
    "</gui>");
} // namespace

BasesViewActions::BasesViewActions(QObject *parent)
    : ViewActions(parent)
{
    setComponentName(QStringLiteral("corbomite_bases_actions"), i18n("Bases"));
    setupActions();
    setXML(kBasesGuiXml, false /*merge*/);
    actionCollection()->setConfigGroup(QStringLiteral("Shortcuts"));
    refresh();
}

QString BasesViewActions::viewType() const
{
    return QStringLiteral("bases");
}

void BasesViewActions::bind(View *view)
{
    disconnect(m_contextConnection);
    m_boundView = qobject_cast<Bases::BasesView *>(view);
    if (m_boundView) {
        m_contextConnection =
            connect(m_boundView, &View::contextChanged, this, &BasesViewActions::refresh);
        adoptActions();
    }
    refresh();
}

void BasesViewActions::unbind()
{
    disconnect(m_contextConnection);
    // Detach before dropping the pointer: the buttons go back to standalone
    // behaviour (and Results regains its local menu), so a view that outlives
    // the provider — or a re-bind to a different bases view — is never left
    // pointing at actions this object is about to destroy.
    if (m_boundView) releaseActions();
    m_boundView = nullptr;
    refresh();
}

void BasesViewActions::adoptActions()
{
    if (!m_boundView) return;
    auto *ac = actionCollection();
    using Slot = Bases::BasesView::ToolbarSlot;
    static const std::pair<Slot, const char *> kSlots[] = {
        {Slot::Properties, "bases_properties"},
        {Slot::SortGroup, "bases_sort_group"},
        {Slot::Views, "bases_views"},
        {Slot::Filters, "bases_filters"},
        {Slot::PropertiesPane, "bases_properties_pane"},
        {Slot::NewEntry, "bases_new_entry"},
        {Slot::Results, "bases_results"},
    };
    for (const auto &[slot, id] : kSlots) {
        if (auto *a = ac->action(QString::fromLatin1(id)))
            m_boundView->setToolbarAction(slot, a);
    }

    // Hand the shared results menu to the action so the menubar/palette
    // entry and the in-view button are the same menu object (see the
    // Results comment in setupActions). Done per-bind rather than once in
    // the ctor because the menu belongs to the view, not to this provider.
    if (auto *a = ac->action(QStringLiteral("bases_results")))
        a->setMenu(m_boundView->toolbarResultsMenu());
}

void BasesViewActions::releaseActions()
{
    if (!m_boundView) return;
    using Slot = Bases::BasesView::ToolbarSlot;
    for (Slot slot : {Slot::Properties, Slot::SortGroup, Slot::Views, Slot::Filters,
                      Slot::PropertiesPane, Slot::NewEntry, Slot::Results})
        m_boundView->setToolbarAction(slot, nullptr);

    // Drop our reference to the (now possibly destroyed) view's menu.
    if (auto *ac = actionCollection())
        if (auto *a = ac->action(QStringLiteral("bases_results")))
            a->setMenu(nullptr);
}

void BasesViewActions::refresh()
{
    auto *ac = actionCollection();
    auto *view = m_boundView;

    // Tier B — the predicates live on BasesView itself, so the
    // enablement rule has exactly one definition rather than a copy here.
    const bool bound = view != nullptr;
    const bool hasView = bound && view->hasActiveView();
    const bool hasQuery = bound && view->hasQuery();

    for (const auto &id : {QStringLiteral("bases_properties"),
                            QStringLiteral("bases_sort_group")})
        if (auto *a = ac->action(id)) a->setEnabled(hasView);

    // Views edits the query's named-view list, so it needs the query but not
    // a selected view — it can be used to *create* the first view.
    if (auto *a = ac->action(QStringLiteral("bases_views")))
        a->setEnabled(hasQuery);

    if (auto *a = ac->action(QStringLiteral("bases_filters")))
        a->setEnabled(hasQuery);

    // +New — the RE resolves an unset newItemFolder to a default, and
    // FileManager::createMarkdownNote auto-creates a missing one, so only a
    // missing/vaultless FileManager actually blocks creation.
    if (auto *a = ac->action(QStringLiteral("bases_new_entry")))
        a->setEnabled(bound && view->canCreateNewEntry());

    // Tier C — the drawer's check state is read from the real widget rather
    // than tracked separately, so the two can never disagree.
    if (auto *a = ac->action(QStringLiteral("bases_properties_pane"))) {
        a->setEnabled(hasQuery);
        a->setChecked(bound && view->propertiesPaneVisible());
    }

    if (auto *a = ac->action(QStringLiteral("bases_results")))
        a->setEnabled(hasView);
}

QList<QAction *> BasesViewActions::toolBarActions() const
{
    QList<QAction *> actions;
    auto *ac = actionCollection();
    if (auto *a = ac->action(QStringLiteral("bases_properties_pane"))) actions << a;
    if (auto *a = ac->action(QStringLiteral("bases_new_entry"))) actions << a;
    if (auto *a = ac->action(QStringLiteral("bases_results"))) actions << a;
    return actions;
}

void BasesViewActions::setupActions()
{
    auto *ac = actionCollection();

    // The three popup-panel buttons. No shortcuts: Obsidian assigns these
    // to its command palette rather than chords, and inventing chords for
    // them would collide with the universal Ctrl+F that already focuses the
    // bases search box.
    auto addPanelVerb = [this, ac](const QString &id, const QString &label,
                                   void (Bases::BasesView::*verb)()) -> QAction * {
        auto *act = ac->addAction(id);
        act->setText(label);
        connect(act, &QAction::triggered, this, [this, verb]() {
            if (m_boundView) (m_boundView->*verb)();
        });
        return act;
    };
    addPanelVerb(QStringLiteral("bases_properties"), i18n("Properties"),
                 &Bases::BasesView::showPropertiesPanel);
    addPanelVerb(QStringLiteral("bases_sort_group"), i18n("Sort & group"),
                 &Bases::BasesView::showSortGroupPanel);
    addPanelVerb(QStringLiteral("bases_views"), i18n("Views"),
                 &Bases::BasesView::showViewsPanel);

    auto *filters = ac->addAction(QStringLiteral("bases_filters"));
    filters->setText(i18n("Filters"));
    connect(filters, &QAction::triggered, this, [this]() {
        if (m_boundView) m_boundView->openFiltersDialog();
    });

    // Checkable drawer toggle. setChecked() is sync'd into the button by
    // BasesView::setPropertiesPaneVisible, so the two never drift.
    auto *pane = ac->addAction(QStringLiteral("bases_properties_pane"));
    pane->setText(i18n("Properties pane"));
    pane->setCheckable(true);
    connect(pane, &QAction::triggered, this, [this](bool on) {
        if (m_boundView) m_boundView->setPropertiesPaneVisible(on);
    });

    auto *newEntry = ac->addAction(QStringLiteral("bases_new_entry"));
    newEntry->setText(i18n("New entry"));
    newEntry->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    connect(newEntry, &QAction::triggered, this, [this]() {
        if (m_boundView) m_boundView->onNewItem();
    });

    // Container action owning the results menu, mirroring the RE's
    // `Results menu ($X)`. The menu itself is NOT built here — BasesView
    // owns it and its two children, and this adopts that same menu, so the
    // in-view button and this menubar/palette entry are literally the same
    // menu rather than two copies that can drift. Its two children stay
    // individually triggerable actions, so the palette can reach Copy-table
    // directly.
    auto *results = ac->addAction(QStringLiteral("bases_results"));
    results->setText(i18n("Results"));
    results->setIcon(QIcon::fromTheme(QStringLiteral("document-export")));
    connect(results, &QAction::triggered, this, [this]() {
        // The menubar entry has no button to anchor a popup to, so it opens
        // the shared menu at the cursor — the standard treatment for a
        // menu-only action.
        if (auto *menu = m_boundView ? m_boundView->toolbarResultsMenu() : nullptr) {
            if (menu->isVisible()) {
                menu->hide();
            } else {
                menu->popup(QCursor::pos());
            }
        }
    });
}

} // namespace Corbomite