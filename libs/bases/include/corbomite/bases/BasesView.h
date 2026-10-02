// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesQuery.h"
#include "corbomite/bases/NewItemSeed.h"

#include "corbomite/core/TextFileView.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QPoint>
#include <QString>
#include <QUndoStack>

#include <functional>
#include <memory>

class QAction;
class QLabel;
class QLineEdit;
class QMenu;
class QComboBox;
class QToolButton;
class QTreeView;
class QSplitter;

namespace Corbomite {
class FileManager;
class MetadataCache;
class NoteDocument;
class TFile;
class Vault;
class WorkspaceLeaf;
}  // namespace Corbomite

namespace Corbomite::Bases {

class BasesCellDelegate;
class BasesTreeModel;
class FunctionRegistry;
class QueryController;
class PropertiesMenuPanel;
class SortGroupMenuPanel;
class ViewsMenuPanel;
class PropertiesDrawer;
struct FilterSpec;

/// Main-area view widget for `.base` files. TextFileView subclass — save/
/// load plumbing comes from the base class; setViewData/getViewData
/// round-trip through BasesQuery::fromString/toString.
class BasesView : public TextFileView
{
    Q_OBJECT
public:
    explicit BasesView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);
    ~BasesView() override;

    /// Factory for ViewRegistry::registerViewWithExtensions.
    static Corbomite::View *factory(WorkspaceLeaf *leaf);

    /// Host injects these before the first setViewData call.
    void setServices(Vault *vault,
                     MetadataCache *cache,
                     FileManager *fileManager,
                     FunctionRegistry *funcs = nullptr);

    QString getViewData() const override;
    void setViewData(const QString &data, bool clear) override;
    void clear() override;

    QString getViewType() const override { return QStringLiteral("bases"); }

    BasesQuery *query() const { return m_query.get(); }
    BasesViewConfig *activeView() const { return m_activeView; }
    void setActiveView(const QString &name);

    /// Set the file `this` refers to inside `.base` formula expressions —
    /// host wires this to `Workspace::activeLeafChanged` so the table
    /// re-runs when the user switches notes. Re-uses BasesView's existing
    /// QueryController; no debouncing on this side (controller already
    /// debounces).
    void setCurrentFile(Corbomite::TFile *file);

    /// Host callbacks for cell interactions that escape the base's own leaf.
    void setOpenInNewTabHandler(std::function<void(const QString &path)> cb) { m_openInNewTab = std::move(cb); }
    void setTagSearchHandler(std::function<void(const QString &tag)> cb) { m_searchTag = std::move(cb); }
    void setRenamePrompt(std::function<void(const QString &path)> cb) { m_promptRename = std::move(cb); }
    void setDeletePrompt(std::function<void(const QString &path)> cb) { m_promptDelete = std::move(cb); }

    /// Drive the per-view undo stack (host wires these to Edit ▸ Undo/Redo).
    void undo();
    void redo();
    /// Cluster O Phase O1.T8 — real undo-stack-depth query so the host's
    /// edit_undo/edit_redo KActions can be enabled/disabled honestly
    /// instead of always-enabled. O2.T1: now overrides View's virtual.
    bool canUndo() const override { return m_undoStack.canUndo(); }
    bool canRedo() const override { return m_undoStack.canRedo(); }

    /// Cluster O Phase O1.T5 — route the universal edit_find action here:
    /// Bases already owns a search box in its toolbar, so Ctrl+F on a
    /// bases tab should focus it rather than silently do nothing.
    void focusSearch();

    // Cluster O Phase O2.T1 — remaining Tier-B capability overrides.
    bool canEdit() const override { return true; }
    bool canSave() const override { return true; }
    bool canFind() const override { return true; }
    bool canZoom() const override { return false; }
    bool hasSelection() const override;

    /// Assign (or clear) a summary function for `prop` in the active view.
    /// Public so tests can exercise the mutation without spawning a dialog.
    void applySummaryChoice(const PropertyId &prop, const QString &fnName);

    /// Replace both filter scopes from edited specs, then recompute + persist.
    /// Public for testability (the dialog path calls this on accept).
    void applyFilterSpecs(const FilterSpec &globalSpec, const FilterSpec &perViewSpec);

