// libs/core/include/corbomite/core/WorkspaceLeaf.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>

#include "corbomite/core/LeafHistory.h"

namespace KDDockWidgets::QtWidgets {
class DockWidget;
}

namespace Corbomite {

class MenuEventEmitter;
class View;
class ViewRegistry;
class Workspace;

class WorkspaceLeaf : public QObject
{
    Q_OBJECT

public:
    explicit WorkspaceLeaf(ViewRegistry *registry, QObject *parent = nullptr);
    ~WorkspaceLeaf() override;

    QString id() const;
    void setId(const QString &id);
    static QString generateId();

    QWidget *widget();
    QJsonObject serialize() const;

    View *view() const;
    ViewRegistry *registry() const;

    /// The Workspace that owns this leaf. After Phase 4b leaves are
    /// QObject-parented directly to the owning Workspace at construction
    /// (the substrate-walk path is gone), so this is just a parent
    /// qobject_cast. Returns nullptr if the leaf is unattached (e.g. test
    /// fixtures).
    Workspace *workspace() const;

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

    /// Round-tripped verbatim by serialize() so vault-format bumps in
    /// Obsidian don't lose data on Corbomite save. Populated during
    /// fromJson with any leaf-level keys the parser didn't recognize.
    QJsonObject unknownLeafKeys() const;
    void setUnknownLeafKeys(const QJsonObject &keys);

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

    /// Close and destroy the current view, releasing any file references.
    void closeCurrentView();

    /// Non-owning emitter used by ItemView::buildMoreOptionsMenu to fire
    /// `leaf-menu` to plugins. May be null (tests, headless contexts).
    Corbomite::MenuEventEmitter *menuEventEmitter() const { return m_menuEmitter; }
    void setMenuEventEmitter(Corbomite::MenuEventEmitter *e) { m_menuEmitter = e; }

    // Package-private; do not include this header from outside libs/core.
    // Used by Workspace + WorkspaceSerializer to drive the KDDW substrate.
    KDDockWidgets::QtWidgets::DockWidget *dockWidget() const { return m_dockWidget; }
    void setAsCurrentTab();

    /// Workspace calls this in its destructor before tearing down the
    /// owning KDDW MainWindow, to suppress the leaf's `delete m_dockWidget`
    /// in ~WorkspaceLeaf. The MainWindow's destructor disposes of every
    /// docked DockWidget, so leaves left as Qt-children of the Workspace
    /// (e.g. closeLeaf'd ones still pending deleteLater) would otherwise
    /// double-free during ~QObject's child cleanup.
    void releaseDockWidget() { m_dockWidget = nullptr; }

Q_SIGNALS:
    void viewChanged(View *newView);
    void pinnedChanged(bool pinned);
    void groupChanged(const QString &group);
    /// Fires from `setViewState` when `requestedType` has no registered
    /// view factory — e.g. a workspace.json referencing a disabled/removed
    /// plugin's view type. `reason` says whether the leaf fell back to the
    /// "empty" placeholder or, absent even that, closed outright.
    void viewTypeUnresolved(const QString &requestedType, const QString &reason);

private:
    /// Restore a history entry, recreating the view via the registry when the
    /// entry's view type differs from the current view (so back/forward across
    /// view types — e.g. bases ↔ markdown — rebuilds the right view instead of
    /// loading state into the wrong one).
    void restoreFromHistory(const LeafHistoryEntry &entry);

    QString m_id;
    KDDockWidgets::QtWidgets::DockWidget *m_dockWidget = nullptr;
    QPointer<View> m_view;
    ViewRegistry *m_registry;

    bool m_pinned = false;
    QString m_group;
    QJsonObject m_unknownLeafKeys;
    bool m_deferred = false;
    QString m_cachedIcon;
    QString m_cachedTitle;
    LeafHistory m_history;
    qint64 m_activeTime = 0;

    QJsonObject m_deferredViewState;

    Corbomite::MenuEventEmitter *m_menuEmitter = nullptr;  // non-owning
};

} // namespace Corbomite
