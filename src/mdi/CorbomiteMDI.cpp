/*
    SPDX-FileCopyrightText: 2005 Christoph Cullmann <cullmann@kde.org>
    SPDX-FileCopyrightText: 2002, 2003 Joseph Wenninger <jowenn@kde.org>
    SPDX-FileCopyrightText: 2026 Corbomite Contributors

    GUIClient partly based on ktoolbarhandler.cpp: SPDX-FileCopyrightText: 2002 Simon Hausmann <hausmann@kde.org>

    Adapted from Kate's KateMDI framework for use in Corbomite.

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "CorbomiteMDI.h"

#include <KAcceleratorManager>
#include <KActionCollection>
#include <KActionMenu>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>
#include <KToggleAction>
#include <KToolBar>
#include <KWindowConfig>
#include <KXMLGUIFactory>

#include <QApplication>
#include <QChildEvent>
#include <QContextMenuEvent>
#include <QDomDocument>
#include <QDrag>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QRubberBand>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace CorbomiteMDI
{

static KToggleAction *createToolViewToggleAction(const QString &text, ToolView *tv, QObject *parent)
{
    auto a = new KToggleAction(text, parent);
    a->setChecked(tv->toolVisible());
    QObject::connect(a, &KToggleAction::toggled, a, [tv](bool t) {
        if (tv->toolVisible() == t) {
            return;
        }
        if (t) {
            tv->mainWindow()->showToolView(tv);
            tv->setFocus();
        } else {
            tv->mainWindow()->hideToolView(tv);
        }
    });
    QObject::connect(tv, &ToolView::toolVisibleChanged, a, [a](bool v) {
        if (a->isChecked() != v) {
            a->setChecked(v);
        }
    });
    return a;
}

// BEGIN GUICLIENT

QString actionListName()
{
    return QStringLiteral("corbomite_mdi_view_actions");
}

GUIClient::GUIClient(MainWindow *mw)
    : QObject(mw)
    , KXMLGUIClient(mw)
    , m_mw(mw)
{
    setComponentName(QStringLiteral("toolviewmanager"), i18n("Toolview Manager"));
    connect(m_mw->guiFactory(), &KXMLGUIFactory::clientAdded, this, &GUIClient::clientAdded);
    const QString guiDescription = QStringLiteral(
        ""
        "<!DOCTYPE gui><gui name=\"corbomite_mdi_view_actions\">"
        "<MenuBar>"
        "    <Menu name=\"view\">"
        "        <ActionList name=\"%1\" />"
        "    </Menu>"
        "</MenuBar>"
        "</gui>");

    if (domDocument().documentElement().isNull()) {
        QString completeDescription = guiDescription.arg(actionListName());
        setXML(completeDescription, false /*merge*/);
    }

    m_sidebarButtonsMenu = new KActionMenu(i18n("Sidebar Buttons"), this);
    actionCollection()->addAction(QStringLiteral("corbomite_mdi_show_sidebar_buttons"), m_sidebarButtonsMenu);

    m_focusToolviewMenu = new KActionMenu(i18n("Focus Toolview"), this);
    actionCollection()->addAction(QStringLiteral("corbomite_mdi_focus_toolview"), m_focusToolviewMenu);

    m_toolMenu = new KActionMenu(i18n("Tool &Views"), this);
    actionCollection()->addAction(QStringLiteral("corbomite_mdi_toolview_menu"), m_toolMenu);
    m_showSidebarsAction = new KToggleAction(i18n("Show Side&bars"), this);
    actionCollection()->addAction(QStringLiteral("corbomite_mdi_sidebar_visibility"), m_showSidebarsAction);
    KActionCollection::setDefaultShortcut(m_showSidebarsAction, Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_F);

    m_showSidebarsAction->setChecked(m_mw->sidebarsVisible());
    connect(m_showSidebarsAction, &KToggleAction::toggled, m_mw, &MainWindow::setSidebarsVisible);

    m_hideToolViews = actionCollection()->addAction(QStringLiteral("corbomite_mdi_hide_toolviews"), m_mw, &MainWindow::hideToolViews);
    m_hideToolViews->setText(i18n("Hide All Tool Views"));

    m_toolMenu->addAction(m_showSidebarsAction);
    m_toolMenu->addAction(m_hideToolViews);
    auto *sep_act = new QAction(this);
    sep_act->setSeparator(true);
    m_toolMenu->addAction(sep_act);

    // Set config group
    actionCollection()->setConfigGroup(QStringLiteral("Shortcuts"));

    actionCollection()->addAssociatedWidget(m_mw);
    const auto actions = actionCollection()->actions();
    for (QAction *action : actions) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }
}

void GUIClient::updateSidebarsVisibleAction()
{
    m_showSidebarsAction->setChecked(m_mw->sidebarsVisible());
}

void GUIClient::registerToolView(ToolView *tv)
{
    QString aname = QLatin1String("corbomite_mdi_toolview_") + tv->id;

    auto shortcutsForActionName = [](const QString &actName) {
        QList<QKeySequence> shortcuts;
        KSharedConfigPtr cfg = KSharedConfig::openConfig();
        const QString shortcutString = cfg->group(QStringLiteral("Shortcuts")).readEntry(actName, QString());
        const auto shortcutStrings = shortcutString.split(QStringLiteral("; "));
        for (const QString &shortcut : shortcutStrings) {
            shortcuts << QKeySequence::fromString(shortcut);
        }
        return shortcuts;
    };

    /** Show ToolView Action **/
    KToggleAction *a = createToolViewToggleAction(i18n("Show %1", tv->text), tv, this);
    QString s = QKeySequence::listToString(shortcutsForActionName(aname));
    if (!(s.isEmpty())) {
        a->setShortcuts(shortcutsForActionName(aname));
    }
    actionCollection()->addAction(aname, a);

    m_toolMenu->addAction(a);

    Q_ASSERT(std::find_if(m_toolToActions.begin(),
                          m_toolToActions.end(),
                          [tv](const ToolViewActions &tvActs) {
                              return tvActs.toolview == tv;
                          })
             == m_toolToActions.end());

    m_toolToActions.push_back({tv, {a}});
    auto &actionsForTool = m_toolToActions.back().actions;

    /** Show Tab button in sidebar action **/
    aname = QStringLiteral("corbomite_mdi_show_toolview_button_") + tv->id;
    a = new KToggleAction(i18n("Show %1 Button", tv->text), this);
    a->setChecked(true);
    s = QKeySequence::listToString(shortcutsForActionName(aname));
    if (!(s.isEmpty())) {
        a->setShortcuts(shortcutsForActionName(aname));
    }
    actionCollection()->addAction(aname, a);
    connect(a, &KToggleAction::toggled, this, [toolview = QPointer<ToolView>(tv)](bool checked) {
        if (toolview) {
            const QSignalBlocker b(toolview);
            toolview->sidebar()->showToolviewTab(toolview, checked);
        }
    });
    connect(tv, &ToolView::tabButtonVisibleChanged, a, &QAction::setChecked);

    m_sidebarButtonsMenu->addAction(a);
    actionsForTool.push_back(a);

    aname = QStringLiteral("corbomite_mdi_focus_toolview_") + tv->id;
    auto *act = new QAction(i18n("Focus %1", tv->text), this);
    s = QKeySequence::listToString(shortcutsForActionName(aname));
    if (!(s.isEmpty())) {
        act->setShortcuts(shortcutsForActionName(aname));
    }
    actionCollection()->addAction(aname, act);
    connect(act, &QAction::triggered, tv, [tv = QPointer(tv)] {
        if (tv && tv->mainWindow()) {
            if (!tv->isVisible()) {
                tv->mainWindow()->showToolView(tv);
            }
            tv->setFocus();
        }
    });
    m_focusToolviewMenu->addAction(act);
    actionsForTool.push_back(act);

    updateActions();
}

void GUIClient::unregisterToolView(ToolView *tv)
{
    std::erase_if(m_toolToActions, [tv](const ToolViewActions &a) {
        if (a.toolview == tv) {
            qDeleteAll(a.actions);
            return true;
        }
        return false;
    });

    updateActions();
}

