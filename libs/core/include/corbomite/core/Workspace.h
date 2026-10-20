// libs/core/include/corbomite/core/Workspace.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVector>

#include <functional>

#include "corbomite/core/LeafHistory.h"

namespace KDDockWidgets::QtWidgets {
class MainWindow;
}

namespace Corbomite {

class ViewRegistry;
class WorkspaceFloating;
class WorkspaceLeaf;
class WorkspaceWindow;

struct UndoEntry {
    QString leafId;
    QJsonObject state;
    QJsonObject eState;
    // Where to restore the leaf. When it closed with a live sibling in its own
    // tab group, `parentId` is that sibling and `parentSide` is empty (restore
    // as a tab beside it). When the leaf was the last tab in its pane and the
    // pane collapsed, `parentId` is the first live leaf of the adjacent pane
    // in the enclosing split and `parentSide` is "left"/"right"/"top"/"bottom"
    // (restore as a fresh split on that side, so the divider returns).
    QString parentId;
    QString parentSide;
    QString rootId;
    LeafHistory leafHistory;
    bool pinned = false;
    QString group;
};

class Workspace : public QObject
{
    Q_OBJECT
public:
    static constexpr int UndoCap = 10;

    /// Where `getLeaf(...)` should place a newly-created leaf relative to the
    /// currently active leaf. Mirrors the Obsidian `PaneType` shape that
    /// plugin authors expect from `workspace.getLeaf(...)`.
    /// - `Same`: return the active leaf (no creation) — fall back to a fresh
    ///   leaf if there is no active one.
    /// - `Tab`: new leaf in the active leaf's tab group.
    /// - `Split`: new leaf in a sibling pane created by splitting the active
    ///   leaf along `LeafDirection`.
    /// - `Window`: new leaf in a freshly popped-out floating window.
    enum class LeafMode { Same, Tab, Split, Window };
    Q_ENUM(LeafMode)

    /// Direction passed to `getLeaf(LeafMode::Split, ...)`. Other modes
    /// ignore this argument.
    enum class LeafDirection { Horizontal, Vertical };
    Q_ENUM(LeafDirection)

    explicit Workspace(ViewRegistry *registry, QObject *parent = nullptr);

    /// Vault-scoped constructor. The `vaultId` identifies which vault this
    /// workspace belongs to; used by the KDDW substrate for `DockRegistry`
    /// unique-name namespacing across multiple workspaces in one process.
    /// New callers should prefer this constructor.
    explicit Workspace(QString vaultId, ViewRegistry *registry,
                       QObject *parent = nullptr);
    ~Workspace() override;

    /// Identifier of the vault this workspace belongs to. Empty if the
    /// legacy (non-vaultId) constructor was used.
    QString vaultId() const;

    ViewRegistry *viewRegistry() const;

    /// Bare central widget of the workspace — the KDDW MainWindow.
    QWidget *rootWidget() const;

    WorkspaceLeaf *activeLeaf() const;
    void setActiveLeaf(WorkspaceLeaf *leaf);

    /// Layout-ready gate. Default `true` for freshly-constructed workspaces;
    /// flipped `false` while a workspace.json load is in flight to suppress
    /// activeLeafChanged cascades during materialization, then back to `true`
    /// (with `layoutReady` emitted). Mirrors Obsidian's `Workspace.layoutReady`
    /// invariant — see `docs/obsidian-audit/domains/workspace.md §"layout-ready"`.
    void setLayoutReady(bool ready);
    bool isLayoutReady() const;

    QStringList lastOpenFiles() const;
    void setLastOpenFiles(const QStringList &files);
    void pushLastOpenFile(const QString &path);

    // Tree operations

    /// Create a new leaf in the same tab group as `sibling`. Pass `nullptr`
    /// to create a new leaf in a new dock at the right (default location)
    /// or piggy-back the active group if one exists.
    WorkspaceLeaf *createLeafInGroupOf(WorkspaceLeaf *sibling);

    /// Create a new leaf in the active group, or as a fresh dock if there
    /// is no active leaf.
    WorkspaceLeaf *createLeafInActiveGroup();

    void closeLeaf(WorkspaceLeaf *leaf);
    bool canUndoCloseLeaf() const;
    void undoCloseLeaf();

