// src/plugins/graph-view/GraphViewActions.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphViewActions.h"

#include "GraphView.h"
#include "GraphViewTab.h"
#include "corbomite/core/View.h"

#include <KActionCollection>
#include <KLocalizedString>

#include <QAction>
#include <QIcon>
#include <QKeySequence>

namespace Corbomite {

namespace {
const QString kGraphGuiXml = QStringLiteral(
    "<!DOCTYPE gui SYSTEM \"kpartgui.dtd\">"
    "<gui name=\"graph_view_actions\" version=\"1\">"
    "  <MenuBar>"
    "    <Menu name=\"graph\">"
    "      <text>&amp;Graph</text>"
    "      <Action name=\"graph_open_local\"/>"
    "      <Separator/>"
    "      <Action name=\"graph_animate\"/>"
    "      <Action name=\"graph_reset_forces\"/>"
    "      <Action name=\"graph_zoom_to_fit\"/>"
    "      <Action name=\"graph_copy_screenshot\"/>"
    "    </Menu>"
    "  </MenuBar>"
    "</gui>");
} // namespace

GraphViewActions::GraphViewActions(CommandDispatch commandDispatch, QObject *parent)
    : ViewActions(parent)
    , m_commandDispatch(std::move(commandDispatch))
{
    setComponentName(QStringLiteral("corbomite_graph_actions"), i18n("Graph"));
    setupActions();
    setXML(kGraphGuiXml, false /*merge*/);
    actionCollection()->setConfigGroup(QStringLiteral("Shortcuts"));
    refresh();
}

QString GraphViewActions::viewType() const
{
    return QStringLiteral("graph");
}

void GraphViewActions::bind(View *view)
{
    disconnect(m_contextConnection);
    m_boundView = qobject_cast<GraphView *>(view);
    if (m_boundView) {
        m_contextConnection =
            connect(m_boundView, &View::contextChanged, this, &GraphViewActions::refresh);
    }
    refresh();
}

void GraphViewActions::unbind()
{
    disconnect(m_contextConnection);
    m_boundView = nullptr;
    refresh();
}

void GraphViewActions::refresh()
{
    auto *ac = actionCollection();
    auto *view = m_boundView;
    const bool bound = view != nullptr;
    auto *tab = bound ? view->graphWidget() : nullptr;

    // A graph with no nodes has nothing to fit, nothing to re-randomize, and
    // a screenshot worth taking — so those three wait for a built graph
    // rather than being live-but-useless.
    const bool hasGraph = tab && tab->isGraphBuilt();

    for (const auto &id : {QStringLiteral("graph_animate"),
                            QStringLiteral("graph_zoom_to_fit"),
                            QStringLiteral("graph_copy_screenshot")})
        if (auto *a = ac->action(id)) a->setEnabled(hasGraph);

    // Reset-forces is a settings verb on the controls panel, which exists as
    // soon as the tab does; it does not need nodes to have something to
    // restore.
    if (auto *a = ac->action(QStringLiteral("graph_reset_forces")))
        a->setEnabled(bound);

    // Opening the local graph needs a graph open at all — the host's command
    // replaces the active leaf's view, so there must be one to replace.
    if (auto *a = ac->action(QStringLiteral("graph_open_local")))
        a->setEnabled(bound);
}

QList<QAction *> GraphViewActions::toolBarActions() const
{
    QList<QAction *> actions;
    auto *ac = actionCollection();
    // Only zoom-to-fit on the toolbar: it is the one graph verb with an
    // obvious single-click meaning. The others are multi-step operations
    // that belong in the menu and the palette, not pinned to a toolbar.
    if (auto *a = ac->action(QStringLiteral("graph_zoom_to_fit"))) actions << a;
    return actions;
}

void GraphViewActions::setupActions()
{
    auto *ac = actionCollection();

    // Delegated to the host's CommandRegistry rather than reimplemented:
    // both of these already have exactly one implementation, and duplicating
    // them is how "the graph's screenshot looks different from the command
    // palette's screenshot" bugs happen.
    auto *openLocal = ac->addAction(QStringLiteral("graph_open_local"));
    openLocal->setText(i18n("Open Local Graph"));
    openLocal->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-network")));
    connect(openLocal, &QAction::triggered, this, [this]() {
        if (m_commandDispatch)
            m_commandDispatch(QStringLiteral("corbomite-local-graph:open-local"));
    });

    auto *copyScreenshot = ac->addAction(QStringLiteral("graph_copy_screenshot"));
    copyScreenshot->setText(i18n("Copy graph screenshot"));
    copyScreenshot->setIcon(QIcon::fromTheme(QStringLiteral("camera-photo")));
    connect(copyScreenshot, &QAction::triggered, this, [this]() {
        if (m_commandDispatch)
            m_commandDispatch(QStringLiteral("copy-screenshot"));
    });

    auto *animate = ac->addAction(QStringLiteral("graph_animate"));
    animate->setText(i18n("Animate"));
    animate->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    connect(animate, &QAction::triggered, this, [this]() {
        if (auto *tab = m_boundView ? m_boundView->graphWidget() : nullptr)
            tab->animate();
    });

    auto *resetForces = ac->addAction(QStringLiteral("graph_reset_forces"));
    resetForces->setText(i18n("Reset forces"));
    resetForces->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
    connect(resetForces, &QAction::triggered, this, [this]() {
        if (auto *tab = m_boundView ? m_boundView->graphWidget() : nullptr)
            tab->resetForces();
    });

    auto *zoomToFit = ac->addAction(QStringLiteral("graph_zoom_to_fit"));
    zoomToFit->setText(i18n("Zoom to Fit"));
    zoomToFit->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-best")));
    KActionCollection::setDefaultShortcut(zoomToFit, QKeySequence(Qt::SHIFT | Qt::Key_1));
    connect(zoomToFit, &QAction::triggered, this, [this]() {
        if (auto *tab = m_boundView ? m_boundView->graphWidget() : nullptr)
            tab->zoomToFitGraph();
    });
}

} // namespace Corbomite