void GUIClient::clientAdded(KXMLGUIClient *client)
{
    if (client == this) {
        updateActions();
        actionCollection()->readSettings();
    }
}

void GUIClient::updateActions()
{
    if (!factory()) {
        return;
    }

    unplugActionList(actionListName());

    QList<QAction *> addList;
    addList.append(m_toolMenu);
    addList.append(m_sidebarButtonsMenu);
    addList.append(m_focusToolviewMenu);

    plugActionList(actionListName(), addList);
}

// END GUICLIENT

// BEGIN TOOLVIEW

ToolView::ToolView(MainWindow *mainwin, Sidebar *sidebar, QWidget *parent, const QString &identifier)
    : QFrame(parent)
    , m_mainWin(mainwin)
    , m_sidebar(sidebar)
    , m_toolbar(nullptr)
    , id(identifier)
    , m_toolVisible(false)
{
    QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    policy.setRetainSizeWhenHidden(true);
    setSizePolicy(policy);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);

    m_toolbar = new KToolBar(this);
    m_toolbar->setVisible(false);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    const int iconSize = style()->pixelMetric(QStyle::PM_ButtonIconSize, nullptr, this);
    m_toolbar->setIconSize(QSize(iconSize, iconSize));
}

QSize ToolView::sizeHint() const
{
    return size();
}

QSize ToolView::minimumSizeHint() const
{
    return QSize(160, 160);
}

bool ToolView::tabButtonVisible() const
{
    return isTabButtonVisible;
}

void ToolView::setTabButtonVisible(bool visible)
{
    isTabButtonVisible = visible;
}

ToolView::~ToolView()
{
    if (m_mainWin) {
        m_mainWin->toolViewDeleted(this);
    }
}

void ToolView::setToolVisible(bool vis)
{
    if (m_toolVisible == vis) {
        return;
    }

    m_toolVisible = vis;
    Q_EMIT toolVisibleChanged(m_toolVisible);
}

bool ToolView::toolVisible() const
{
    return m_toolVisible;
}

void ToolView::childEvent(QChildEvent *ev)
{
    if (ev->type() == QEvent::ChildAdded) {
        if (QWidget *widget = qobject_cast<QWidget *>(ev->child())) {
            setFocusProxy(widget);
            layout()->addWidget(widget);
        }
    }

    QFrame::childEvent(ev);
}

void ToolView::actionEvent(QActionEvent *event)
{
    QFrame::actionEvent(event);
    if (event->type() == QEvent::ActionAdded) {
        m_toolbar->addAction(event->action());
    } else if (event->type() == QEvent::ActionRemoved) {
        m_toolbar->removeAction(event->action());
    }
    m_toolbar->setVisible(!m_toolbar->actions().isEmpty());
}

// END TOOLVIEW

// BEGIN SIDEBAR

MultiTabBar::MultiTabBar(KMultiTabBar::KMultiTabBarPosition pos, Sidebar *sb, int idx)
    : m_sb(sb)
    , m_stack(new QStackedWidget())
    , m_multiTabBar(new KMultiTabBar(pos, this))
{
    setProperty("is-multi-tabbar", true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_multiTabBar);

    m_sb->m_ownSplit->insertWidget(idx, m_stack);
    m_stack->hide();
}

MultiTabBar::~MultiTabBar()
{
    m_stack->deleteLater();
}

KMultiTabBarTab *MultiTabBar::addTab(int id, ToolView *tv)
{
    m_stack->addWidget(tv);
    KMultiTabBarTab *newTab;

    if (std::find(m_tabList.begin(), m_tabList.end(), id) != m_tabList.end()) {
        // We are in session restore
        newTab = m_multiTabBar->tab(id);
        newTab->setIcon(tv->icon);
        newTab->setText(tv->text);
    } else {
        m_tabList.push_back(id);
        m_multiTabBar->appendTab(tv->icon, id, tv->text);
        newTab = m_multiTabBar->tab(id);
    }

    connect(newTab, &KMultiTabBarTab::clicked, this, &MultiTabBar::tabClicked);

    return newTab;
}

int MultiTabBar::addBlankTab()
{
    int id = m_sb->nextId();
    m_tabList.push_back(id);
    m_multiTabBar->appendTab(QIcon(), id, QStringLiteral("placeholder"));
    return id;
}

void MultiTabBar::tabClicked(int id)
{
    if (m_multiTabBar->isTabRaised(id) || m_sb->isCollapsed()) {
        showToolView(id);
    } else {
        hideToolView(id);
    }

    m_sb->updateSidebar();
}

void MultiTabBar::removeBlankTab(int id)
{
    m_tabList.erase(std::remove(m_tabList.begin(), m_tabList.end(), id), m_tabList.end());
    m_multiTabBar->removeTab(id);
    if (tabCount() == 0) {
        m_activeTab = 0;
        Q_EMIT lastTabRemoved(this);
    }
}

void MultiTabBar::removeTab(int id, ToolView *tv)
{
    m_tabList.erase(std::remove(m_tabList.begin(), m_tabList.end(), id), m_tabList.end());
    m_multiTabBar->removeTab(id);

    bool hideView = (m_stack->currentWidget() == tv);
    m_stack->removeWidget(tv);

    if (tabCount() == 0) {
        m_activeTab = 0;
        Q_EMIT lastTabRemoved(this);
        return;
    }

    if (!hideView) {
        return;
    }

    m_activeTab = 0;
    tv = static_cast<ToolView *>(m_stack->currentWidget());
    if (tv) {
        auto it = std::find_if(m_sb->m_toolviews.begin(), m_sb->m_toolviews.end(), [tv](const Sidebar::ToolViewData &d) {
            return d.toolview == tv;
        });
        if (it != m_sb->m_toolviews.end()) {
            hideToolView(it->id);
        }
    }
}

void MultiTabBar::reorderTab(int id, KMultiTabBarTab *before)
{
    auto it = std::find(m_tabList.begin(), m_tabList.end(), id);
    if (it == m_tabList.end()) {
        return;
    }

    const qsizetype idIdx = std::distance(m_tabList.begin(), it);
    it = before ? std::find(m_tabList.begin(), m_tabList.end(), before->id()) : m_tabList.end();
    if (before && it == m_tabList.end()) {
        return;
    }
    const qsizetype beforeIdx = it == m_tabList.end() ? m_tabList.size() - 1 : std::distance(m_tabList.begin(), it);
    if (idIdx == beforeIdx) {
        return;
    }

    const qsizetype start = std::min(idIdx, beforeIdx);

    const int beforeId = before ? before->id() : -1;

    for (size_t i = start; i < m_tabList.size(); ++i) {
        KMultiTabBarTab *oldTab = m_multiTabBar->tab(m_tabList[i]);
        oldTab->removeEventFilter(m_sb);
        m_multiTabBar->removeTab(m_tabList[i]);
    }

    m_tabList.erase(std::remove(m_tabList.begin(), m_tabList.end(), id), m_tabList.end());
    it = before ? std::find(m_tabList.begin(), m_tabList.end(), beforeId) : m_tabList.end();
    m_tabList.insert(it, id);

    for (size_t i = start; i < m_tabList.size(); ++i) {
        int tabId = m_tabList[i];
        ToolView *tv = m_sb->dataForId(tabId).toolview;
        m_multiTabBar->appendTab(tv->icon, tabId, tv->text);
        m_sb->appendStyledTab(tabId, this, tv);
    }
}

void MultiTabBar::showToolView(int id)
{
    setTabActive(id, true);
    expandToolView();
}

void MultiTabBar::hideToolView(int id)
{
    setTabActive(id, false);
    collapseToolView();
}

void MultiTabBar::setTabActive(int id, bool state)
{
    if (m_activeTab == id) {
        m_multiTabBar->setTab(id, state);
        m_sb->dataForId(id).toolview->setToolVisible(state);
        m_activeTab = state ? id : 0;
        return;
    }

    if (m_activeTab && state) {
        m_multiTabBar->setTab(m_activeTab, false);
        m_sb->dataForId(m_activeTab).toolview->setToolVisible(false);
    }

    m_multiTabBar->setTab(id, state);
    m_sb->dataForId(id).toolview->setToolVisible(state);
    m_activeTab = state ? id : m_activeTab;
}

