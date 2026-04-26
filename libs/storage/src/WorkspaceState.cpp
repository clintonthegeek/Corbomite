// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/WorkspaceState.h"

#include "corbomite/storage/DataAdapter.h"
#include "corbomite/storage/VaultConfig.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace Corbomite {

std::optional<WorkspaceState> WorkspaceState::load(DataAdapter *fs, const QString &path)
{
    if (!fs) return std::nullopt;
    const auto bytes = fs->readBinary(path);
    if (!bytes.has_value()) return std::nullopt;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(*bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    return WorkspaceState(doc.object());
}

bool WorkspaceState::save(DataAdapter *fs, const QString &path) const
{
    if (!fs) return false;
    return fs->writeBinary(path, VaultConfig::serializeObsidianStyle(m_root));
}

QJsonObject WorkspaceState::main() const
{
    return m_root.value(QStringLiteral("main")).toObject();
}

void WorkspaceState::setMain(const QJsonObject &node)
{
    m_root.insert(QStringLiteral("main"), node);
}

QJsonObject WorkspaceState::left() const
{
    return m_root.value(QStringLiteral("left")).toObject();
}

void WorkspaceState::setLeft(const QJsonObject &node)
{
    m_root.insert(QStringLiteral("left"), node);
}

QJsonObject WorkspaceState::right() const
{
    return m_root.value(QStringLiteral("right")).toObject();
}

void WorkspaceState::setRight(const QJsonObject &node)
{
    m_root.insert(QStringLiteral("right"), node);
}

QJsonObject WorkspaceState::floating() const
{
    return m_root.value(QStringLiteral("floating")).toObject();
}

void WorkspaceState::setFloating(const QJsonObject &node)
{
    m_root.insert(QStringLiteral("floating"), node);
}

QStringList WorkspaceState::lastOpenFiles() const
{
    QStringList out;
    const auto arr = m_root.value(QStringLiteral("lastOpenFiles")).toArray();
    for (const auto &v : arr) {
        if (v.isString()) out.append(v.toString());
    }
    return out;
}

void WorkspaceState::setLastOpenFiles(const QStringList &paths)
{
    QJsonArray arr;
    for (const auto &p : paths) arr.append(p);
    m_root.insert(QStringLiteral("lastOpenFiles"), arr);
}

QString WorkspaceState::activeLeafId() const
{
    return m_root.value(QStringLiteral("active")).toString();
}

void WorkspaceState::setActiveLeafId(const QString &id)
{
    m_root.insert(QStringLiteral("active"), id);
}

WorkspaceState::NodeType WorkspaceState::typeOf(const QJsonObject &node)
{
    const QString t = node.value(QStringLiteral("type")).toString();
    if (t == QStringLiteral("split")) return NodeType::Split;
    if (t == QStringLiteral("tabs")) return NodeType::Tabs;
    if (t == QStringLiteral("leaf")) return NodeType::Leaf;
    if (t == QStringLiteral("floating")) return NodeType::Floating;
    if (t == QStringLiteral("window")) return NodeType::Window;
    if (t == QStringLiteral("mobile-drawer")) return NodeType::MobileDrawer;
    return NodeType::Unknown;
}

QString WorkspaceState::typeString(NodeType t)
{
    switch (t) {
    case NodeType::Split:        return QStringLiteral("split");
    case NodeType::Tabs:         return QStringLiteral("tabs");
    case NodeType::Leaf:         return QStringLiteral("leaf");
    case NodeType::Floating:     return QStringLiteral("floating");
    case NodeType::Window:       return QStringLiteral("window");
    case NodeType::MobileDrawer: return QStringLiteral("mobile-drawer");
    case NodeType::Unknown:      break;
    }
    return {};
}

QJsonArray WorkspaceState::children(const QJsonObject &node)
{
    return node.value(QStringLiteral("children")).toArray();
}

void WorkspaceState::setChildren(QJsonObject &node, const QJsonArray &kids)
{
    node.insert(QStringLiteral("children"), kids);
}

void WorkspaceState::walk(const QJsonObject &node,
                          const std::function<bool(const QJsonObject &)> &visitor)
{
    if (!visitor) return;
    if (!visitor(node)) return;
    const auto kids = children(node);
    for (const auto &k : kids) {
        if (k.isObject()) walk(k.toObject(), visitor);
    }
}

} // namespace Corbomite
