/*
    SPDX-FileCopyrightText: 2005 Christoph Cullmann <cullmann@kde.org>
    SPDX-FileCopyrightText: 2002, 2003 Joseph Wenninger <jowenn@kde.org>
    SPDX-FileCopyrightText: 2026 Corbomite Contributors

    Adapted from Kate's KateMDI framework for use in Corbomite.

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <KXmlGuiWindow>

#include <KMultiTabBar>
#include <KXMLGUIClient>

#include <QFrame>
#include <QPointer>
#include <QSplitter>

#include <vector>

class KActionMenu;
class QAction;
class QLabel;
class QPixmap;
class QStackedWidget;
class KConfigBase;
class QHBoxLayout;
class QRubberBand;
class QEvent;
class QChildEvent;
class KToggleAction;

namespace CorbomiteMDI
{

/**
 * Tool view position enum, replacing KTextEditor::MainWindow::ToolViewPosition
 */
enum ToolViewPosition { Left = 0, Right = 1, Top = 2, Bottom = 3 };

class ToolView;

class GUIClient : public QObject, public KXMLGUIClient
{
public:
    explicit GUIClient(class MainWindow *mw);

    void registerToolView(ToolView *tv);
    void unregisterToolView(ToolView *tv);
    void updateSidebarsVisibleAction();

private:
    void clientAdded(KXMLGUIClient *client);
    void updateActions();

private:
    MainWindow *m_mw;
    KToggleAction *m_showSidebarsAction;
    struct ToolViewActions {
        ToolView *toolview = nullptr;
        std::vector<QAction *> actions;
    };
    std::vector<ToolViewActions> m_toolToActions;
    KActionMenu *m_toolMenu;
    QAction *m_hideToolViews;
    KActionMenu *m_sidebarButtonsMenu;
    KActionMenu *m_focusToolviewMenu;
};

class ToolView : public QFrame
{
    Q_OBJECT

    friend class Sidebar;
    friend class MultiTabBar;
    friend class MainWindow;
    friend class GUIClient;

protected:
    /**
     * ToolView
     * Objects of this class represent a toolview in the mainwindow.
     * You should only add one widget as child to this toolview, it will
     * be automatically set to be the focus proxy of the toolview.
     */
    ToolView(class MainWindow *mainwin, class Sidebar *sidebar, QWidget *parent, const QString &identifier);

public:
    ~ToolView() override;

    MainWindow *mainWindow()
    {
        return m_mainWin;
    }

Q_SIGNALS:
    void toolVisibleChanged(bool visible);
    void tabButtonVisibleChanged(bool visible);

protected:
    Sidebar *sidebar()
    {
        return m_sidebar;
    }

    void setToolVisible(bool vis);

public:
    bool toolVisible() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    bool tabButtonVisible() const;
    void setTabButtonVisible(bool visible);

protected:
    void childEvent(QChildEvent *ev) override;
    void actionEvent(QActionEvent *event) override;

private:
    MainWindow *m_mainWin;
    Sidebar *m_sidebar;
    KToolBar *m_toolbar;

    /// plugin this view belongs to, may be null
    QPointer<QObject> plugin;

    /// unique id
    const QString id;

    /// is visible in sidebar
    bool m_toolVisible;

    /// Is the button visible in sidebar
    bool isTabButtonVisible = true;

    QIcon icon;
    QString text;
};

class MultiTabBar : public QWidget
{
    Q_OBJECT

public:
    MultiTabBar(KMultiTabBar::KMultiTabBarPosition pos, Sidebar *sb, int idx);
    ~MultiTabBar();

    KMultiTabBarTab *addTab(int id, ToolView *tv);
    int addBlankTab();
    void removeBlankTab(int id);
    void removeTab(int id, ToolView *tv);
    void reorderTab(int id, KMultiTabBarTab *before);

    void showToolView(int id);
    void hideToolView(int id);

    void setTabActive(int id, bool state);

    bool isToolActive() const;
    void collapseToolView() const;
    bool expandToolView() const;

    KMultiTabBar *tabBar() const
    {
        return m_multiTabBar;
    }

    const std::vector<int> &tabList() const
    {
        return m_tabList;
    }

    int tabCount() const
    {
        return (int)m_tabList.size();
    }

    int sectionSize() const
    {
        return m_sectionSize;
    }

    void setSectionSize(int size)
    {
        m_sectionSize = size;
    }