bool MultiTabBar::isToolActive() const
{
    return m_activeTab > 0;
}

void MultiTabBar::collapseToolView() const
{
    m_stack->hide();

    if (m_stack->count() < 1) {
        return;
    }

    static_cast<ToolView *>(m_stack->currentWidget())->setToolVisible(false);
}

bool MultiTabBar::expandToolView() const
{
    if (!m_activeTab) {
        return false;
    }

    if (m_stack->count() < 1) {
        return false;
    }

    ToolView *tv = m_sb->dataForId(m_activeTab).toolview;
    tv->setToolVisible(true);
    tv->setFocus();
    m_stack->setCurrentWidget(tv);
    m_stack->show();

    return true;
}

Sidebar::Sidebar(KMultiTabBar::KMultiTabBarPosition pos, QSplitter *sp, MainWindow *mainwin, QWidget *parent)
    : QSplitter(parent)
    , m_mainWin(mainwin)
    , m_tabBarPosition(pos)
    , m_splitter(sp)
    , m_ownSplit(new QSplitter(sp))
    , m_ownSplitIndex(sp->indexOf(m_ownSplit))
    , m_lastSize(200)
    , m_dropIndicator(new QRubberBand(QRubberBand::Rectangle, mainwin))
    , m_internalDropIndicator(new QRubberBand(QRubberBand::Rectangle, mainwin))
{
    setChildrenCollapsible(false);
    setAcceptDrops(true);
    connect(this, &Sidebar::destroyed, m_dropIndicator, &QObject::deleteLater);

    if (isVertical()) {
        m_ownSplit->setOrientation(Qt::Vertical);
        setOrientation(Qt::Vertical);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    } else {
        m_ownSplit->setOrientation(Qt::Horizontal);
        setOrientation(Qt::Horizontal);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    m_ownSplit->setChildrenCollapsible(false);

    // ensure proper sidebar state
    m_ownSplit->hide();

    connect(this, &QSplitter::splitterMoved, this, &Sidebar::barSplitMoved);
    connect(m_ownSplit, &QSplitter::splitterMoved, this, &Sidebar::ownSplitMoved);
    connect(m_splitter, &QSplitter::splitterMoved, this, &Sidebar::handleCollapse);

    insertTabBar();

    // apply initial config
    readConfig();
}

void Sidebar::readConfig()
{
    bool needsUpdate = false;

    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    KConfigGroup cgGeneral = KConfigGroup(config, QStringLiteral("General"));
    const bool syncWithTabs = cgGeneral.readEntry("Sync section size with tab positions", false);
    if (syncWithTabs != m_syncWithTabs) {
        m_syncWithTabs = syncWithTabs;
        needsUpdate = true;
    }
    // Ignore the option for the bottom bar
    if (position() == KMultiTabBar::Bottom) {
        m_syncWithTabs = false;
        needsUpdate = false;
    }
    if (m_syncWithTabs && needsUpdate) {
        QList<int> wsizes = m_ownSplit->sizes();
        for (int i = 0; i < wsizes.count(); ++i) {
            if (wsizes.at(i) == 0) {
                wsizes[i] = tabBar(i)->sectionSize();
            }
        }
        setSizes(wsizes);
        adjustSplitterSections();
        needsUpdate = false;
    }

    const bool showTextForLeftRight = cgGeneral.readEntry("Show text for left and right sidebar", false);
    if (showTextForLeftRight != m_showTextForLeftRight) {
        m_showTextForLeftRight = showTextForLeftRight;
        needsUpdate = true;
    }

    int size = cgGeneral.readEntry("Icon size for left and right sidebar buttons", 32);
    if (size != m_leftRightSidebarIconSize) {
        m_leftRightSidebarIconSize = size;
        needsUpdate = true;
    }

    if (!needsUpdate) {
        return;
    }

    for (const auto &[id, wid, _] : m_toolviews) {
        updateButtonStyle(kmTabBar(wid)->tab(id));
    }
}

void Sidebar::appendStyledTab(int id, MultiTabBar *bar, ToolView *widget)
{
    auto newTab = bar->addTab(id, widget);
    auto it = std::find_if(m_toolviews.begin(), m_toolviews.end(), [id](const Sidebar::ToolViewData &d) {
        return d.id == id;
    });
    Q_ASSERT(it != m_toolviews.end());
    it->tabbar = bar;

    Q_ASSERT(newTab);
    newTab->installEventFilter(this);

    newTab->setProperty("corbomite_original_text", widget->text);

    updateButtonStyle(newTab);

    QMetaObject::invokeMethod(
        m_mainWin,
        [w = m_mainWin, widget, newTab] {
            Q_EMIT w->tabForToolViewAdded(widget, newTab);
        },
        Qt::QueuedConnection);
}

void Sidebar::updateButtonStyle(KMultiTabBarTab *button)
{
    const auto originalText = button->property("corbomite_original_text").toString();
    if (!m_showTextForLeftRight && (position() == KMultiTabBar::Left || position() == KMultiTabBar::Right)) {
        const int iconSize = m_leftRightSidebarIconSize;
        button->setIconSize(QSize(iconSize, iconSize));
        button->setText(QString());
        button->setToolTip(originalText);
    } else {
        const int iconSize = style()->pixelMetric(QStyle::PM_ButtonIconSize, nullptr, this);
        button->setIconSize(QSize(iconSize, iconSize));
        button->setText(originalText);
        button->setToolTip(QString());
    }
}

void Sidebar::setStyle(KMultiTabBar::KMultiTabBarStyle style)
{
    m_tabBarStyle = style;

    for (int i = 0; i < tabBarCount(); ++i) {
        tabBar(i)->tabBar()->setStyle(style);
    }
}

QSize Sidebar::sizeHint() const
{
    return minimumSizeHint();
}

QSize Sidebar::minimumSizeHint() const
{
    return isVisible() ? QSplitter::minimumSizeHint() : QSize{0, 0};
}

MultiTabBar *Sidebar::insertTabBar(int idx /* = -1*/)
{
    auto *newBar = new MultiTabBar(m_tabBarPosition, this, idx);
    newBar->installEventFilter(this);
    newBar->tabBar()->setStyle(tabStyle());
    QList<int> sections = sizes();
    insertWidget(idx, newBar);

    idx = idx < 0 ? sections.count() : idx;
    if (idx) {
        if (m_syncWithTabs) {
            sections[idx - 1] = sections.at(idx - 1) / 2;
            sections.insert(idx, sections.at(idx - 1));
            setSizes(sections);
        } else {
            QTimer::singleShot(100, this, [this, idx, sections]() {
                if (tabBarCount() - 1 < idx) {
                    return;
                }
                QList<int> sectionsC(sections);
                if (sectionsC.count() == 1) {
                    int oldTabSize = isVertical() ? tabBar(idx - 1)->sizeHint().height() : tabBar(idx - 1)->sizeHint().width();
                    sectionsC[0] -= oldTabSize;
                    sectionsC.insert(0, oldTabSize);
                } else {
                    int newTabSize = isVertical() ? tabBar(idx)->sizeHint().height() : tabBar(idx)->sizeHint().width();
                    for (int i = 0; i < sections.size(); ++i) {
                        sectionsC[i] -= newTabSize;
                    }
                    sectionsC.insert(idx, newTabSize);
                }
                setSizes(sectionsC);
            });
        }
    }

    connect(newBar, &MultiTabBar::lastTabRemoved, this, &Sidebar::tabBarIsEmpty);

    return newBar;
}

void Sidebar::updateLastSizeOnResize()
{
    const int splitHandleIndex = qMin(m_splitter->indexOf(m_ownSplit) + 1, m_splitter->count() - 1);
    Q_ASSERT(splitHandleIndex > 0);
    m_splitter->handle(splitHandleIndex)->installEventFilter(this);
}

int Sidebar::nextId()
{
    static int id = 0;
    return ++id;
}

ToolView *Sidebar::addToolView(const QIcon &icon, const QString &text, const QString &identifier, ToolView *widget)
{
    if (widget) {
        if (widget->sidebar() == this) {
            return widget;
        }

        widget->sidebar()->removeToolView(widget);

    } else {
        widget = new ToolView(m_mainWin, this, nullptr, identifier);
        widget->icon = icon;
        widget->text = text;
    }

    widget->m_sidebar = this;

    auto blankTabId = std::find_if(m_tvIdToTabId.begin(), m_tvIdToTabId.end(), [&identifier](const ToolViewToTabId &a) {
        return a.toolview == identifier;
    });
    if (blankTabId != m_tvIdToTabId.end()) {
        int newId = blankTabId->tabId;
        m_toolviews.push_back({.id = newId, .toolview = widget, .tabbar = nullptr});
        appendStyledTab(newId, tabBar(blankTabId->tabbarId), widget);
        m_tvIdToTabId.erase(blankTabId);
    } else {
        int newId = nextId();
        m_toolviews.push_back({.id = newId, .toolview = widget, .tabbar = nullptr});
        appendStyledTab(newId, tabBar(0), widget);
    }

    show();

    return widget;
}

bool Sidebar::removeToolView(ToolView *widget)
{
    auto it = std::find_if(m_toolviews.begin(), m_toolviews.end(), [widget](const Sidebar::ToolViewData &d) {
        return d.toolview == widget;
    });
    if (it == m_toolviews.end()) {
        return false;
    }

    int id = it->id;
    MultiTabBar *tabbar = it->tabbar;
    m_toolviews.erase(it);

    tabbar->removeTab(id, widget);
    updateSidebar();
    return true;
}

bool Sidebar::showToolView(ToolView *widget)
{
    auto it = std::find_if(m_toolviews.begin(), m_toolviews.end(), [widget](const Sidebar::ToolViewData &d) {
        return d.toolview == widget;
    });
    if (it == m_toolviews.end()) {
        return false;
    }

    tabBar(widget)->showToolView(it->id);
    updateSidebar();

    return true;
}

bool Sidebar::hideToolView(ToolView *widget)
{
    auto it = std::find_if(m_toolviews.begin(), m_toolviews.end(), [widget](const Sidebar::ToolViewData &d) {
        return d.toolview == widget;
    });
    if (it == m_toolviews.end()) {
        return false;
    }

    updateLastSize();
    tabBar(widget)->hideToolView(it->id);
    updateSidebar();

    return true;
}

void Sidebar::showToolviewTab(ToolView *widget, bool show)
{
    auto it = std::find_if(m_toolviews.begin(), m_toolviews.end(), [widget](const Sidebar::ToolViewData &d) {
        return d.toolview == widget;
    });
    if (it == m_toolviews.end()) {
        return;
    }
    KMultiTabBarTab *tab = kmTabBar(widget)->tab(it->id);
    if (widget->tabButtonVisible() == show) {
        return;
    } else {
        widget->setTabButtonVisible(show);
        tab->setVisible(show);
        Q_EMIT widget->tabButtonVisibleChanged(show);
    }
}

bool Sidebar::isCollapsed()
{
    return m_splitter->sizes().at(m_ownSplitIndex) == 0;
}

ToolView *Sidebar::firstVisibleToolView()
{
    for (int i = 0; i < tabBarCount(); ++i) {
        MultiTabBar *tabbar = tabBar(i);
        if (tabbar->isToolActive()) {
            Q_ASSERT(tabbar->tabCount() > 0);
            int id = tabbar->activeTab();
            auto it = std::find_if(m_toolviews.begin(), m_toolviews.end(), [id](const Sidebar::ToolViewData &d) {
                return d.id == id;
            });
            return it->toolview;
        }
    }
    return nullptr;
}

void Sidebar::handleCollapse(int pos, int index)
{
    Q_UNUSED(pos);

    const bool myInterest = ((m_ownSplitIndex == 0) && (1 == index)) || (m_ownSplitIndex == index);
    if (!myInterest) {
        return;
    }

    if (isCollapsed() && !m_isPreviouslyCollapsed) {
        if (!m_resizePlaceholder) {
            m_resizePlaceholder = new QLabel();
            m_ownSplit->addWidget(m_resizePlaceholder);
            m_resizePlaceholder->show();
            m_resizePlaceholder->setMinimumSize(QSize(160, 160));
        }
        collapseSidebar();
    } else if (!isCollapsed() && m_isPreviouslyCollapsed) {
        updateSidebar();
    }
}

void Sidebar::ownSplitMoved(int pos, int index)
{
    QList<int> wsizes = m_ownSplit->sizes();
    for (int i = 0; i < tabBarCount(); ++i) {
        if (tabBar(i)->isToolActive()) {
            tabBar(i)->setSectionSize(wsizes.at(i));
        }
    }

    if (m_syncWithTabs) {
        moveSplitter(pos, index);
    }
}

void Sidebar::barSplitMoved(int pos, int index)
{
    Q_UNUSED(pos);
    Q_UNUSED(index);

    if (m_syncWithTabs) {
        adjustSplitterSections();
    }
}

bool Sidebar::tabBarIsEmpty(MultiTabBar *bar)
{
    if (!bar || bar->tabCount() > 0 || tabBarCount() == 1) {
        return false;
    }

    delete bar;

    QTimer::singleShot(0, this, [this]() {
        updateSidebar();
    });

    return true;
}

void Sidebar::collapseSidebar()
{
    if (m_isPreviouslyCollapsed) {
        return;
    }

    updateLastSize();

    for (int i = 0; i < tabBarCount(); ++i) {
        tabBar(i)->collapseToolView();
    }

    m_isPreviouslyCollapsed = true;

    if (!m_resizePlaceholder) {
        m_ownSplit->hide();
    } else {
        QList<int> wsizes = m_splitter->sizes();
        wsizes[m_ownSplitIndex] = 0;
        m_splitter->setSizes(wsizes);
    }

    m_mainWin->triggerFocusForCentralWidget();
}

bool Sidebar::adjustSplitterSections()
{
    bool anyVis = false;
    QList<int> wsizes = sizes();
    int sizeCollector = 0;
    int lastExpandedId = -1;
    for (int i = tabBarCount() - 1; i > -1; --i) {
        sizeCollector += wsizes.at(i);
        if (tabBar(i)->expandToolView()) {
            anyVis = true;
            wsizes[i] = sizeCollector;
            sizeCollector = 0;
            lastExpandedId = i;
        } else {
            wsizes[i] = 0;
        }
    }

    if (!anyVis) {
        return false;
    }

    if (!m_syncWithTabs) {
        return true;
    }

    if (sizeCollector && lastExpandedId > -1) {
        wsizes[lastExpandedId] += sizeCollector;
    }

    m_ownSplit->setSizes(wsizes);

    return true;
}

void Sidebar::updateSidebar()
{
    if (!adjustSplitterSections()) {
        collapseSidebar();
        return;
    }

    if (m_resizePlaceholder) {
        delete m_resizePlaceholder;
    }

    m_isPreviouslyCollapsed = false;

    if (isCollapsed()) {
        QList<int> wsizes = m_splitter->sizes();
        wsizes[m_ownSplitIndex] = m_lastSize;
        m_splitter->setSizes(wsizes);
    }

    if (!m_syncWithTabs) {
        QList<int> wsizes = m_ownSplit->sizes();
        for (int i = 0; i < tabBarCount(); ++i) {
            wsizes[i] = tabBar(i)->isToolActive() ? tabBar(i)->sectionSize() : 0;
        }
        m_ownSplit->setSizes(wsizes);
    }

    m_ownSplit->show();
}

bool Sidebar::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::ContextMenu) {
        auto *e = static_cast<QContextMenuEvent *>(ev);
        auto *bt = qobject_cast<KMultiTabBarTab *>(obj);
        if (bt) {
            m_popupButton = bt->id();

            auto it = std::find_if(m_toolviews.begin(), m_toolviews.end(), [id = m_popupButton](const Sidebar::ToolViewData &d) {
                return d.id == id;
            });
            Q_ASSERT(it != m_toolviews.end());
            ToolView *w = it->toolview;

            if (w) {
                QMenu menu(this);

                menu.addSection(w->icon, w->text);

                // No plugin configure action in Corbomite (removed KTextEditor::Plugin dependency)

                menu.addAction(i18n("Hide Button"))->setData(HideButtonAction);

                menu.addSection(QIcon::fromTheme(QStringLiteral("move")), i18n("Move To"));

                int tabBarId = indexOf(it->tabbar);

                if (tabBar(tabBarId)->tabCount() > 1) {
                    menu.addAction(QIcon::fromTheme(QStringLiteral("list-add")), i18n("Own Section"))->setData(ToOwnSectAction);
                }

                if (tabBarCount() > 1) {
                    if (tabBarId < 1) {
                        if (isVertical()) {
                            menu.addAction(QIcon::fromTheme(QStringLiteral("go-down")), i18n("One Down"))->setData(DownRightAction);
                        } else {
                            menu.addAction(QIcon::fromTheme(QStringLiteral("go-next")), i18n("One Right"))->setData(DownRightAction);
                        }
                    } else {
                        if (isVertical()) {
                            menu.addAction(QIcon::fromTheme(QStringLiteral("go-up")), i18n("One Up"))->setData(UpLeftAction);
                            if (tabBarId < tabBarCount() - 1) {
                                menu.addAction(QIcon::fromTheme(QStringLiteral("go-down")), i18n("One Down"))->setData(DownRightAction);
                            }
                        } else {
                            menu.addAction(QIcon::fromTheme(QStringLiteral("go-previous")), i18n("One Left"))->setData(UpLeftAction);
                            if (tabBarId < tabBarCount() - 1) {
                                menu.addAction(QIcon::fromTheme(QStringLiteral("go-next")), i18n("One Right"))->setData(DownRightAction);
                            }
                        }
                    }
                }

                if (position() != 0) {
                    menu.addAction(QIcon::fromTheme(QStringLiteral("go-previous")), i18n("Left Sidebar"))->setData(0);
                }

                if (position() != 1) {
                    menu.addAction(QIcon::fromTheme(QStringLiteral("go-next")), i18n("Right Sidebar"))->setData(1);
                }

                if (position() != 2) {
                    menu.addAction(QIcon::fromTheme(QStringLiteral("go-up")), i18n("Top Sidebar"))->setData(2);
                }

                if (position() != 3) {
                    menu.addAction(QIcon::fromTheme(QStringLiteral("go-down")), i18n("Bottom Sidebar"))->setData(3);
                }

                connect(&menu, &QMenu::triggered, this, &Sidebar::buttonPopupActivate);

                menu.exec(e->globalPos());

                return true;
            }
        }
    } else if (ev->type() == QEvent::MouseButtonRelease) {
        auto *e = static_cast<QMouseEvent *>(ev);
        if (e->button() == Qt::LeftButton) {
            updateLastSize();
        }
    } else if (ev->type() == QEvent::MouseButtonPress) {
        auto *e = static_cast<QMouseEvent *>(ev);
        if (qobject_cast<MultiTabBar *>(obj)) {
            if (e->button() == Qt::LeftButton) {
                if (obj->property("is-multi-tabbar").toBool()) {
                    if (isCollapsed()) {
                        updateSidebar();
                    } else {
                        collapseSidebar();
                    }
                    return true;
                }
            }
        } else if (qobject_cast<KMultiTabBarTab *>(obj) && e->button() == Qt::LeftButton) {
            dragStartPos = e->pos();
        }
    } else if (!dragStartPos.isNull() && ev->type() == QEvent::MouseMove) {
        auto *e = static_cast<QMouseEvent *>(ev);
        auto *tab = qobject_cast<KMultiTabBarTab *>(obj);
        if (tab && (e->pos() - dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
            QPixmap pixmap = tab->grab();
            auto *drag = new QDrag(this);
            auto md = new QMimeData();
            ToolView *toolView = dataForId(tab->id()).toolview;
            Q_ASSERT(toolView);
            md->setProperty("toolviewToMove", QVariant::fromValue(toolView));
            drag->setMimeData(md);
            drag->setPixmap(pixmap);
            drag->setHotSpot(dragStartPos);
            dragStartPos = {};
            connect(drag, &QObject::destroyed, this, &Sidebar::dragEnded);

            Q_EMIT dragStarted();

            drag->exec(Qt::MoveAction);
            return true;
        }
    }

    return QSplitter::eventFilter(obj, ev);
}

