// libs/core/include/corbomite/core/WorkspaceLeaf.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QJsonObject>

namespace Corbomite {

class View;
class ViewRegistry;

class WorkspaceLeaf : public QWidget
{
    Q_OBJECT

public:
    explicit WorkspaceLeaf(ViewRegistry *registry, QWidget *parent = nullptr);
    ~WorkspaceLeaf() override;

    QString id() const;
    View *view() const;

    void open(View *newView);

    QJsonObject getViewState() const;
    void setViewState(const QJsonObject &state);

    QJsonObject getEphemeralState() const;
    void setEphemeralState(const QJsonObject &state);

    QJsonObject serialize() const;
    static WorkspaceLeaf *deserialize(const QJsonObject &json,
                                      ViewRegistry *registry,
                                      QWidget *parent);

    static QString generateId();

Q_SIGNALS:
    void viewChanged(View *newView);

private:
    void closeCurrentView();

    QString m_id;
    View *m_view = nullptr;
    ViewRegistry *m_registry;
};

} // namespace Corbomite
