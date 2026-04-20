// SPDX-License-Identifier: GPL-3.0-or-later
#include "RibbonStateController.h"

#include "RibbonToolBar.h"
#include "SessionManager.h"

namespace Corbomite {

namespace {
constexpr auto kHiddenItems = "hiddenItems";
} // namespace

RibbonStateController::RibbonStateController(RibbonToolBar *toolBar,
                                             SessionManager *session,
                                             QObject *parent)
    : QObject(parent)
    , m_toolBar(toolBar)
    , m_session(session)
{
    if (m_toolBar) {
        connect(m_toolBar, &RibbonToolBar::iconAdded,
                this, &RibbonStateController::onIconAdded);
        connect(m_toolBar, &RibbonToolBar::iconVisibilityChanged,
                this, &RibbonStateController::onIconVisibilityChanged);
    }
}

RibbonStateController::~RibbonStateController() = default;

void RibbonStateController::rebind(SessionManager *session)
{
    m_session = session;
    m_cachedHiddenItems = {};
}

void RibbonStateController::applyFromSession()
{
    if (!m_session) {
        m_cachedHiddenItems = {};
        return;
    }
    const QJsonObject ribbon = m_session->leftRibbonState();
    m_cachedHiddenItems = ribbon.value(QLatin1String(kHiddenItems)).toObject();

    if (!m_toolBar) return;
    const QStringList ids = m_toolBar->iconIdsInOrder();
    for (const auto &id : ids) {
        const bool hidden = m_cachedHiddenItems.value(id).toBool(false);
        m_toolBar->setIconVisible(id, !hidden);
    }
}

void RibbonStateController::onIconAdded(const QString &id)
{
    if (!m_toolBar) return;
    const bool hidden = m_cachedHiddenItems.value(id).toBool(false);
    if (hidden) m_toolBar->setIconVisible(id, false);
}

void RibbonStateController::onIconVisibilityChanged(const QString &id, bool visible)
{
    writeThrough(id, !visible);
}

void RibbonStateController::writeThrough(const QString &id, bool hidden)
{
    if (!m_session) return;
    QJsonObject ribbon = m_session->leftRibbonState();
    QJsonObject items = ribbon.value(QLatin1String(kHiddenItems)).toObject();
    if (hidden) {
        items.insert(id, true);
    } else {
        items.remove(id);
    }
    m_cachedHiddenItems = items;
    if (items.isEmpty()) {
        ribbon.remove(QLatin1String(kHiddenItems));
    } else {
        ribbon.insert(QLatin1String(kHiddenItems), items);
    }
    m_session->setLeftRibbonState(ribbon);
}

} // namespace Corbomite