void Sidebar::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->proposedAction() != Qt::MoveAction) {
        return;
    }

    if (!e->mimeData() || !e->mimeData()->property("toolviewToMove").value<ToolView *>()) {
        return;
    }

    if (e->source() == this) {
        if (toolviewCount() == 1) {
            return;
        }
        m_internalDropIndicator->raise();
        if (m_internalDropIndicator->geometry() != geometry()) {
            m_internalDropIndicator->setGeometry(geometry());
        }

        if (isVertical()) {
            m_internalDropIndicator->setFixedHeight(4);
        } else {
            m_internalDropIndicator->setFixedWidth(4);
        }

        auto mimeData = e->mimeData();
        auto *toolview = mimeData->property("toolviewToMove").value<ToolView *>();
        QWidget *tab = tabButtonForToolview(toolview);
        if (!tab || !toolview) {
            return;
        }

        const QPoint tabPos = tab->pos();
        auto globalPos = mapToGlobal(tabPos);
        auto pos = m_mainWin->mapFromGlobal(globalPos);
        m_internalDropIndicator->move(pos);
        m_internalDropIndicator->show();
    } else {
        m_dropIndicator->raise();
        if (m_dropIndicator->geometry() != geometry()) {
            m_dropIndicator->setGeometry(geometry());
        }
        auto globalPos = mapToGlobal(pos());
        auto indicatorPos = m_dropIndicator->mapFromGlobal(globalPos);
        if (indicatorPos != m_dropIndicator->pos()) {
            m_dropIndicator->move(indicatorPos);
        }

        m_dropIndicator->show();
    }
    e->acceptProposedAction();
}

