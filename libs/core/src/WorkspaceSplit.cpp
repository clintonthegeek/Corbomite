// libs/core/src/WorkspaceSplit.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceSplit.h"

#include <QJsonArray>
#include <QSplitter>

namespace Corbomite {

WorkspaceSplit::WorkspaceSplit(QObject *parent)
    : WorkspaceParent(parent)
    , m_splitter(new QSplitter)
{
    m_splitter->setOrientation(m_direction);
    m_splitter->setChildrenCollapsible(false);
}

WorkspaceSplit::~WorkspaceSplit()
{
    delete m_splitter;
}

Qt::Orientation WorkspaceSplit::direction() const { return m_direction; }

void WorkspaceSplit::setDirection(Qt::Orientation dir)
{
    m_direction = dir;
    m_splitter->setOrientation(dir);
}

QWidget *WorkspaceSplit::widget() { return m_splitter; }

void WorkspaceSplit::addChild(WorkspaceItem *child, int index)
{
    WorkspaceParent::addChild(child, index);
    if (auto *w = child->widget()) {
        int idx = m_children.indexOf(child);
        m_splitter->insertWidget(idx, w);
    }
    syncDimensionsToSplitter();
}

void WorkspaceSplit::removeChild(WorkspaceItem *child, bool deleteChild)
{
    if (auto *w = child->widget())
        w->setParent(nullptr);
    WorkspaceParent::removeChild(child, deleteChild);
    syncDimensionsToSplitter();
}

void WorkspaceSplit::syncDimensionsFromSplitter()
{
    QList<int> sizes = m_splitter->sizes();
    int total = 0;
    for (int s : sizes)
        total += s;
    if (total == 0)
        return;

    for (int i = 0; i < m_children.size() && i < sizes.size(); ++i) {
        int pct = (sizes[i] * 100) / total;
        m_children[i]->setDimension(pct);
    }
}

void WorkspaceSplit::syncDimensionsToSplitter()
{
    if (m_children.isEmpty())
        return;

    QList<int> sizes;
    bool anySet = false;
    for (auto *child : m_children) {
        int dim = child->dimension().value_or(0);
        if (dim > 0)
            anySet = true;
        sizes.append(dim);
    }

    if (!anySet) {
        int equal = 100 / m_children.size();
        sizes.fill(equal, m_children.size());
    }

    m_splitter->setSizes(sizes);
}

QJsonObject WorkspaceSplit::serialize() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id();
    json[QStringLiteral("type")] = QStringLiteral("split");
    json[QStringLiteral("direction")] =
        m_direction == Qt::Horizontal ? QStringLiteral("horizontal")
                                      : QStringLiteral("vertical");

    if (dimension().has_value())
        json[QStringLiteral("dimension")] = dimension().value();

    QJsonArray children;
    for (const auto *child : m_children)
        children.append(child->serialize());
    json[QStringLiteral("children")] = children;

    return json;
}

} // namespace Corbomite
