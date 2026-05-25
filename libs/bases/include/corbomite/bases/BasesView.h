// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesQuery.h"

#include "corbomite/core/TextFileView.h"

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

private Q_SLOTS:
    void onHeaderClicked(int column);
    void onSearchChanged(const QString &text);
    void onViewSelectorChanged(const QString &name);
    void onSectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void onSelectionChanged();

private:
    void rebuildLayout();
    void populateViewSelector();
    void onConfigMutated();              // recompute + persist after a panel edit
    QVector<PropertyId> availableProperties() const;
    QString displayNameFor(const PropertyId &pid) const;
    void showPanelUnder(QWidget *panel, QToolButton *button);

    Vault *m_vault = nullptr;
    MetadataCache *m_cache = nullptr;
    FileManager *m_fm = nullptr;
    FunctionRegistry *m_funcs = nullptr;

    std::shared_ptr<BasesQuery> m_query;
    BasesViewConfig *m_activeView = nullptr;

    std::unique_ptr<QueryController> m_controller;
    std::unique_ptr<BasesTreeModel> m_model;

    QTreeView *m_table = nullptr;
    BasesCellDelegate *m_delegate = nullptr;
    QLabel *m_errorBanner = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_viewSelector = nullptr;
    QToolButton *m_propsBtn = nullptr;
    QToolButton *m_sortBtn = nullptr;
    QToolButton *m_viewsBtn = nullptr;
    PropertiesMenuPanel *m_propsPanel = nullptr;
    SortGroupMenuPanel *m_sortPanel = nullptr;
    ViewsMenuPanel *m_viewsPanel = nullptr;
    QToolButton *m_drawerBtn = nullptr;
    QSplitter *m_splitter = nullptr;
    PropertiesDrawer *m_drawer = nullptr;
};

}  // namespace Corbomite::Bases