void Sidebar::dropEvent(QDropEvent *e)
{
    auto mimeData = e->mimeData();
    m_dropIndicator->hide();
    m_internalDropIndicator->hide();

    if (!mimeData) {
        return;
    }
    auto *toolview = mimeData->property("toolviewToMove").value<ToolView *>();
    if (!toolview) {
        return;
    }

    if (e->source() == this) {
        auto *sourceTab = qobject_cast<KMultiTabBarTab *>(tabButtonForToolview(toolview));
        if (!sourceTab) {
            return;
        }
        auto destTab = qobject_cast<KMultiTabBarTab *>(childAt(e->position().toPoint()));
        auto it = std::find_if(m_toolviews.begin(), m_toolviews.end(), [toolview](const Sidebar::ToolViewData &d) {
            return d.toolview == toolview;
        });
        Q_ASSERT(it != m_toolviews.end());
        it->tabbar->reorderTab(sourceTab->id(), destTab);
    } else {
        m_mainWin->moveToolView(toolview, position(), /*isDND=*/true);
        m_mainWin->showToolView(toolview);
    }

    e->accept();
}

void Sidebar::dragMoveEvent(QDragMoveEvent *e)
{
    if (e->source() != this || toolviewCount() == 1) {
        return;
    }

    auto *tab = qobject_cast<KMultiTabBarTab *>(childAt(e->position().toPoint()));
    if (tab) {
        const QPoint tabPos = tab->pos();
        auto globalPos = mapToGlobal(tabPos);
        auto pos = m_mainWin->mapFromGlobal(globalPos);
        m_internalDropIndicator->move(pos);
    } else {
        auto mimeData = e->mimeData();
        auto *toolview = mimeData->property("toolviewToMove").value<ToolView *>();
        if (!toolview) {
            return;
        }

        auto it = std::find_if(m_toolviews.begin(), m_toolviews.end(), [toolview](const Sidebar::ToolViewData &d) {
            return d.toolview == toolview;
        });
        Q_ASSERT(it != m_toolviews.end());
        MultiTabBar *tabbar = it->tabbar;
        int lastTabId = tabbar->tabList().back();
        KMultiTabBarTab *lastTab = tabbar->tabBar()->tab(lastTabId);

        QPoint tabPos = lastTab->pos();
        if (isVertical()) {
            tabPos.setY(tabPos.y() + lastTab->height());
        } else {
            tabPos.setX(tabPos.x() + lastTab->width());
        }
        auto globalPos = mapToGlobal(tabPos);
        auto pos = m_mainWin->mapFromGlobal(globalPos);
        m_internalDropIndicator->move(pos);
    }
}

