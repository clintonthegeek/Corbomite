// libs/core/include/corbomite/core/WorkspaceLeaf.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include "corbomite/core/LeafHistory.h"
#include "corbomite/core/WorkspaceItem.h"

namespace Corbomite {

class View;
class ViewRegistry;

class WorkspaceLeaf : public WorkspaceItem
{
    Q_OBJECT

public:
    explicit WorkspaceLeaf(ViewRegistry *registry, QObject *parent = nullptr);
    ~WorkspaceLeaf() override;

    QWidget *widget() override;
    QJsonObject serialize() const override;

    View *view() const;
    ViewRegistry *registry() const;

    void open(View *newView);

    QJsonObject getViewState() const;
    void setViewState(const QJsonObject &state);

    QJsonObject getEphemeralState() const;
    void setEphemeralState(const QJsonObject &state);

    // Pinned
    bool pinned() const;
    void setPinned(bool pinned);

    // Group
    QString group() const;
    void setGroup(const QString &group);

    // Deferred loading
    bool isDeferred() const;
    void setDeferred(bool deferred, const QString &icon = {}, const QString &title = {});
    void loadIfDeferred();

    // Cached metadata (used when deferred or before view loads)
    QString cachedIcon() const;
    QString cachedTitle() const;

    // History
    LeafHistory &history();
    const LeafHistory &history() const;

    // Active time (ms since epoch, updated on focus)
    qint64 activeTime() const;
    void updateActiveTime();

    // Navigation
    void navigate(const QJsonObject &viewState);
    void goBack();
    void goForward();

    static WorkspaceLeaf *deserialize(const QJsonObject &json,
                                      ViewRegistry *registry,
                                      QObject *parent = nullptr);

Q_SIGNALS:
    void viewChanged(View *newView);
    void pinnedChanged(bool pinned);
    void groupChanged(const QString &group);

private:
    void closeCurrentView();

    QWidget *m_widget;
    View *m_view = nullptr;
    ViewRegistry *m_registry;

    bool m_pinned = false;
    QString m_group;
    bool m_deferred = false;
    QString m_cachedIcon;
    QString m_cachedTitle;
    LeafHistory m_history;
    qint64 m_activeTime = 0;

    QJsonObject m_deferredViewState;
};

} // namespace Corbomite
