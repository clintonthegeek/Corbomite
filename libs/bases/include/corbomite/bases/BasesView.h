// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesQuery.h"
#include "corbomite/bases/NewItemSeed.h"

#include "corbomite/core/TextFileView.h"

#include <QElapsedTimer>
#include <QPoint>
#include <QString>
#include <QUndoStack>

#include <functional>
#include <memory>

class QLabel;
class QLineEdit;
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

    /// Assign (or clear) a summary function for `prop` in the active view.
    /// Public so tests can exercise the mutation without spawning a dialog.
    void applySummaryChoice(const PropertyId &prop, const QString &fnName);

    /// Replace both filter scopes from edited specs, then recompute + persist.
    /// Public for testability (the dialog path calls this on accept).
    void applyFilterSpecs(const FilterSpec &globalSpec, const FilterSpec &perViewSpec);

public Q_SLOTS:
    /// Single chokepoint: build a CmdSetFrontMatter and push it. Connected to
    /// BasesTreeModel::frontMatterEditRequested and
    /// PropertiesDrawer::frontMatterEditRequested.
    void pushFrontMatterEdit(Corbomite::TFile *file, const QString &key,
                             const QVariant &value);

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
    void onCopyTable();
    void onExportCsv();
    void onNewItem();

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

    QStringList summaryNamesForPicker() const;        ///< built-in defaults + custom summary names
    QStringList formulaCandidateList() const;
    void openFormulaDialog(const QString &editName);  ///< add (empty) or edit a named formula
    void openSummaryDialog(const PropertyId &prop);   ///< create/assign a custom summary
    void openFiltersDialog();

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