void Sidebar::dragLeaveEvent(QDragLeaveEvent *)
{
    m_internalDropIndicator->hide();
    m_dropIndicator->hide();
}

void Sidebar::setVisible(bool visible)
{
    // Guard: honour the user's "Show Sidebars" master toggle. Previously
    // also bailed when `m_toolviews.empty()`, but that broke the Cluster Q
    // flow where all tool views arrive from plugins AFTER the window has
    // shown — the initial show(true) was silently dropped and nothing
    // re-triggered when plugins added their tool views. An empty sidebar
    // collapses to a ~25px tab strip; that's fine.
    if (visible && !m_mainWin->sidebarsVisible()) {
        return;
    }

    QSplitter::setVisible(visible);
}

void Sidebar::buttonPopupActivate(QAction *a)
{
    const int id = a->data().toInt();
    ToolViewData tvData = dataForId(m_popupButton);
    ToolView *w = tvData.toolview;

    if (!w) {
        return;
    }

    // move to other Sidebar ids
    if (id < 4) {
        m_mainWin->moveToolView(w, static_cast<KMultiTabBar::KMultiTabBarPosition>(id));
        m_mainWin->showToolView(w);
    }

    // ConfigureAction removed (no KTextEditor::Plugin)

    if (id == HideButtonAction) {
        showToolviewTab(w, false);
    }

    if (id == ToOwnSectAction) {
        MultiTabBar *newBar = insertTabBar(indexOf(tvData.tabbar) + 1);
        tabBar(w)->removeTab(tvData.id, w);
        appendStyledTab(tvData.id, newBar, w);
        showToolView(w);
    }
    if (id == UpLeftAction) {
        MultiTabBar *newBar = tabBar(indexOf(tabBar(w)) - 1);
        tabBar(w)->removeTab(tvData.id, w);
        appendStyledTab(tvData.id, newBar, w);
    }
    if (id == DownRightAction) {
        MultiTabBar *newBar = tabBar(indexOf(tabBar(w)) + 1);
        tabBar(w)->removeTab(tvData.id, w);
        appendStyledTab(tvData.id, newBar, w);
    }
}

void Sidebar::updateLastSize()
{
    if (isCollapsed()) {
        return;
    }

    QList<int> s = m_splitter->sizes();

    m_lastSize = qMax(s[m_ownSplitIndex], 160);
}

void Sidebar::startRestoreSession(KConfigGroup &config)
{
    if (m_sessionRestoreRunning) {
        return;
    }
    m_sessionRestoreRunning = true;

    char key[256]{};
    snprintf(key, sizeof(key), "Corbomite-MDI-Sidebar-%d-Splitter", (int)position());
    QList<int> s = config.readEntry(key, QList<int>());
    for (int i = 1; i < s.size(); ++i) {
        insertTabBar();
    }
    for (int i = 0; i < s.size(); ++i) {
        snprintf(key, sizeof(key), "Corbomite-MDI-Sidebar-%d-Bar-%d-TvList", (int)position(), i);
        const QStringList tvList = config.readEntry(key, QStringList());
        for (int j = 0; j < tvList.size(); ++j) {
            auto it = std::find_if(m_tvIdToTabId.begin(), m_tvIdToTabId.end(), [&tvList, j](const ToolViewToTabId &a) {
                return a.toolview == tvList.at(j);
            });
            if (it == m_tvIdToTabId.end()) {
                int id = tabBar(i)->addBlankTab();
                m_tvIdToTabId.emplace_back(ToolViewToTabId{.toolview = tvList[j], .tabId = id, .tabbarId = i});
            }
        }
    }
}

void Sidebar::restoreSession(KConfigGroup &config)
{
    if (!m_sessionRestoreRunning) {
        return;
    }

    for (const auto &[id, tv, tabbar] : m_toolviews) {
        tabbar->setTabActive(id, config.readEntry(QStringLiteral("Corbomite-MDI-ToolView-%1-Visible").arg(tv->id), false));
        showToolviewTab(tv, config.readEntry(QStringLiteral("Corbomite-MDI-ToolView-%1-Show-Button-In-Sidebar").arg(tv->id), true));
    }

    for (const auto &[tv, id, tabbarId] : m_tvIdToTabId) {
        tabBar(tabbarId)->removeBlankTab(id);
    }
    m_tvIdToTabId.clear();

    for (int i = 0; i < tabBarCount(); ++i) {
        if (tabBarIsEmpty(tabBar(i))) {
            --i;
        }
    }

    char key[256]{};
    snprintf(key, sizeof(key), "Corbomite-MDI-Sidebar-%d-SectSizes", (int)position());
    QList<int> sectSizes = config.readEntry(key, QList<int>());
    for (int i = 0; i < sectSizes.count(); ++i) {
        if (tabBarCount() - 1 < i) {
            break;
        }
        tabBar(i)->setSectionSize(sectSizes.at(i));
    }

    collapseSidebar();
    snprintf(key, sizeof(key), "Corbomite-MDI-Sidebar-%d-LastSize", (int)position());
    m_lastSize = config.readEntry(key, 160);
    snprintf(key, sizeof(key), "Corbomite-MDI-Sidebar-%d-Splitter", (int)position());
    auto sz = config.readEntry(key, QList<int>());
    QTimer::singleShot(100, this, [this, sz]() {
        setSizes(sz);

        m_mainWin->triggerFocusForCentralWidget();
    });
    updateSidebar();

    m_sessionRestoreRunning = false;
}

void Sidebar::saveSession(KConfigGroup &config)
{
    if (m_sessionRestoreRunning) {
        return;
    }

    char key[256]{};
    snprintf(key, sizeof(key), "Corbomite-MDI-Sidebar-%d-Splitter", (int)position());
    config.writeEntry(key, sizes());

    snprintf(key, sizeof(key), "Corbomite-MDI-Sidebar-%d-LastSize", (int)position());
    config.writeEntry(key, m_lastSize);

    for (const auto &[id, tv, _] : m_toolviews) {
        config.writeEntry(QStringLiteral("Corbomite-MDI-ToolView-%1-Position").arg(tv->id), int(tv->sidebar()->position()));
        config.writeEntry(QStringLiteral("Corbomite-MDI-ToolView-%1-Visible").arg(tv->id), tv->toolVisible());
        config.writeEntry(QStringLiteral("Corbomite-MDI-ToolView-%1-Show-Button-In-Sidebar").arg(tv->id), tv->tabButtonVisible());
    }

    QList<int> sectSizes;
    for (int i = 0; i < tabBarCount(); ++i) {
        sectSizes << tabBar(i)->sectionSize();

        QStringList tvList;
        for (int j : tabBar(i)->tabList()) {
            tvList << dataForId(j).toolview->id;
        }
        snprintf(key, sizeof(key), "Corbomite-MDI-Sidebar-%d-Bar-%d-TvList", (int)position(), i);
        config.writeEntry(key, tvList);
    }

    snprintf(key, sizeof(key), "Corbomite-MDI-Sidebar-%d-SectSizes", (int)position());
    config.writeEntry(key, sectSizes);
}

// END SIDEBAR

// BEGIN MAIN WINDOW

