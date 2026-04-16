// libs/core/include/corbomite/core/ItemView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/View.h"

#include <functional>

class QHBoxLayout;
class QLabel;
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

protected:
    virtual void onMoreOptionsMenu(QMenu *menu);
    void onOpen() override;

private:
    void buildHeader();
    void showMoreOptionsMenu();

    QWidget *m_headerWidget = nullptr;
    QWidget *m_contentWidget = nullptr;
    QHBoxLayout *m_actionsLayout = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_iconLabel = nullptr;
};

} // namespace Corbomite
