// libs/core/include/corbomite/core/WorkspaceLeaf.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QJsonObject>
#include "corbomite/core/LeafHistory.h"

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
    void setId(const QString &id);
    View *view() const;

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

    QJsonObject serialize() const;
    static WorkspaceLeaf *deserialize(const QJsonObject &json,
                                      ViewRegistry *registry,
                                      QWidget *parent);

    static QString generateId();

Q_SIGNALS:
    void viewChanged(View *newView);
    void pinnedChanged(bool pinned);
    void groupChanged(const QString &group);

private:
    void closeCurrentView();

    QString m_id;
    View *m_view = nullptr;
    ViewRegistry *m_registry;

    bool m_pinned = false;
    QString m_group;
    bool m_deferred = false;
    QString m_cachedIcon;
    QString m_cachedTitle;
    LeafHistory m_history;
    qint64 m_activeTime = 0;

    // Saved state for deferred load
    QJsonObject m_deferredViewState;
};

} // namespace Corbomite