    /// Close every leaf whose current view-type matches `type`, including
    /// deferred leaves whose persisted state declares the type. Mirrors
    /// Obsidian's `app.workspace.detachLeavesOfType(viewType)` invocation
    /// site — used by the host on plugin disable to evict any leaves the
    /// disabling plugin would have owned.
    void detachLeavesOfType(const QString &type);

    /// Split `leaf`'s pane along `direction`, creating a new sibling pane
    /// holding a fresh empty leaf which is returned. Returns nullptr if
    /// `leaf` is not in a splittable position.
    WorkspaceLeaf *splitLeaf(WorkspaceLeaf *leaf, Qt::Orientation direction);

    /// Obsidian-shape user-facing split: clone `leaf`'s view state (+ ephemeral
    /// state, history, pinned, group) into a new leaf in a new split sibling.
    /// The new leaf becomes active. Returns the new leaf, or nullptr on failure.
    WorkspaceLeaf *duplicateLeaf(WorkspaceLeaf *leaf, Qt::Orientation direction);

    /// Obsidian-shape leaf factory. Creates (or returns) a leaf positioned
    /// relative to the active leaf as `mode` requests; `dir` only used for
    /// `LeafMode::Split`. Mirrors `Workspace.getLeaf(...)` in the Obsidian
    /// plugin API. Returns `nullptr` only if `mode == Window` and the popout
    /// substrate refuses (should not occur in normal use).
    WorkspaceLeaf *getLeaf(LeafMode mode,
                            LeafDirection dir = LeafDirection::Horizontal);

    /// Resolver hook signature for `openLinkText`. Takes the unparsed
    /// path-only portion of a link (after stripping `#heading` / `^block`)
    /// + the source file's vault-relative path, and returns the
    /// vault-relative path to actually open. An empty return preserves the
    /// input verbatim — this is the default identity behaviour. Real
    /// resolution (Obsidian's `getFirstLinkpathDest` + create-if-missing
    /// fallback) requires a `MetadataCache` + `Vault` + `FileManager`,
    /// which live above libs/core; production callers (MainWindow / vault
    /// layer) install a lambda here.
    using LinkResolverFn =
        std::function<QString(const QString &path, const QString &source)>;
    void setLinkResolver(LinkResolverFn resolver);

    /// Obsidian-shape link dispatcher. Parses `[[linktext]]` — separates
    /// the path component from a `#heading` / `^blockid` subpath; runs the
    /// installed link resolver (if any) over the path; calls `getLeaf` for
    /// the requested mode; sets the leaf's view-state to
    /// `{type: "markdown", state: {file: <resolved>}}`; sets ephemeral
    /// state from `opts["eState"]` if present, else from the parsed
    /// subpath; focuses the leaf via `setActiveLeaf`. Returns true on
    /// success.
    bool openLinkText(const QString &linktext,
                       const QString &source,
                       LeafMode mode,
                       const QJsonObject &opts = {});

    // Popout windows
    WorkspaceWindow *popoutLeaf(WorkspaceLeaf *leaf);
    void reparentToMain(WorkspaceWindow *window);
    QVector<WorkspaceWindow *> windows() const;

    // Find nodes
    WorkspaceLeaf *findLeafById(const QString &id) const;
    QVector<WorkspaceLeaf *> allLeaves() const;

    /// Tab-group navigation. `nextLeafInActiveGroup` returns the leaf
    /// immediately after the active leaf in its tab group, wrapping back
    /// to the start. Returns nullptr if the active group has fewer than
    /// two leaves or there is no active leaf. Used by Ctrl+Tab.
    WorkspaceLeaf *nextLeafInActiveGroup() const;
    /// Mirror of `nextLeafInActiveGroup`. Used by Ctrl+Shift+Tab.
    WorkspaceLeaf *previousLeafInActiveGroup() const;

    /// Position of `leaf` inside its tab group (0-based), or -1 if `leaf`
    /// is not in any tab group. Used by tab context menus.
    int leafIndexInGroup(WorkspaceLeaf *leaf) const;
    /// Total number of leaves in `leaf`'s tab group, or 0 if `leaf` is
    /// not in any tab group.
    int leafCountInGroup(WorkspaceLeaf *leaf) const;