    // ---- Cluster O Phase O5.T1/T2 — the KAction-facing toolbar surface ----
    //
    // The in-view toolbar's seven QToolButtons used to carry their whole
    // behaviour in `clicked` lambdas inside the ctor, which meant the
    // behaviour was unreachable from a menubar, a command palette, or the
    // Hotkeys page. These hoist each lambda to a named method so
    // `BasesViewActions` can own the QActions and drive them — the buttons
    // then become plain `setDefaultAction()` carriers, giving one
    // definition and four surfaces (in-view, menubar, palette, Hotkeys).

    /// Feed state + show the Properties / Sort+group / Views popup panels.
    /// Each no-ops when the query or active view is missing, and each
    /// carries the same 150 ms re-click debounce the button lambdas had.
    void showPropertiesPanel();
    void showSortGroupPanel();
    void showViewsPanel();

    /// Properties pane (the right-hand drawer) — the checkable toggle's
    /// read and write halves, so the action's checked state can be synced
    /// from real state instead of guessed.
    bool propertiesPaneVisible() const;
    void setPropertiesPaneVisible(bool visible);

    // Tier-B predicates (O5.T2). These are the enablement rules the
    // provider mirrors; keeping them here means one definition of "can
    // this control do anything" rather than a second copy in the provider.
    /// A `.base` file has been parsed. False while the tab is empty/erroring.
    bool hasQuery() const { return static_cast<bool>(m_query); }
    /// A named view is selected — the Properties/Sort/Views panels are all
    /// per-view state, so they need one.
    bool hasActiveView() const { return m_activeView != nullptr; }
    /// Either filter scope is present. Per the RE (`bases.md` §2,
    /// `BasesQuery.filters` / `BasesViewConfig.filters` are both `optional`),
    /// an absent filter node means "no filtering" and a present one means
    /// filtering is in force — so this is a null check on either scope, not
    /// a structural walk.
    bool hasActiveFilters() const;
    /// The query has a writable target for a new entry: a FileManager bound
    /// to a live vault. Note `FileManager::createMarkdownNote` auto-creates
    /// a missing `newItemFolder`, so an unset or absent folder is NOT a
    /// reason to disable — it resolves to the vault root, as the RE's
    /// `FY(...)` default-folder resolution also does.
    bool canCreateNewEntry() const;

    /// The seven in-view toolbar controls, as a name the host can bind a
    /// QAction to. `BasesViewActions` (which lives in the `src/` binary,
    /// deliberately not in this library) drives O5.T1's "one definition,
    /// four surfaces" by adopting its actions onto the buttons it already
    /// owns, rather than this library taking a dependency on the provider.
    enum class ToolbarSlot {
        Properties,
        SortGroup,
        Views,
        Filters,
        PropertiesPane,
        NewEntry,
        Results,
    };

    /// Adopt `action` as the definition of `slot`: the matching button
    /// becomes a `setDefaultAction()` carrier, so triggering the action from
    /// the menubar, the command palette, or the Hotkeys page runs exactly
    /// the code the button runs. Passing nullptr detaches and restores the
    /// button's standalone behaviour.
    ///
    /// Safe to call with no action (the standalone unit-test path, and any
    /// window that failed to build the provider): Results in particular
    /// falls back to a locally-built menu so the button is never left
    /// without one.
    void setToolbarAction(ToolbarSlot slot, QAction *action);

    /// The action currently adopted for `slot`, or nullptr. Exposed so the
    /// provider and tests can assert the binding rather than infer it from
    /// button appearance.
    QAction *toolbarAction(ToolbarSlot slot) const;

    /// The Results menu and its two children, owned by the view so the
    /// in-view button and the provider's menubar/palette entry share one
    /// menu object rather than two that can drift. Null before the ctor has
    /// built the toolbar, which cannot happen for a constructed view.
    QMenu *toolbarResultsMenu() const;
    QAction *toolbarCopyTableAction() const;
    QAction *toolbarExportCsvAction() const;

public Q_SLOTS:
    /// Single chokepoint: build a CmdSetFrontMatter and push it. Connected to
    /// BasesTreeModel::frontMatterEditRequested and
    /// PropertiesDrawer::frontMatterEditRequested.
    void pushFrontMatterEdit(Corbomite::TFile *file, const QString &key,
                             const QVariant &value);

    /// Cluster O Phase O5.T1 — the four plain-button toolbar verbs, moved up
    /// from private so `BasesViewActions` can drive them. Visibility change
    /// only; the bodies and every existing connection are untouched.
    void onCopyTable();
    void onExportCsv();
    void onNewItem();
    void openFiltersDialog();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

