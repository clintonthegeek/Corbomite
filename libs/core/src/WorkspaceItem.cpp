// libs/core/src/WorkspaceItem.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceItem.h"
#include <QRandomGenerator>

namespace Corbomite {

WorkspaceItem::WorkspaceItem(QObject *parent)
    : QObject(parent)
    , m_id(generateId())
{
}

WorkspaceItem::~WorkspaceItem() = default;

QString WorkspaceItem::id() const { return m_id; }

void WorkspaceItem::setId(const QString &id) { m_id = id; }

std::optional<int> WorkspaceItem::dimension() const { return m_dimension; }

void WorkspaceItem::setDimension(std::optional<int> dim) { m_dimension = dim; }

WorkspaceParent *WorkspaceItem::parentItem() const { return m_parentItem; }

void WorkspaceItem::setParentItem(WorkspaceParent *parent) { m_parentItem = parent; }

QString WorkspaceItem::generateId()
{
    static const char chars[] = "0123456789abcdef";
    QString result;
    result.reserve(16);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 16; ++i)
        result.append(QLatin1Char(chars[rng->bounded(16)]));
    return result;
}

} // namespace Corbomite
