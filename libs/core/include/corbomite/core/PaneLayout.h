// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CORBOMITE_CORE_PANELAYOUT_H
#define CORBOMITE_CORE_PANELAYOUT_H

#include <functional>
#include <memory>
#include <optional>

#include <QJsonObject>
#include <QList>
#include <QString>

#include <Qt>

namespace Corbomite {

/// One visible view inside a pane — one Obsidian leaf. A pane-layout leaf
/// node may hold multiple stacked `PaneLeaf`s (= Obsidian `tabs` node).
///
/// Fields cover what Corbomite needs to re-open the view; `viewState` carries
/// the full Obsidian `ViewState` object so unknown fields survive round-trip.
struct PaneLeaf {
    /// 16-char stable id (Obsidian convention; `active` on the workspace
    /// root refers to one of these).
    QString id;
    /// View factory key: `"markdown"`, `"canvas"`, plugin-provided, …
    QString viewType;
    /// Vault-relative file path for `FileView`-descended views. Empty
    /// otherwise.
    QString filePath;
    /// View-specific mode (`"source"` / `"preview"` for `MarkdownView`).
    QString mode;
    bool pinned = false;
    /// Optional tab-group tag (Obsidian `group`).
    QString group;
    /// The *full* `ViewState` object as read from workspace.json. Round-trip
    /// preserves plugin-stashed fields inside `viewState.state`.
    QJsonObject viewState;
    /// Any leaf-level unknown JSON keys (outside the fields above), carried
    /// through the round-trip.
    QJsonObject unknown;
};

/// B-tree split index — one node in a pane-layout tree.
///
/// Invariant (inherited from KDevelop's `Sublime::AreaIndex`): a node either
/// holds **children** (`first` + `second`, `isSplit() == true`) OR holds a
/// list of stacked **views**, never both. Stacked views at a single index
/// correspond to Obsidian's `tabs` node; a split index corresponds to the
/// `split` node; a leaf index with exactly one view corresponds to a pure
/// `leaf` node (wrapped in a trivial tabs container when we serialise).
///
/// Design ported from `~/src/kde/src/kdevelop/kdevplatform/sublime/areaindex.{h,cpp}`
/// (LGPL-2.0+, relicensed under GPL3 here). Implementation rewritten, not
/// copied, to drop KConfig and re-root on std::unique_ptr ownership.
class PaneLayoutIndex
{
public:
    PaneLayoutIndex();
    explicit PaneLayoutIndex(PaneLayoutIndex *parent);
    ~PaneLayoutIndex();

    // Non-copyable, non-movable (parent pointers make it unsafe).
    PaneLayoutIndex(const PaneLayoutIndex &) = delete;
    PaneLayoutIndex &operator=(const PaneLayoutIndex &) = delete;

    // --- Tree queries ---

    bool isSplit() const;
    PaneLayoutIndex *parent() const;
    PaneLayoutIndex *first() const;
    PaneLayoutIndex *second() const;

    // --- Split-node accessors ---

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation);

    /// Obsidian flex-ratio (0-100 across siblings). `nullopt` = flexible.
    std::optional<double> dimension() const;
    void setDimension(std::optional<double>);

    /// Stable id of this index (Obsidian `split`/`tabs` node `id` field).
    /// Generated lazily on first access.
    QString indexId() const;
    void setIndexId(const QString &);

    /// Catch-all for JSON keys this layer doesn't interpret — propagated
    /// on round-trip (unknown-key preservation invariant).
    QJsonObject unknownKeys() const;
    void setUnknownKeys(const QJsonObject &);

    // --- Leaf-node (stacked views / tabs) accessors ---

    const QList<PaneLeaf> &views() const;
    int viewCount() const;
    const PaneLeaf *viewAt(int position) const;
    PaneLeaf *viewAt(int position);

    /// Active tab index (Obsidian `currentTab`, 0-based). Omitted from JSON
    /// when equal to zero (matches Obsidian).
    int currentTab() const;
    void setCurrentTab(int);

    /// Obsidian `stacked: true` — UI hint for stacked tab presentation.
    bool stacked() const;
    void setStacked(bool);

    // --- Mutations ---

    /// Add a view to this (leaf) index. If the index is split, this is a
    /// no-op (matches KDevelop's safety guard).
    void addView(PaneLeaf leaf, int afterIndex = -1);