    int activeTab() const
    {
        return m_activeTab;
    }

Q_SIGNALS:
    void lastTabRemoved(MultiTabBar *);

private:
    void tabClicked(int);

private:
    Sidebar *m_sb;
    QStackedWidget *m_stack;
    KMultiTabBar *m_multiTabBar;
    std::vector<int> m_tabList;
    int m_activeTab = 0;
    int m_sectionSize = 0;
};

class Sidebar : public QSplitter
{
    Q_OBJECT

    friend class MultiTabBar;

public:
    Sidebar(KMultiTabBar::KMultiTabBarPosition pos, QSplitter *sp, class MainWindow *mainwin, QWidget *parent);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    ToolView *addToolView(const QIcon &icon, const QString &text, const QString &identifier, ToolView *widget);
    bool removeToolView(ToolView *widget);

    bool showToolView(ToolView *widget);
    bool hideToolView(ToolView *widget);

    void showToolviewTab(ToolView *widget, bool show);

    bool isCollapsed();

    QWidget *tabButtonForToolview(ToolView *widget) const
    {
        for (CorbomiteMDI::Sidebar::ToolViewData d : m_toolviews) {
            if (d.toolview == widget) {
                return d.tabbar->tabBar()->tab(d.id);
            }
        }
        return nullptr;
    }

    int toolviewCount() const
    {
        return (int)m_toolviews.size();
    }

    ToolView *firstVisibleToolView();

    void updateSidebar();
    void collapseSidebar();

    KMultiTabBar::KMultiTabBarPosition position() const
    {
        return m_tabBarPosition;
    }

    bool isVertical() const
    {
        return m_tabBarPosition == KMultiTabBar::Right || m_tabBarPosition == KMultiTabBar::Left;
    }

    void setStyle(KMultiTabBar::KMultiTabBarStyle style);

    KMultiTabBar::KMultiTabBarStyle tabStyle() const
    {
        return m_tabBarStyle;
    }

    void startRestoreSession(KConfigGroup &config);
    void restoreSession(KConfigGroup &config);
    void saveSession(KConfigGroup &config);

public Q_SLOTS:
    void setVisible(bool visible) override;

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dropEvent(QDropEvent *) override;

private:
    void buttonPopupActivate(QAction *);
    void readConfig();
    void handleCollapse(int pos, int index);
    void ownSplitMoved(int pos, int index);
    void barSplitMoved(int pos, int index);
    bool tabBarIsEmpty(MultiTabBar *bar);

private:
    void updateLastSize();
    int nextId();
    bool adjustSplitterSections();

    void appendStyledTab(int id, MultiTabBar *bar, ToolView *widget);
    void updateButtonStyle(KMultiTabBarTab *button);
    void updateLastSizeOnResize();

    MultiTabBar *insertTabBar(int idx = -1);

    MultiTabBar *tabBar(int idx) const
    {
        return static_cast<MultiTabBar *>(widget(idx));
    }

    MultiTabBar *tabBar(ToolView *tv) const
    {
        for (CorbomiteMDI::Sidebar::ToolViewData d : m_toolviews) {
            if (d.toolview == tv)
                return d.tabbar;
        }
        return nullptr;
    }

    KMultiTabBar *kmTabBar(ToolView *widget) const
    {
        if (MultiTabBar *tabbar = tabBar(widget)) {
            return tabbar->tabBar();
        }
        return nullptr;
    }

    int tabBarCount() const
    {
        return count();
    }

private:
    enum ActionIds {
        HideButtonAction = 11,
        ConfigureAction = 20,
        ToOwnSectAction = 30,
        UpLeftAction = 31,
        DownRightAction = 32,
    };

    MainWindow *m_mainWin;

    KMultiTabBar::KMultiTabBarPosition m_tabBarPosition{};
    KMultiTabBar::KMultiTabBarStyle m_tabBarStyle{};
    QSplitter *m_splitter;
    QSplitter *m_ownSplit;
    const int m_ownSplitIndex;

    struct ToolViewData {
        int id = -1;
        ToolView *toolview = nullptr;
        MultiTabBar *tabbar = nullptr;
    };
    std::vector<ToolViewData> m_toolviews;

    ToolViewData dataForId(int id)
    {
        for (const CorbomiteMDI::Sidebar::ToolViewData &d : m_toolviews) {
            if (d.id == id)
                return d;
        }
        return {};
    }