    /// Every leaf sharing `leaf`'s tab group (its pane), **in tab order**,
    /// including `leaf` itself; empty when `leaf` has no live dock widget.
    /// This is the membership accessor for tab-group questions — it reads
    /// KDDW's live group, the same substrate as `leafIndexInGroup`/
    /// `leafCountInGroup`/`moveLeafWithinGroup`, which are all documented as
    /// "tab group".
    ///
    /// Do NOT reach for `tabGroupIdOf()` + `groupMembers()` here: those are
    /// two different notions of "group". `groupMembers()` answers the
    /// *linked-pane* question and compares `WorkspaceLeaf::group()` — the id
    /// `linkLeaves()`/`unlinkLeaf()`/`isLinkedPane()` traffic in and the
    /// serializer round-trips as the leaf's `group` key. `tabGroupIdOf()`
    /// answers the *serializer's* question and reads `m_tabGroupOf`, already
    /// documented above as "no longer authoritative for membership". Pairing
    /// them returns an empty list, which is what silently disabled Go to
    /// Tab 1..8 (found live 2026-10-18).
    QVector<WorkspaceLeaf *> tabSiblings(WorkspaceLeaf *leaf) const;

    /// Move `leaf` one slot left (-1) or right (+1) within its tab group,
    /// returning true if it actually moved. Clamps at the ends: a leaf that
    /// is already leftmost (or rightmost) is a no-op returning false, so the
    /// caller can disable the action rather than have it silently do nothing.
    /// Works through KDDW's own `TabBar::moveTabTo`, so the tab bar, the
    /// workspace.json leaf order, and the active leaf all agree afterwards.
    bool moveLeafWithinGroup(WorkspaceLeaf *leaf, int delta);

    // ---- Linked-pane groups ----
    //
    // Obsidian groups tabs so that navigating one navigates every member
    // (audit: `FEATURE-MATRIX.md` "Linked-pane group"; `workspace.md:123`
    // records `setGroup(id|null)` firing `group-change` and
    // `setGroupMember(other|null)` *creating or reusing the target's* group
    // id, plus `workspace.md:392`: "setGroup joining a pinned group forces
    // the new member pinned").

    /// Join `a` and `b` into one linked-pane group, reusing `b`'s existing
    /// group id when it has one (matching `setGroupMember`'s create-or-reuse
    /// rule) and otherwise minting a fresh id. Both leaves end up with the
    /// same group; if either was pinned, both end up pinned. Returns false
    /// if either argument is null or they are the same leaf.
    bool linkLeaves(WorkspaceLeaf *a, WorkspaceLeaf *b);

    /// Break `leaf` out of any linked-pane group, clearing the group on
    /// every member so none is left pointing at a group it is no longer
    /// part of. No-op for a null or ungrouped leaf.
    void unlinkLeaf(WorkspaceLeaf *leaf);

    /// True when `leaf` shares its group with at least one other live leaf.
    bool isLinkedPane(WorkspaceLeaf *leaf) const;

    /// The leaf that was active immediately before the current one. This is
    /// Obsidian's "the active pane" for Link-with-active-pane; tracked
    /// because Corbomite had no notion of it, and picking an arbitrary other
    /// visible leaf would make the command's effect unpredictable.
    WorkspaceLeaf *previousActiveLeaf() const { return m_previousActiveLeaf; }

    /// Close every leaf in `leaf`'s tab group except `leaf` itself.
    /// No-op if `leaf` is not in any tab group.
    void closeOtherLeavesInGroupOf(WorkspaceLeaf *leaf);
    /// Close every leaf in `leaf`'s tab group whose tab index is greater
    /// than `leaf`'s. No-op if `leaf` is not in any tab group.
    void closeLeavesToRightOf(WorkspaceLeaf *leaf);

    // Linked-pane group propagation
    void propagatePinToGroup(WorkspaceLeaf *leaf);
    QVector<WorkspaceLeaf *> groupMembers(const QString &groupId) const;

    /// Returns the first unpinned leaf in `sibling`'s tab group, or
    /// creates a new leaf there if all are pinned.
    WorkspaceLeaf *findOrCreateUnpinnedLeafInGroupOf(WorkspaceLeaf *sibling);

    // Persistence
    QJsonObject serialize() const;
    void deserialize(const QJsonObject &json);
    void readWorkspaceJson(const QString &vaultPath);
    void writeWorkspaceJson(const QString &vaultPath);

    /// Tear down the current tree and recreate an empty default layout.
    void resetToDefaultLayout();

