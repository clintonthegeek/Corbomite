// libs/core/src/internal/LeafSubstrateAdapter.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "internal/LeafSubstrateAdapter.h"

#include "corbomite/core/WorkspaceLeaf.h"

namespace Corbomite {

LeafSubstrateAdapter::LeafSubstrateAdapter(WorkspaceLeaf *leaf, QObject *parent)
    : WorkspaceItem(parent)
    , m_leaf(leaf)
{
    if (m_leaf)
        setId(m_leaf->id());
}

LeafSubstrateAdapter::~LeafSubstrateAdapter() = default;

QWidget *LeafSubstrateAdapter::widget()
{
    return m_leaf ? m_leaf->widget() : nullptr;
}

QJsonObject LeafSubstrateAdapter::serialize() const
{
    return m_leaf ? m_leaf->serialize() : QJsonObject{};
}

} // namespace Corbomite
