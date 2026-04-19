// libs/core/include/corbomite/core/ItemView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/View.h"

#include <functional>

class QHBoxLayout;
class QLabel;
class QMenu;
class QToolButton;

namespace Corbomite {

class ItemView : public View
{
    Q_OBJECT

public:
    explicit ItemView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    QWidget *contentWidget() const;
    QWidget *headerWidget() const;

    void addAction(const QString &icon, const QString &title,
                   std::function<void()> callback);

    // Populates `menu` with hamburger contributions in the canonical order:
    //   1. View::onMoreOptionsMenu(helper)        — primary subclass hook
    //   2. View::onPaneMenu(menu, "more-options") — back-compat
    //   3. WorkspaceLeaf::menuEventEmitter()->emitLeafMenu — plugin hook
    //   4. helper.finalize()
    // Does not exec() the menu — primarily a test seam; production callers use
    // showMoreOptionsMenu().
    void buildMoreOptionsMenu(QMenu *menu);

protected:
    // Re-export View's helper-based overload so subclass override declarations
    // do not hide the base virtual.
    using View::onMoreOptionsMenu;

    void onOpen() override;

private:
    void buildHeader();
    void showMoreOptionsMenu();
    void updateNavigationButtons();

    QWidget *m_headerWidget = nullptr;
    QWidget *m_contentWidget = nullptr;
    QHBoxLayout *m_actionsLayout = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_iconLabel = nullptr;
    QToolButton *m_backButton = nullptr;
    QToolButton *m_forwardButton = nullptr;
};

} // namespace Corbomite