MainWindow::MainWindow(QWidget *parent)
    : KXmlGuiWindow(parent, Qt::Window)
    , m_guiClient(new GUIClient(this))
{
    // central frame for all stuff
    auto *hb = new QFrame(this);
    hb->setObjectName("CorbomiteCentralWidget");
    setCentralWidget(hb);

    // top level vbox for all stuff + bottom bar
    auto *toplevelVBox = new QVBoxLayout(hb);
    toplevelVBox->setContentsMargins(0, 0, 0, 0);
    toplevelVBox->setSpacing(0);

    // hbox for all splitters and other side bars
    m_mainHLayout = new QHBoxLayout;
    m_mainHLayout->setContentsMargins(0, 0, 0, 0);
    m_mainHLayout->setSpacing(0);
    toplevelVBox->addLayout(m_mainHLayout);

    m_hSplitter = new QSplitter(Qt::Horizontal, hb);
    m_sidebars[KMultiTabBar::Left] = std::make_unique<Sidebar>(KMultiTabBar::Left, m_hSplitter, this, hb);
    m_mainHLayout->addWidget(m_sidebars[KMultiTabBar::Left].get());
    m_mainHLayout->addWidget(m_hSplitter);

    auto *vb = new QFrame(m_hSplitter);
    auto *vlayout = new QVBoxLayout(vb);
    vlayout->setContentsMargins(0, 0, 0, 0);
    vlayout->setSpacing(0);

    m_hSplitter->setCollapsible(m_hSplitter->indexOf(vb), false);
    m_hSplitter->setStretchFactor(m_hSplitter->indexOf(vb), 1);

    m_vSplitter = new QSplitter(Qt::Vertical, vb);
    m_sidebars[KMultiTabBar::Top] = std::make_unique<Sidebar>(KMultiTabBar::Top, m_vSplitter, this, vb);
    vlayout->addWidget(m_sidebars[KMultiTabBar::Top].get());
    vlayout->addWidget(m_vSplitter);

    m_centralWidget = new QWidget(m_vSplitter);
    m_centralWidget->setLayout(new QVBoxLayout);
    m_centralWidget->layout()->setSpacing(0);
    m_centralWidget->layout()->setContentsMargins(0, 0, 0, 0);

    m_vSplitter->setCollapsible(m_vSplitter->indexOf(m_centralWidget), false);
    m_vSplitter->setStretchFactor(m_vSplitter->indexOf(m_centralWidget), 1);

    m_sidebars[KMultiTabBar::Right] = std::make_unique<Sidebar>(KMultiTabBar::Right, m_hSplitter, this, hb);
    m_mainHLayout->addWidget(m_sidebars[KMultiTabBar::Right].get());

    auto separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);
    separator->setEnabled(false);
    toplevelVBox->addWidget(separator);

    // bottom side bar spans full windows, include status bar, too
    m_sidebars[KMultiTabBar::Bottom] = std::make_unique<Sidebar>(KMultiTabBar::Bottom, m_vSplitter, this, vb);
    m_bottomSidebarLayout = new QHBoxLayout;
    m_bottomSidebarLayout->addWidget(m_sidebars[KMultiTabBar::Bottom].get());
    m_statusBarStackedWidget = new QStackedWidget(this);
    m_statusBarStackedWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_bottomSidebarLayout->addWidget(m_statusBarStackedWidget);
    m_bottomSidebarLayout->setStretch(0, 100);
    toplevelVBox->addLayout(m_bottomSidebarLayout);

    // ensure proper toolview style
    setToolViewStyle(KMultiTabBar::KDEV3ICON);

    for (const auto &sidebar : m_sidebars) {
        connect(sidebar.get(), &Sidebar::sigShowPluginConfigPage, this, &MainWindow::sigShowPluginConfigPage);

        // Drag/Drop
        connect(sidebar.get(), &Sidebar::dragStarted, this, [this]() {
            for (const auto &sb : m_sidebars) {
                m_dragState.m_wasVisible[(int)sb->position()] = sb->isVisible();
                if (!sb->isVisible()) {
                    sb->setMinimumSize({50, 50});
                    sb->QSplitter::setVisible(true);
                }
            }
        });
        connect(sidebar.get(), &Sidebar::dragEnded, this, [this]() {
            for (const auto &sb : m_sidebars) {
                bool wasVisible = m_dragState.m_wasVisible[(int)sb->position()];
                if (!wasVisible) {
                    sb->setMinimumSize({0, 0});
                    sb->QSplitter::setVisible(false);
                }
            }
        });
    }
}

MainWindow::~MainWindow()
{
    // All toolviews and widgets are parented within the splitter hierarchy,
    // which is parented to this MainWindow. Qt's parent-child ownership
    // handles deletion automatically. Explicit deletion here causes
    // double-free because the widgets are destroyed twice: once explicitly,
    // then again by Qt's child cleanup in ~QObject.
    //
    // Just clear the tracking vector so toolViewDeleted() is a no-op
    // during child destruction.
    m_toolviews.clear();
}

QWidget *MainWindow::centralWidget() const
{
    return m_centralWidget;
}

void MainWindow::insertWidgetBeforeStatusbar(QWidget *widget)
{
    Q_ASSERT(m_bottomSidebarLayout);
    const auto idxOfStatusbar = m_bottomSidebarLayout->indexOf(m_statusBarStackedWidget);
    Q_ASSERT(idxOfStatusbar != -1);
    m_bottomSidebarLayout->insertWidget(idxOfStatusbar, widget);
}

void MainWindow::prependToMainHLayout(QWidget *widget)
{
    Q_ASSERT(m_mainHLayout);
    m_mainHLayout->insertWidget(0, widget);
}

void MainWindow::setStatusBarVisible(bool visible)
{
    statusBarStackedWidget()->setVisible(visible);
    const auto idxOfItemAfterBottomSidebar = m_bottomSidebarLayout->indexOf(m_sidebars[KMultiTabBar::Bottom].get()) + 1;
    for (int i = idxOfItemAfterBottomSidebar; i < m_bottomSidebarLayout->count(); ++i) {
        auto item = m_bottomSidebarLayout->itemAt(i);
        if (item && item->widget()) {
            item->widget()->setVisible(visible);
        }
    }
}

ToolView *MainWindow::createToolView(QObject *plugin,
                                     const QString &identifier,
                                     KMultiTabBar::KMultiTabBarPosition pos,
                                     const QIcon &icon,
                                     const QString &text)
{
    // clashing names are not allowed
    if (toolView(identifier)) {
        return nullptr;
    }

    // try the restore config to figure out real pos
    if (m_restoreConfig && m_restoreConfig->hasGroup(m_restoreGroup)) {
        KConfigGroup cg(m_restoreConfig, m_restoreGroup);
        pos = static_cast<KMultiTabBar::KMultiTabBarPosition>(cg.readEntry(QStringLiteral("Corbomite-MDI-ToolView-%1-Position").arg(identifier), int(pos)));
    }

    ToolView *v = m_sidebars[pos]->addToolView(icon, text, identifier, nullptr);
    v->plugin = plugin;

    Q_ASSERT(toolView(identifier) == nullptr);
    m_toolviews.push_back({identifier, v});

    // register for menu stuff
    m_guiClient->registerToolView(v);

    return v;
}

ToolView *MainWindow::toolView(const QString &identifier) const
{
    for (const auto &[name, toolview] : m_toolviews) {
        if (name == identifier) {
            return toolview;
        }
    }
    return nullptr;
}

void MainWindow::toolViewDeleted(ToolView *widget)
{
    if (!widget) {
        return;
    }

    if (widget->mainWindow() != this) {
        return;
    }

    // During destruction, m_toolviews is already cleared — skip cleanup
    auto it = std::find_if(m_toolviews.begin(), m_toolviews.end(),
                           [widget](const ToolViewWithId &p) { return p.toolview == widget; });
    if (it == m_toolviews.end()) {
        return;
    }

    m_guiClient->unregisterToolView(widget);

    widget->sidebar()->removeToolView(widget);

    m_toolviews.erase(it);
}