    /// Request the host reveal the dock panel for the given plugin slug.
    void revealDockView(const QString &slug);

    /// Request the host dispatch `commandId` through its CommandRegistry.
    void requestCommand(const QString &commandId);

    // Package-private — used by WorkspaceSerializer to drive the substrate.
    KDDockWidgets::QtWidgets::MainWindow *kddwMainWindow() const { return m_kddwMain; }

    /// Create + register a leaf with the given persistent id but do NOT
    /// dock it. The serializer drives placement explicitly during
    /// fromJson so it can route nested splits and floating-window
    /// attachments correctly. Caller is responsible for: (1) docking the
    /// leaf's KDDW dock widget at the desired location; (2) updating the
    /// tab-group bookkeeping via setTabGroupOf.
    WorkspaceLeaf *createLeafUnplaced(const QString &leafId);

    /// Set the opaque tab-group id this leaf belongs to. Called by the
    /// serializer after dock placement. Generates a fresh id when the
    /// caller passes an empty string.
    void setTabGroupOf(WorkspaceLeaf *leaf, const QString &tabGroupId);

    /// Generate a fresh 16-hex-char tab-group id. Exposed for the
    /// serializer; production callers use setTabGroupOf with an empty
    /// string instead.
    static QString freshTabGroupId();

    /// Whether the tab group named `tabGroupId` is rendered in stacked
    /// (all-tabs-side-by-side) mode. Round-tripped verbatim through
    /// workspace.json. Currently advisory — KDDW lacks a stacked-rendering
    /// hook — but the bit preserves the user's intent across save+load.
    bool isTabGroupStacked(const QString &tabGroupId) const;
    void setTabGroupStacked(const QString &tabGroupId, bool stacked);

    /// Read m_tabGroupOf for a leaf. Used by the serializer to look up
    /// the live tab-group id on the write side.
    QString tabGroupIdOf(WorkspaceLeaf *leaf) const;

    /// Popout-window container; its `windows()`
    /// mirrors `Workspace::windows()`. The Obsidian-shape `rootSplit()` /
    /// `leftSplit()` / `rightSplit()` bookkeeping shells were removed —
    /// they had no real callers and
    /// `leftSplit()`/`rightSplit()` returned literal `nullptr`, a
    /// crash invitation for any caller that assumed Obsidian-shape
    /// compatibility. Left/right sidebars still live in `ToolViewHost`;
    /// when they are migrated into the Workspace tree, add honest
    /// accessors then.
    WorkspaceFloating *floatingSplit() const { return m_floating; }

    /// Re-emit `cssChange()` from the host. Called by MainWindow whenever
    /// `ThemeService::themeChanged` fires. Mirrors Obsidian's
    /// `app.workspace.trigger("css-change")` invocation site.
    void emitCssChange() { Q_EMIT cssChange(); }

Q_SIGNALS:
    void activeLeafChanged(WorkspaceLeaf *leaf);
    void layoutChanged();
    /// Emitted on every `false → true` transition of the layout-ready gate
    /// (typically at the end of `deserialize`/`readWorkspaceJson`), once
    /// the workspace is safe to consume `activeLeafChanged` from.
    void layoutReady();
    /// Emitted whenever the host's KDDW MainWindow widget receives a
    /// QEvent::Resize. Mirrors Obsidian's `Workspace.on("resize")`.
    void resize();
    /// Emitted whenever the floating-window topology changes — a popout
    /// creates a new FloatingWindow, or a FloatingWindow is destroyed
    /// (close-via-X or `reparentToMain`). Mirrors Obsidian's
    /// `Workspace.on("window-frame-change")`.
    void windowFrameChange();
    void leafClosed(WorkspaceLeaf *leaf);
    void revealDockViewRequested(const QString &slug);
    void commandRequested(const QString &commandId);

    /// Mirrors Obsidian's `Workspace.on("css-change")`. Re-emitted by the
    /// host whenever the active theme changes (ThemeService::themeChanged).
    /// The host calls `emitCssChange()` from a one-line slot wired in
    /// MainWindow::onWorkspaceCreated. Plugins observe via
    /// `WorkspaceController::cssChange`.
    void cssChange();

    /// Re-emission of "the user clicked a tab to make this leaf active"
    /// from the substrate. Hosts (MainWindow) connect once at Workspace
    /// setup time and call setActiveLeaf in response.
    void tabSelectRequested(WorkspaceLeaf *leaf);