    /// Remove a view by id. If the removal leaves the index empty AND the
    /// node has a parent, the parent is auto-unsplit (sibling contents flow
    /// up, the now-empty child is destroyed).
    void removeView(const QString &leafId);

    void moveViewPosition(const QString &leafId, int newPos);

    /// Split this index; existing views move to `first` (or `second` if
    /// `moveViewsToSecondChild` is true). No-op if the node is already split.
    void split(Qt::Orientation dir, bool moveViewsToSecondChild = false);

    /// Split this index and place `newLeaf` as the sole view of the new
    /// `second` child.
    void splitWithNewLeaf(PaneLeaf newLeaf, Qt::Orientation dir);

    /// Unsplit: merge `childToRemove`'s sibling up into this node. Both
    /// children are deleted. No-op if this node isn't split.
    void unsplit(PaneLayoutIndex *childToRemove);

    /// Pre-order visitor. Visitor may return false to stop traversal.
    void walk(const std::function<bool(PaneLayoutIndex *)> &visitor);
    void walk(const std::function<bool(const PaneLayoutIndex *)> &visitor) const;

    // Transplant helpers used by PaneLayout's JSON builder and by
    // unsplit(); public-but-advanced — normal users should prefer
    // split/unsplit/add/removeView.
    void adoptFrom(PaneLayoutIndex *source);
    void setParent(PaneLayoutIndex *p);
    /// Install a child in the first/second slot. Takes ownership, repoints
    /// parent. Intended for the JSON load path only.
    void setFirst(std::unique_ptr<PaneLayoutIndex> child);
    void setSecond(std::unique_ptr<PaneLayoutIndex> child);
    /// Replace the leaf-view list wholesale (JSON load path).
    void setViews(QList<PaneLeaf> views);

private:
    void moveViewsTo(PaneLayoutIndex *target);
    void copyChildrenTo(PaneLayoutIndex *target);

    QList<PaneLeaf> m_views;
    PaneLayoutIndex *m_parent = nullptr;
    std::unique_ptr<PaneLayoutIndex> m_first;
    std::unique_ptr<PaneLayoutIndex> m_second;
    Qt::Orientation m_orientation = Qt::Horizontal;
    std::optional<double> m_dimension;
    int m_currentTab = 0;
    bool m_stacked = false;
    mutable QString m_indexId; // lazily generated
    QJsonObject m_unknown;
};

/// Top-level pane layout for a window region (Obsidian `main` / `left` /
/// `right` SplitNode). Holds the root `PaneLayoutIndex` plus convenience
/// helpers.
class PaneLayout
{
public:
    PaneLayout();
    ~PaneLayout();

    PaneLayout(const PaneLayout &) = delete;
    PaneLayout &operator=(const PaneLayout &) = delete;
    PaneLayout(PaneLayout &&) noexcept;
    PaneLayout &operator=(PaneLayout &&) noexcept;

    PaneLayoutIndex *root();
    const PaneLayoutIndex *root() const;

    QString activeLeafId() const;
    void setActiveLeafId(const QString &);

    /// Locate a leaf by id anywhere in the tree. nullptr on miss.
    PaneLeaf *findLeaf(const QString &id);
    const PaneLeaf *findLeaf(const QString &id) const;

    // --- Obsidian SplitNode JSON round-trip ---

    /// Build a PaneLayout from an Obsidian `SplitNode` object (the `main`/
    /// `left`/`right` value in workspace.json). Unknown node types are
    /// silently skipped (matches Obsidian's forward-compat degradation).
    static PaneLayout fromJson(const QJsonObject &splitNode);

    /// Emit an Obsidian `SplitNode` object. A leaf-index with exactly one
    /// view becomes a bare `leaf` node; a leaf-index with 2+ views becomes
    /// a `tabs` node wrapping `leaf` children.
    QJsonObject toJson() const;

    /// Utility: mint a 16-char random id for a new leaf or index. Exposed
    /// for callers that allocate ids at view-creation time.
    static QString newId();

private:
    std::unique_ptr<PaneLayoutIndex> m_root;
    QString m_activeLeafId;
};

} // namespace Corbomite

#endif // CORBOMITE_CORE_PANELAYOUT_H