    /// Load the `.base` body from the NoteDocument and parse it. TextFileView's
    /// adapter-based loader is inert (no DataAdapter is ever injected), so —
    /// like MarkdownView — we take content straight from the document.
    void onLoadFile(Corbomite::NoteDocument *file) override;

private Q_SLOTS:
    void onHeaderClicked(int column);
    void onSearchChanged(const QString &text);
    void onViewSelectorChanged(const QString &name);
    void onSectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void onSelectionChanged();
    void onLinkClicked(const QString &target, Qt::KeyboardModifiers mods);
    void onContextMenu(const QPoint &pos);

private:
    QString resolveLink(const QString &target) const;   // wikilink target -> vault path ("" if unresolved)

    /// Resolve the newItemTemplate path to a (key,value) frontmatter list via
    /// the metadata cache. Empty if no template or no frontmatter.
    NewItemSeed::SeedList resolveTemplateProps() const;

    void rebuildLayout();
    void populateViewSelector();
    /// Read the raw `.base` bytes from the vault and parse them. No-op until
    /// both the vault (services) and the document (onLoadFile) are present.
    void loadBaseFromVault();
    void onConfigMutated();              // recompute + persist after a panel edit
    QVector<PropertyId> availableProperties() const;
    QString displayNameFor(const PropertyId &pid) const;
    void showPanelUnder(QWidget *panel, QToolButton *button);
    /// True when a popup panel was dismissed within the last 150 ms, i.e.
    /// this click is the tail of an outside-click dismissal of the very same
    /// panel and must be swallowed. Was inlined in three lambdas; hoisted so
    /// the KAction trigger path debounces identically (O5.T1) — the panels
    /// are anchored on the in-view buttons, which survive the switch to
    /// `setDefaultAction()`, so behaviour is unchanged whichever surface
    /// fired the action.
    bool panelDismissedRecently() const;

    QStringList summaryNamesForPicker() const;        ///< built-in defaults + custom summary names
    QStringList formulaCandidateList() const;
    void openFormulaDialog(const QString &editName);  ///< add (empty) or edit a named formula
    void openSummaryDialog(const PropertyId &prop);   ///< create/assign a custom summary

    Vault *m_vault = nullptr;
    MetadataCache *m_cache = nullptr;
    FileManager *m_fm = nullptr;
    FunctionRegistry *m_funcs = nullptr;

    std::shared_ptr<BasesQuery> m_query;
    BasesViewConfig *m_activeView = nullptr;

    std::unique_ptr<QueryController> m_controller;
    std::unique_ptr<BasesTreeModel> m_model;

    QUndoStack m_undoStack;

    QTreeView *m_table = nullptr;
    BasesCellDelegate *m_delegate = nullptr;
    QLabel *m_errorBanner = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_viewSelector = nullptr;
    QToolButton *m_propsBtn = nullptr;
    QToolButton *m_sortBtn = nullptr;
    QToolButton *m_viewsBtn = nullptr;
    QToolButton *m_filtersBtn = nullptr;
    PropertiesMenuPanel *m_propsPanel = nullptr;
    SortGroupMenuPanel *m_sortPanel = nullptr;
    ViewsMenuPanel *m_viewsPanel = nullptr;
    QToolButton *m_drawerBtn = nullptr;
    QToolButton *m_resultsBtn = nullptr;

    /// Actions adopted by BasesViewActions (O5.T1), keyed by ToolbarSlot.
    /// QPointer so an action destroyed on provider teardown (unbind before
    /// MainWindow's children are destroyed) drops out instead of dangling.
    QHash<int, QPointer<QAction>> m_toolbarActions;

    /// Standalone Results menu, kept so the button still has one when no
    /// provider is bound — `BasesView` is unit-tested without a MainWindow.
    QPointer<QMenu> m_fallbackResultsMenu;
    QPointer<QAction> m_copyTableAction;
    QPointer<QAction> m_exportCsvAction;
    QToolButton *m_newBtn = nullptr;
    QSplitter *m_splitter = nullptr;
    PropertiesDrawer *m_drawer = nullptr;

    QElapsedTimer m_panelDismissTimer;   // guards popup re-open flip-flop

    std::function<void(const QString &)> m_openInNewTab;
    std::function<void(const QString &)> m_searchTag;
    std::function<void(const QString &)> m_promptRename;
    std::function<void(const QString &)> m_promptDelete;
};

}  // namespace Corbomite::Bases
