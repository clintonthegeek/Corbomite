// src/plugins/graph-view/GraphView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GraphView.h"
#include "GraphViewTab.h"

#include "corbomite/core/MenuSectionHelper.h"

#include <forcegraph/ForceGraphView.h>

#include <KLocalizedString>

#include <QAction>
#include <QIcon>
#include <QVBoxLayout>

namespace Corbomite {

GraphView::GraphView(WorkspaceLeaf *leaf, QWidget *parent)
    : ItemView(leaf, parent)
{
}

View *GraphView::factory(WorkspaceLeaf *leaf)
{
    return new GraphView(leaf);
}

QString GraphView::getViewType() const { return QStringLiteral("graph"); }
QString GraphView::getDisplayText() const { return i18n("Graph view"); }
QString GraphView::getIcon() const { return QStringLiteral("network-wired"); }

void GraphView::setSearch(SearchProxy *search) { m_search = search; }
void GraphView::setVault(VaultProxy *vault) { m_vault = vault; }

void GraphView::setMetadataCache(MetadataCacheReader *cache)
{
    m_pendingCache = cache;
    if (m_graphWidget) m_graphWidget->setMetadataCache(cache);
}

void GraphView::setControlsPanel(GraphControlsPanel *panel)
{
    m_pendingPanel = panel;
    if (m_graphWidget) m_graphWidget->setControlsPanel(panel);
}

GraphViewTab *GraphView::graphWidget() const { return m_graphWidget; }

void GraphView::zoomIn()
{
    if (m_graphWidget)
        if (auto *gv = m_graphWidget->graphView())
            gv->zoomIn();
}

void GraphView::zoomOut()
{
    if (m_graphWidget)
        if (auto *gv = m_graphWidget->graphView())
            gv->zoomOut();
}

void GraphView::zoomReset()
{
    if (m_graphWidget)
        if (auto *gv = m_graphWidget->graphView())
            gv->resetTransform();
}

void GraphView::onOpen()
{
    ItemView::onOpen();
    if (!m_graphWidget && m_search && m_vault) {
        m_graphWidget = new GraphViewTab(m_search, m_vault, contentWidget());
        auto *layout = new QVBoxLayout(contentWidget());
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_graphWidget);

        connect(m_graphWidget, &GraphViewTab::noteActivated,
                this, &GraphView::noteActivated);

        // Apply services that were set before the widget existed.
        if (m_pendingCache) m_graphWidget->setMetadataCache(m_pendingCache);
        if (m_pendingPanel) m_graphWidget->setControlsPanel(m_pendingPanel);
    }
}

void GraphView::setGraphCommandDispatcher(CommandDispatch dispatcher)
{
    m_graphCommandDispatcher = std::move(dispatcher);
}

void GraphView::onMoreOptionsMenu(MenuSectionHelper &helper)
{
    auto dispatch = [this](const QString &cmd) {
        if (m_graphCommandDispatcher) m_graphCommandDispatcher(cmd);
    };

    // ---- pane: Split right / Split down ----
    auto *splitR = new QAction(
        QIcon::fromTheme(QStringLiteral("view-split-left-right")),
        i18n("Split right"), this);
    connect(splitR, &QAction::triggered, this,
            [dispatch] { dispatch(QStringLiteral("split_right")); });
    helper.addToSection(splitR, QStringLiteral("pane"));

    auto *splitD = new QAction(
        QIcon::fromTheme(QStringLiteral("view-split-top-bottom")),
        i18n("Split down"), this);
    connect(splitD, &QAction::triggered, this,
            [dispatch] { dispatch(QStringLiteral("split_down")); });
    helper.addToSection(splitD, QStringLiteral("pane"));

    // ---- action: Copy screenshot (graph:copy-screenshot via plugin) ----
    auto *screenshotAct = new QAction(
        QIcon::fromTheme(QStringLiteral("camera-photo")),
        i18n("Copy screenshot"), this);
    connect(screenshotAct, &QAction::triggered, this, [dispatch] {
        dispatch(QStringLiteral("corbomite-graph-view:copy-screenshot"));
    });
    helper.addToSection(screenshotAct, QStringLiteral("action"));

    // ---- action: Bookmark (placeholder — Cluster S) ----
    auto *bookmarkAct = new QAction(
        QIcon::fromTheme(QStringLiteral("bookmark-new")),
        i18n("Bookmark..."), this);
    bookmarkAct->setEnabled(false);
    bookmarkAct->setToolTip(
        i18n("Requires Bookmarks core plugin (Cluster S)"));
    helper.addToSection(bookmarkAct, QStringLiteral("action"));

    // GraphView is an ItemView, not an EditableFileView — no chain-up.
}

} // namespace Corbomite