void MainWindow::setSidebarsVisibleInternal(bool visible, bool hideFullySilent)
{
    bool old_visible = m_sidebarsVisible;
    m_sidebarsVisible = visible;

    for (auto &sidebar : m_sidebars) {
        sidebar->setVisible(visible);

        if (hideFullySilent) {
            sidebar->collapseSidebar();
        }
    }

    m_guiClient->updateSidebarsVisibleAction();

    // show information message box, if the users hides the sidebars
    if (!hideFullySilent && old_visible && (!m_sidebarsVisible)) {
        KMessageBox::information(this,
                                 i18n("<qt>You are about to hide the sidebars. With "
                                      "hidden sidebars it is not possible to directly "
                                      "access the tool views with the mouse anymore, "
                                      "so if you need to access the sidebars again "
                                      "invoke <b>View &gt; Tool Views &gt; Show Sidebars</b> "
                                      "in the menu. It is still possible to show/hide "
                                      "the tool views with the assigned shortcuts.</qt>"),
                                 QString(),
                                 QStringLiteral("Corbomite hide sidebars notification message"));
    }
}

ToolView *MainWindow::activeViewToolView(KMultiTabBar::KMultiTabBarPosition pos)
{
    const auto side = static_cast<size_t>(pos);
    if (side >= 4) {
        return nullptr;
    }
    return m_sidebars[side]->firstVisibleToolView();
}

bool MainWindow::sidebarsVisible() const
{
    return m_sidebarsVisible;
}

void MainWindow::setToolViewStyle(KMultiTabBar::KMultiTabBarStyle style)
{
    for (auto &sidebar : m_sidebars) {
        sidebar->setStyle(style);
    }
}

KMultiTabBar::KMultiTabBarStyle MainWindow::toolViewStyle() const
{
    return m_sidebars[KMultiTabBar::Top]->tabStyle();
}

bool MainWindow::moveToolView(ToolView *widget, KMultiTabBar::KMultiTabBarPosition pos, bool isDND)
{
    if (!widget || widget->mainWindow() != this) {
        return false;
    }

    // try the restore config to figure out real pos
    if (m_restoreConfig && m_restoreConfig->hasGroup(m_restoreGroup)) {
        KConfigGroup cg(m_restoreConfig, m_restoreGroup);
        pos = static_cast<KMultiTabBar::KMultiTabBarPosition>(cg.readEntry(QStringLiteral("Corbomite-MDI-ToolView-%1-Position").arg(widget->id), int(pos)));
    }

    if (isDND) {
        m_dragState.m_wasVisible[pos] = true;

        auto source = widget->sidebar();
        if (source->toolviewCount() == 1) {
            m_dragState.m_wasVisible[source->position()] = false;
        }
    }

    m_sidebars[pos]->addToolView(widget->icon, widget->text, widget->id, widget);

    if (isDND) {
        m_sidebars[pos]->setMinimumSize({0, 0});
    }

    Q_EMIT toolViewMoved(widget, static_cast<CorbomiteMDI::ToolViewPosition>(pos));

    return true;
}

bool MainWindow::showToolView(ToolView *widget)
{
    if (!widget || widget->mainWindow() != this) {
        return false;
    }

    if (m_restoreConfig && m_restoreConfig->hasGroup(m_restoreGroup)) {
        return true;
    }

    return widget->sidebar()->showToolView(widget);
}

bool MainWindow::hideToolView(ToolView *widget)
{
    if (!widget || widget->mainWindow() != this) {
        return false;
    }

    if (m_restoreConfig && m_restoreConfig->hasGroup(m_restoreGroup)) {
        return true;
    }

    const bool ret = widget->sidebar()->hideToolView(widget);
    triggerFocusForCentralWidget();
    return ret;
}

void MainWindow::hideToolViews()
{
    for (const auto &tv : m_toolviews) {
        tv.toolview->sidebar()->hideToolView(tv.toolview);
    }
    triggerFocusForCentralWidget();
}

CorbomiteMDI::ToolViewPosition MainWindow::toolViewPosition(QWidget *toolview)
{
    if (auto tv = qobject_cast<ToolView *>(toolview)) {
        return static_cast<CorbomiteMDI::ToolViewPosition>(tv->sidebar()->position());
    }
    qWarning("CorbomiteMDI: Invalid, not a toolview");
    return CorbomiteMDI::Bottom;
}

void MainWindow::startRestore(KConfigBase *config, const QString &group)
{
    m_restoreConfig = config;
    m_restoreGroup = group;

    if (!m_restoreConfig || !m_restoreConfig->hasGroup(m_restoreGroup)) {
        m_restoreConfig = nullptr;
        m_restoreGroup.clear();
        return;
    }

    KConfigGroup cg(m_restoreConfig, m_restoreGroup);
    KWindowConfig::restoreWindowSize(windowHandle(), cg);

    // restore the sidebars
    for (auto &sidebar : std::as_const(m_sidebars)) {
        sidebar->startRestoreSession(cg);
    }

    setToolViewStyle(static_cast<KMultiTabBar::KMultiTabBarStyle>(cg.readEntry("Corbomite-MDI-Sidebar-Style", static_cast<int>(toolViewStyle()))));
    m_sidebarsVisible = cg.readEntry("Corbomite-MDI-Sidebar-Visible", true);
    m_guiClient->updateSidebarsVisibleAction();
}

void MainWindow::finishRestore()
{
    if (!m_restoreConfig) {
        return;
    }

    if (m_restoreConfig->hasGroup(m_restoreGroup)) {
        KConfigGroup cg(m_restoreConfig, m_restoreGroup);
        applyMainWindowSettings(cg);

        for (const auto &[id, tv] : m_toolviews) {
            KMultiTabBar::KMultiTabBarPosition newPos = static_cast<KMultiTabBar::KMultiTabBarPosition>(
                cg.readEntry(QStringLiteral("Corbomite-MDI-ToolView-%1-Position").arg(id), int(tv->sidebar()->position())));

            if (tv->sidebar()->position() != newPos) {
                moveToolView(tv, newPos);
            }
        }

        for (auto &sidebar : m_sidebars) {
            sidebar->restoreSession(cg);
        }

        m_hSplitter->setSizes(cg.readEntry("Corbomite-MDI-H-Splitter", QList<int>{200, 100, 200}));
        m_vSplitter->setSizes(cg.readEntry("Corbomite-MDI-V-Splitter", QList<int>{150, 100, 200}));

        QTimer::singleShot(400, this, [this]() {
            QPointer<QWidget> oldFocusWidget(QApplication::focusWidget());

            for (auto &sidebar : m_sidebars) {
                sidebar->updateSidebar();
            }

            if (oldFocusWidget) {
                oldFocusWidget->setFocus();
            } else {
                triggerFocusForCentralWidget();
            }
        });
    }

    m_restoreConfig = nullptr;
    m_restoreGroup.clear();
}

void MainWindow::saveSession(KConfigGroup &config)
{
    saveMainWindowSettings(config);

    // save main splitter sizes
    config.writeEntry("Corbomite-MDI-H-Splitter", m_hSplitter->sizes());
    config.writeEntry("Corbomite-MDI-V-Splitter", m_vSplitter->sizes());

    // save sidebar style
    config.writeEntry("Corbomite-MDI-Sidebar-Style", static_cast<int>(toolViewStyle()));
    config.writeEntry("Corbomite-MDI-Sidebar-Visible", m_sidebarsVisible);

    // save the sidebars
    for (auto &sidebar : m_sidebars) {
        sidebar->saveSession(config);
    }
}

QWidget *MainWindow::createContainer(QWidget *parent, int index, const QDomElement &element, QAction *&containerAction)
{
    QWidget *createdContainer = KXmlGuiWindow::createContainer(parent, index, element, containerAction);
    if (element.tagName() == QLatin1String("ToolBar")) {
        KAcceleratorManager::setNoAccel(createdContainer);
    }
    return createdContainer;
}

// END MAIN WINDOW

} // namespace CorbomiteMDI

#include "moc_CorbomiteMDI.cpp"
