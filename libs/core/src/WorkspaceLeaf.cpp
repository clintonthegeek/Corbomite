// libs/core/src/WorkspaceLeaf.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/KDDockWidgets.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>

#include <QDateTime>
#include <QRandomGenerator>

namespace Corbomite {

namespace {
void ensureKddwInit()
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
    auto &cfg = KDDockWidgets::Config::self();
    cfg.setFlags(cfg.flags()
                 | KDDockWidgets::Config::Flag_AlwaysShowTabs
                 | KDDockWidgets::Config::Flag_AllowReorderTabs
                 | KDDockWidgets::Config::Flag_TabsHaveCloseButton);
}
} // namespace

WorkspaceLeaf::WorkspaceLeaf(ViewRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_id(generateId())
    , m_registry(registry)
{
    // KDDW DockWidget construction needs an initialized frontend. Tests
    // sometimes construct a WorkspaceLeaf in isolation (no enclosing
    // Workspace), so don't rely on Workspace's ensureKddwInit having run.
    ensureKddwInit();
    // The unique name is finalized when the leaf is parented to a Workspace
    // (which knows the vaultId): Workspace::createLeafInGroupOf renames via
    // setUniqueName({vaultId}:{leafId}). Plain leaf-id is unique enough for
    // tests that construct a leaf in isolation.
    m_dockWidget = new KDDockWidgets::QtWidgets::DockWidget(m_id);
}

WorkspaceLeaf::~WorkspaceLeaf()
{
    closeCurrentView();
    // The owning Workspace clears m_dockWidget via releaseDockWidget() in its
    // destructor before tearing down the KDDW MainWindow (which would
    // double-free leaves that are still QObject-children of Workspace and
    // thus get cleaned up by ~QObject after KDDW already disposed their
    // dock widgets). For independent leaves (constructed without an
    // enclosing Workspace), m_dockWidget is non-null and we own it.
    delete m_dockWidget;
}

QString WorkspaceLeaf::id() const { return m_id; }

void WorkspaceLeaf::setId(const QString &id) { m_id = id; }

QString WorkspaceLeaf::generateId()
{
    static const char chars[] = "0123456789abcdef";
    QString result;
    result.reserve(16);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 16; ++i)
        result.append(QLatin1Char(chars[rng->bounded(16)]));
    return result;
}

QWidget *WorkspaceLeaf::widget() { return m_dockWidget; }

View *WorkspaceLeaf::view() const { return m_view; }

Workspace *WorkspaceLeaf::workspace() const
{
    return qobject_cast<Workspace *>(parent());
}

ViewRegistry *WorkspaceLeaf::registry() const { return m_registry; }

void WorkspaceLeaf::open(View *newView)
{
    closeCurrentView();
    m_view = newView;
    if (m_view) {
        // View::open(QWidget*) historically took the leaf's content widget
        // as a "host" hint. With KDDW underneath, the View itself is the
        // dock widget's guest, so there's no separate host container.
        m_view->open(nullptr);
        m_dockWidget->setWidget(m_view);
        m_dockWidget->setTitle(m_view->getDisplayText());
        connect(m_view, &View::displayTextChanged, m_dockWidget, [this]() {
            if (m_view && m_dockWidget)
                m_dockWidget->setTitle(m_view->getDisplayText());
        });
    }
    Q_EMIT viewChanged(m_view);
}

void WorkspaceLeaf::closeCurrentView()
{
    if (m_view) {
        m_view->close();
        // QtWidgets::setWidget(nullptr) is documented forbidden, but the Core
        // API setGuestView(nullptr) accepts a null shared_ptr and properly
        // teaches KDDW that the dock widget no longer has a guest. Without
        // this, a subsequent open() -> setWidget(newView) call would trip on
        // KDDW's stale guest reference (the old QWidget was reparented away
        // and queued for deleteLater, but KDDW still tracks it).
        if (m_dockWidget && m_dockWidget->dockWidget())
            m_dockWidget->dockWidget()->setGuestView(nullptr);
        m_view->deleteLater();
        m_view = nullptr;
    }
}

void WorkspaceLeaf::setAsCurrentTab()
{
    if (m_dockWidget)
        m_dockWidget->setAsCurrentTab();
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

// --- Unknown leaf keys ---

QJsonObject WorkspaceLeaf::unknownLeafKeys() const { return m_unknownLeafKeys; }

void WorkspaceLeaf::setUnknownLeafKeys(const QJsonObject &keys)
{
    m_unknownLeafKeys = keys;
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
        current.type = m_view->getViewType();
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
        current.type = m_view->getViewType();
        current.state = m_view->getState();
        current.eState = m_view->getEphemeralState();
    }

    LeafHistoryEntry entry = m_history.goBack(current);
    if (!entry.isValid())
        return;

    restoreFromHistory(entry);
}

void WorkspaceLeaf::goForward()
{
    if (!m_history.canGoForward())
        return;

    LeafHistoryEntry current;
    if (m_view) {
        current.title = m_view->getDisplayText();
        current.icon = m_view->getIcon();
        current.type = m_view->getViewType();
        current.state = m_view->getState();
        current.eState = m_view->getEphemeralState();
    }

    LeafHistoryEntry entry = m_history.goForward(current);
    if (!entry.isValid())
        return;

    restoreFromHistory(entry);
}

void WorkspaceLeaf::restoreFromHistory(const LeafHistoryEntry &entry)
{
    // Recreate the view when the entry's type differs from what's mounted
    // (mirrors navigate()'s type-switch). An empty type — e.g. a history entry
    // from a pre-type session — falls back to in-place setState.
    if (!entry.type.isEmpty() && (!m_view || m_view->getViewType() != entry.type)) {
        QJsonObject viewState;
        viewState[QStringLiteral("type")] = entry.type;
        viewState[QStringLiteral("state")] = entry.state;
        setViewState(viewState);  // recreates the view + applies inner state + emits viewChanged
    } else if (m_view) {
        m_view->setState(entry.state);
        Q_EMIT viewChanged(m_view);
    }

    if (m_view && !entry.eState.isEmpty())
        m_view->setEphemeralState(entry.eState);
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

    // Round-trip Obsidian's unknown leaf keys (forward-compat).
    for (auto it = m_unknownLeafKeys.begin(); it != m_unknownLeafKeys.end(); ++it)
        json.insert(it.key(), it.value());

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