    // Session restore only
    struct ToolViewToTabId {
        QString toolview;
        int tabId{};
        int tabbarId{};
    };
    std::vector<ToolViewToTabId> m_tvIdToTabId;
    bool m_sessionRestoreRunning = false;

    int m_lastSize;
    int m_popupButton = 0;
    QPointer<QLabel> m_resizePlaceholder;
    bool m_isPreviouslyCollapsed = false;
    bool m_syncWithTabs = false;
    bool m_showTextForLeftRight = false;
    int m_leftRightSidebarIconSize = 32;
    QPoint dragStartPos;
    QRubberBand *m_dropIndicator;
    QRubberBand *m_internalDropIndicator;

Q_SIGNALS:
    void sigShowPluginConfigPage(QObject *plugin, int id);
    void dragStarted();
    void dragEnded();
};

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

    friend class ToolView;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /**
     * Create a new tool view in the given sidebar position.
     * @param plugin optional plugin object (may be nullptr)
     * @param identifier unique identifier for this toolview
     * @param pos position for the toolview
     * @param icon icon to use for the toolview
     * @param text text to use in addition to icon
     * @return created toolview on success or nullptr
     */
    ToolView *
    createToolView(QObject *plugin, const QString &identifier, KMultiTabBar::KMultiTabBarPosition pos, const QIcon &icon, const QString &text);

    ToolView *toolView(const QString &identifier) const;

    bool sidebarsVisible() const;

    void setSidebarsVisibleInternal(bool visible, bool hideFullySilent);

    ToolView *activeViewToolView(KMultiTabBar::KMultiTabBarPosition pos);

public Q_SLOTS:
    void setSidebarsVisible(bool visible)
    {
        setSidebarsVisibleInternal(visible, false);
    }

    void hideToolViews();

    CorbomiteMDI::ToolViewPosition toolViewPosition(QWidget *toolview);

protected:
    void toolViewDeleted(ToolView *widget);

    void setToolViewStyle(KMultiTabBar::KMultiTabBarStyle style);
    KMultiTabBar::KMultiTabBarStyle toolViewStyle() const;

    // Insert a widget at the outermost-left position of the main horizontal
    // layout — before the left KMultiTabBar. Used by subclasses to mount
    // Obsidian-style ribbon slots.
    void prependToMainHLayout(QWidget *widget);

public:
    QWidget *centralWidget() const;

    virtual void triggerFocusForCentralWidget()
    {
    }

protected:
    QStackedWidget *statusBarStackedWidget() const
    {
        return m_statusBarStackedWidget;
    }

    void insertWidgetBeforeStatusbar(QWidget *widget);

    QWidget *createContainer(QWidget *parent, int index, const QDomElement &element, QAction *&containerAction) override;

    void setStatusBarVisible(bool visible);

public:
    bool moveToolView(ToolView *widget, KMultiTabBar::KMultiTabBarPosition pos, bool isDND = false);
    bool showToolView(ToolView *widget);
    bool hideToolView(ToolView *widget);

    std::vector<QString> toolviewNames() const
    {
        std::vector<QString> out;
        out.reserve(m_toolviews.size());
        for (const auto &tv : m_toolviews) {
            out.emplace_back(tv.toolview->id);
        }
        return out;
    }

    void startRestore(KConfigBase *config, const QString &group);
    void finishRestore();
    void saveSession(KConfigGroup &group);

private:
    struct ToolViewWithId {
        QString id;
        ToolView *toolview = nullptr;
    };
    std::vector<ToolViewWithId> m_toolviews;

    QWidget *m_centralWidget;
    QSplitter *m_hSplitter;
    QSplitter *m_vSplitter;

    std::unique_ptr<Sidebar> m_sidebars[4];

    bool m_sidebarsVisible = true;

    KConfigBase *m_restoreConfig = nullptr;
    QString m_restoreGroup;

    GUIClient *m_guiClient;

    QStackedWidget *m_statusBarStackedWidget;
    QHBoxLayout *m_mainHLayout = nullptr;
    QHBoxLayout *m_bottomSidebarLayout = nullptr;

    struct DragState {
        bool m_wasVisible[4] = {};
    } m_dragState;

Q_SIGNALS:
    void sigShowPluginConfigPage(QObject *plugin, int id);
    void tabForToolViewAdded(QWidget *toolView, QWidget *tab);
    void toolViewMoved(QWidget *toolView, CorbomiteMDI::ToolViewPosition newPos);
};

} // namespace CorbomiteMDI
