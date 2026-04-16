// libs/core/include/corbomite/core/View.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QJsonObject>
#include <functional>
#include <memory>

class QMenu;

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
    virtual void onTabMenu(QMenu *menu);
    virtual void onResize();

    QWidget *containerWidget() const;
    WorkspaceLeaf *leaf() const;

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
