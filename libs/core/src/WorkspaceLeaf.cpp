// libs/core/src/WorkspaceLeaf.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"

#include <QDateTime>
#include <QVBoxLayout>

namespace Corbomite {

WorkspaceLeaf::WorkspaceLeaf(ViewRegistry *registry, QObject *parent)
    : WorkspaceItem(parent)
    , m_widget(new QWidget)
    , m_registry(registry)
{
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}

WorkspaceLeaf::~WorkspaceLeaf()
{
    closeCurrentView();
    delete m_widget;
}

QWidget *WorkspaceLeaf::widget() { return m_widget; }

View *WorkspaceLeaf::view() const { return m_view; }

ViewRegistry *WorkspaceLeaf::registry() const { return m_registry; }

void WorkspaceLeaf::open(View *newView)
{
    closeCurrentView();
    m_view = newView;
    if (m_view) {
        m_view->open(m_widget);
        m_widget->layout()->addWidget(m_view);
    }
    Q_EMIT viewChanged(m_view);
}

void WorkspaceLeaf::closeCurrentView()
{
    if (m_view) {
        m_view->close();
        if (m_widget && m_widget->layout())
            m_widget->layout()->removeWidget(m_view);
        m_view->deleteLater();
        m_view = nullptr;
    }
}

QJsonObject WorkspaceLeaf::getViewState() const
{
    QJsonObject state;
    if (m_view) {
        state[QStringLiteral("type")] = m_view->getViewType();
        state[QStringLiteral("state")] = m_view->getState();
        state[QStringLiteral("icon")] = m_view->getIcon();
        state[QStringLiteral("title")] = m_view->getDisplayText();
    } else if (m_deferred) {
        // Return cached state for deferred leaf
        state = m_deferredViewState;
    }
    return state;
}

void WorkspaceLeaf::setViewState(const QJsonObject &state)
{
    QString type = state[QStringLiteral("type")].toString();
    if (type.isEmpty() || !m_registry) return;

    auto factory = m_registry->getViewCreatorByType(type);
    if (!factory) {
        // Audit: views.md §1.tD / §1.nD — unresolved view type falls back
        // to the "empty" placeholder if the host registered one. This also
        // covers vaults restored across plugin-set changes (Cluster G
        // follow-up #2: unknown-viewType fallback).
        factory = m_registry->getViewCreatorByType(QStringLiteral("empty"));
        if (!factory) {
            closeCurrentView();
            return;
        }
    }

    auto *newView = factory(this);
    open(newView);

    QJsonObject viewState = state[QStringLiteral("state")].toObject();
    if (!viewState.isEmpty()) {
        m_view->setState(viewState);
        Q_EMIT viewChanged(m_view);
    }
}

QJsonObject WorkspaceLeaf::getEphemeralState() const
{
    return m_view ? m_view->getEphemeralState() : QJsonObject{};
}

void WorkspaceLeaf::setEphemeralState(const QJsonObject &state)
{
    if (m_view)
        m_view->setEphemeralState(state);
}

// --- Pinned ---

bool WorkspaceLeaf::pinned() const { return m_pinned; }

void WorkspaceLeaf::setPinned(bool pinned)
{
    if (m_pinned == pinned)
        return;
    m_pinned = pinned;
    Q_EMIT pinnedChanged(m_pinned);
}

// --- Group ---

QString WorkspaceLeaf::group() const { return m_group; }

void WorkspaceLeaf::setGroup(const QString &group)
{
    if (m_group == group)
        return;
    m_group = group;
    Q_EMIT groupChanged(m_group);
}

// --- Deferred ---

bool WorkspaceLeaf::isDeferred() const { return m_deferred; }

void WorkspaceLeaf::setDeferred(bool deferred, const QString &icon, const QString &title)
{
    m_deferred = deferred;
    if (deferred) {
        if (m_view)
            m_deferredViewState = getViewState();
        m_cachedIcon = icon;
        m_cachedTitle = title;
        closeCurrentView();
    }
}

void WorkspaceLeaf::loadIfDeferred()
{
    if (!m_deferred)
        return;
    m_deferred = false;
    if (!m_deferredViewState.isEmpty())
        setViewState(m_deferredViewState);
}

// --- Cached metadata ---

QString WorkspaceLeaf::cachedIcon() const { return m_cachedIcon; }
QString WorkspaceLeaf::cachedTitle() const { return m_cachedTitle; }

// --- History ---

LeafHistory &WorkspaceLeaf::history() { return m_history; }
const LeafHistory &WorkspaceLeaf::history() const { return m_history; }

// --- Active time ---

qint64 WorkspaceLeaf::activeTime() const { return m_activeTime; }

void WorkspaceLeaf::updateActiveTime()
{
    m_activeTime = QDateTime::currentMSecsSinceEpoch();
}

// --- Navigation ---

void WorkspaceLeaf::navigate(const QJsonObject &viewState)
{
    // Push current state to history before navigating
    if (m_view) {
        LeafHistoryEntry current;
        current.title = m_view->getDisplayText();
        current.icon = m_view->getIcon();
        current.state = m_view->getState();
        current.eState = m_view->getEphemeralState();
        m_history.push(current);
    }

    // If viewState specifies a different view type, switch via setViewState.
    // Otherwise update the existing view's state in-place.
    QString type = viewState[QStringLiteral("type")].toString();
    if (!type.isEmpty() && (!m_view || m_view->getViewType() != type)) {
        setViewState(viewState);
    } else if (m_view) {
        QJsonObject inner = viewState[QStringLiteral("state")].toObject();
        m_view->setState(inner.isEmpty() ? viewState : inner);
        Q_EMIT viewChanged(m_view);
    }
}

void WorkspaceLeaf::goBack()
{
    if (!m_history.canGoBack())
        return;

    LeafHistoryEntry current;
    if (m_view) {
        current.title = m_view->getDisplayText();
        current.icon = m_view->getIcon();
        current.state = m_view->getState();
        current.eState = m_view->getEphemeralState();
    }

    LeafHistoryEntry entry = m_history.goBack(current);
    if (!entry.isValid())
        return;

    if (m_view) {
        m_view->setState(entry.state);
        Q_EMIT viewChanged(m_view);
        if (!entry.eState.isEmpty())
            m_view->setEphemeralState(entry.eState);
    }
}

void WorkspaceLeaf::goForward()
{
    if (!m_history.canGoForward())
        return;

    LeafHistoryEntry current;
    if (m_view) {
        current.title = m_view->getDisplayText();
        current.icon = m_view->getIcon();
        current.state = m_view->getState();
        current.eState = m_view->getEphemeralState();
    }

    LeafHistoryEntry entry = m_history.goForward(current);
    if (!entry.isValid())
        return;

    if (m_view) {
        m_view->setState(entry.state);
        Q_EMIT viewChanged(m_view);
        if (!entry.eState.isEmpty())
            m_view->setEphemeralState(entry.eState);
    }
}

// --- Serialize / Deserialize ---

QJsonObject WorkspaceLeaf::serialize() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id();
    json[QStringLiteral("type")] = QStringLiteral("leaf");
    json[QStringLiteral("state")] = getViewState();

    if (m_pinned)
        json[QStringLiteral("pinned")] = true;
    if (!m_group.isEmpty())
        json[QStringLiteral("group")] = m_group;

    return json;
}

WorkspaceLeaf *WorkspaceLeaf::deserialize(const QJsonObject &json,
                                           ViewRegistry *registry,
                                           QObject *parent)
{
    auto *leaf = new WorkspaceLeaf(registry, parent);
    QString leafId = json[QStringLiteral("id")].toString();
    if (!leafId.isEmpty())
        leaf->setId(leafId);

    if (json[QStringLiteral("pinned")].toBool())
        leaf->m_pinned = true;

    QString group = json[QStringLiteral("group")].toString();
    if (!group.isEmpty())
        leaf->m_group = group;

    QJsonObject viewState = json[QStringLiteral("state")].toObject();
    if (!viewState.isEmpty())
        leaf->setViewState(viewState);

    return leaf;
}

} // namespace Corbomite
