// libs/core/src/WorkspaceLeaf.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"

#include <QRandomGenerator>
#include <QVBoxLayout>

namespace Corbomite {

WorkspaceLeaf::WorkspaceLeaf(ViewRegistry *registry, QWidget *parent)
    : QWidget(parent)
    , m_id(generateId())
    , m_registry(registry)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}

WorkspaceLeaf::~WorkspaceLeaf()
{
    closeCurrentView();
}

QString WorkspaceLeaf::id() const { return m_id; }
View *WorkspaceLeaf::view() const { return m_view; }

void WorkspaceLeaf::open(View *newView)
{
    closeCurrentView();
    m_view = newView;
    if (m_view) {
        m_view->open(this);
        layout()->addWidget(m_view);
    }
    Q_EMIT viewChanged(m_view);
}

void WorkspaceLeaf::closeCurrentView()
{
    if (m_view) {
        m_view->close();
        layout()->removeWidget(m_view);
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
    }
    return state;
}

void WorkspaceLeaf::setViewState(const QJsonObject &state)
{
    QString type = state[QStringLiteral("type")].toString();
    if (type.isEmpty() || !m_registry) return;

    auto factory = m_registry->getViewCreatorByType(type);
    if (!factory) {
        closeCurrentView();
        return;
    }

    auto *newView = factory(this);
    open(newView);

    QJsonObject viewState = state[QStringLiteral("state")].toObject();
    if (!viewState.isEmpty())
        m_view->setState(viewState);
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

QJsonObject WorkspaceLeaf::serialize() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = m_id;
    json[QStringLiteral("type")] = QStringLiteral("leaf");
    json[QStringLiteral("state")] = getViewState();
    return json;
}

WorkspaceLeaf *WorkspaceLeaf::deserialize(const QJsonObject &json,
                                           ViewRegistry *registry,
                                           QWidget *parent)
{
    auto *leaf = new WorkspaceLeaf(registry, parent);
    leaf->m_id = json[QStringLiteral("id")].toString();
    if (leaf->m_id.isEmpty())
        leaf->m_id = generateId();

    QJsonObject viewState = json[QStringLiteral("state")].toObject();
    if (!viewState.isEmpty())
        leaf->setViewState(viewState);

    return leaf;
}

QString WorkspaceLeaf::generateId()
{
    static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    QString id;
    id.reserve(16);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 16; ++i)
        id.append(QLatin1Char(chars[rng->bounded(36)]));
    return id;
}

} // namespace Corbomite
