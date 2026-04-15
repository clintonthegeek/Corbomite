// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>
#include <optional>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace Corbomite {

class DataAdapter;

/// In-memory representation of `.obsidian/workspace.json`.
///
/// Schema per `docs/obsidian-audit/domains/workspace.md §2`:
///
///   LayoutJson = {
///     main?, left?, right?, floating?    : SplitNode / FloatingNode
///     'left-ribbon'?                     : { hiddenItems: Record<string, bool> }
///     active?                            : leaf-id (16-char random token)
///     lastOpenFiles?                     : string[]  (recent-file tracker)
///   }
///
///   SplitNode variants (discriminated by `type`):
///     "split" / "tabs" / "leaf" / "floating" / "window" / "mobile-drawer"
///
/// We store the whole object as a `QJsonObject` so every unknown key (future
/// Obsidian versions, plugin state, undocumented fields) round-trips
/// byte-for-byte. Typed accessors are convenience on top; the underlying
/// storage is never lossy.
class WorkspaceState
{
public:
    enum class NodeType {
        Unknown,
        Split,
        Tabs,
        Leaf,
        Floating,
        Window,
        MobileDrawer,
    };

    WorkspaceState() = default;
    explicit WorkspaceState(const QJsonObject &root) : m_root(root) {}

    // --- File I/O ---

    /// Load from `.obsidian/workspace.json` (or any path). Returns nullopt
    /// on missing or malformed file.
    static std::optional<WorkspaceState> load(DataAdapter *fs, const QString &path);

    /// Save via `DataAdapter` using Obsidian's 2-space-indent format.
    bool save(DataAdapter *fs, const QString &path) const;

    // --- Root accessors ---

    QJsonObject main() const;
    void setMain(const QJsonObject &node);

    QJsonObject left() const;
    void setLeft(const QJsonObject &node);

    QJsonObject right() const;
    void setRight(const QJsonObject &node);

    QJsonObject floating() const;
    void setFloating(const QJsonObject &node);

    QStringList lastOpenFiles() const;
    void setLastOpenFiles(const QStringList &paths);

    QString activeLeafId() const;
    void setActiveLeafId(const QString &id);

    /// Raw access — full object including unknown keys. Mutations here
    /// are reflected in the saved file.
    const QJsonObject &raw() const { return m_root; }
    QJsonObject &raw() { return m_root; }
    void setRaw(const QJsonObject &obj) { m_root = obj; }

    // --- SplitNode traversal helpers ---

    /// Discriminator — reads `type` field.
    static NodeType typeOf(const QJsonObject &node);

    /// `NodeType → "split"/"tabs"/…`
    static QString typeString(NodeType t);

    /// `children` array (empty for leaves / unknown / no-children nodes).
    static QJsonArray children(const QJsonObject &node);

    /// Replace the `children` field.
    static void setChildren(QJsonObject &node, const QJsonArray &kids);

    /// Walk the SplitNode tree rooted at `node`, invoking `visitor` for each
    /// node (pre-order). Visitor returns true to continue, false to stop.
    static void walk(const QJsonObject &node,
                     const std::function<bool(const QJsonObject &)> &visitor);

private:
    QJsonObject m_root;
};

} // namespace Corbomite