    /// Re-emission of "the user clicked the X on this leaf's tab" from the
    /// substrate. Hosts call closeLeaf in response.
    void tabCloseRequested(WorkspaceLeaf *leaf);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Whether `destroyLeaves` deletes its leaves synchronously or schedules
    /// `deleteLater`. `Immediate` is for batch teardown paths
    /// (`~Workspace`, `deserialize`, `resetToDefaultLayout`) where the whole
    /// tree is being replaced and nothing downstream needs the leaf to
    /// survive to the next event-loop turn. `Deferred` is for `closeLeaf`,
    /// which must outlive the KDDW signal handler that's still unwinding on
    /// the call stack when it's invoked.
    enum class TeardownMode { Immediate, Deferred };

    /// The single leaf-destruction primitive. Unregisters each leaf from
    /// `m_leaves`/`m_leavesById`/
    /// `m_tabGroupOf` (and clears it as `m_activeLeaf` if applicable)
    /// *before* touching its dock widget, so any KDDW signal that fires
    /// re-entrantly while a sibling leaf in the same batch is being torn
    /// down sees this leaf as already gone rather than mid-flight. Cuts the
    /// dock widget's signal connections as a defense-in-depth measure on
    /// top of the context-object fix, then deletes per `mode`. Does not
    /// touch `m_stackedGroups` (not leaf-keyed) or emit `layoutChanged`/
    /// `leafClosed` — callers own those.
    void destroyLeaves(QVector<WorkspaceLeaf *> leaves, TeardownMode mode);

    void registerLeaf(WorkspaceLeaf *leaf);
    void unregisterLeaf(WorkspaceLeaf *leaf);
    void wireLeafKddwSignals(WorkspaceLeaf *leaf);

    QString m_vaultId;
    ViewRegistry *m_registry;
    KDDockWidgets::QtWidgets::MainWindow *m_kddwMain = nullptr;

    // Leaf indexes. `m_leaves` is insertion-ordered for stable iteration;
    // `m_leavesById` is the O(1) findLeafById index;
    // `m_tabGroupOf` carries an opaque tab-group identifier per leaf,
    // assigned at leaf-create / serializer-restore time. It is no longer
    // authoritative for live tab-group membership — tab-navigation
    // primitives (`nextLeafInActiveGroup`, `closeOtherLeavesInGroupOf`,
    // etc.) read membership from KDDW directly via
    // `DockRegistry::groups()` + `Group::dockWidgets()` so user
    // drag-tab-to-other-group is reflected immediately. The cached id
    // survives only as the key for per-group stacked-state and as a
    // hand-off token to the serializer (see addendum
    // docs/obsidian-audit/addenda/2026-04-26-kddw-public-enumeration.md).
    QVector<WorkspaceLeaf *> m_leaves;
    QHash<QString, WorkspaceLeaf *> m_leavesById;
    QHash<WorkspaceLeaf *, QString> m_tabGroupOf;
    QHash<QString, bool> m_stackedGroups;

    WorkspaceLeaf *m_activeLeaf = nullptr;
    WorkspaceLeaf *m_previousActiveLeaf = nullptr;
    bool m_layoutReady = true;
    /// Resize-event coalescing guard: true while a singleShot(0) flush
    /// of `resize()` is already scheduled for the current event-loop turn.
    bool m_resizeCoalesceScheduled = false;
    QVector<WorkspaceWindow *> m_windows;
    QVector<UndoEntry> m_undoHistory;
    QStringList m_lastOpenFiles;
    LinkResolverFn m_linkResolver;
    WorkspaceFloating *m_floating = nullptr;

    /// Root-level keys from a loaded workspace.json that Workspace doesn't
    /// itself model (e.g. `left`/`right` sidedock sub-trees, or any future
    /// Obsidian schema key). Round-tripped unchanged by serialize() so a
    /// Corbomite load->save cycle never destroys data Obsidian wrote.
    /// Workspace-compat-boundary doctrine: `_corbomite` and
    /// `left-ribbon` are explicitly denylisted here — they were Corbomite's
    /// own retired keys, not Obsidian schema, and must not be swept into
    /// this passthrough (see SessionManager and the compat-boundary spec).
    QJsonObject m_unknownRoot;
};

} // namespace Corbomite
