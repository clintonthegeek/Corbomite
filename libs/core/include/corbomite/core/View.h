// libs/core/include/corbomite/core/View.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QJsonObject>
#include <functional>
#include <memory>

class QMenu;

namespace Corbomite { class MenuSectionHelper; }

namespace Corbomite {

class Component;
class WorkspaceLeaf;

class View : public QWidget
{
    Q_OBJECT

public:
    explicit View(WorkspaceLeaf *leaf, QWidget *parent = nullptr);
    ~View() override;

    Component *component() const;
    void registerQObjectConnection(const QMetaObject::Connection &conn);
    void addChild(Component *child);
    int registerInterval(int ms, std::function<void()> fn);

    virtual QString getViewType() const = 0;
    virtual QString getDisplayText() const = 0;
    virtual QString getIcon() const;

    void open(QWidget *parent);
    void close();

    virtual QJsonObject getState() const;
    virtual void setState(const QJsonObject &state);
    virtual QJsonObject getEphemeralState() const;
    virtual void setEphemeralState(const QJsonObject &state);

    virtual void onPaneMenu(QMenu *menu);

    // Primary hook for hamburger-menu ("…" / overflow) contributions.
    // Subclasses override to add items via the supplied helper. Called by
    // ItemView::showMoreOptionsMenu before onPaneMenu(menu, "more-options").
    virtual void onMoreOptionsMenu(MenuSectionHelper &helper);

    // Context-menu hook with source discrimination. `source` distinguishes
    // invocation contexts — "pane-menu" (tab-header right-click), "more-options"
    // (hamburger click), "file-menu" (file-list right-click), etc.
    // Default implementation forwards to the zero-arg overload for backward compat.
    virtual void onPaneMenu(QMenu *menu, const QString &source);

    virtual void onTabMenu(QMenu *menu);
    virtual void onResize();

    virtual void zoomIn();
    virtual void zoomOut();
    virtual void zoomReset();

    QWidget *containerWidget() const;
    WorkspaceLeaf *leaf() const;

Q_SIGNALS:
    void displayTextChanged();

protected:
    virtual void onOpen();
    virtual void onClose();

    WorkspaceLeaf *m_leaf;

private:
    std::unique_ptr<Component> m_component;
    QWidget *m_containerWidget;
    bool m_opened = false;
};

} // namespace Corbomite
