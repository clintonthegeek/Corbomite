// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesQuery.h"

#include "corbomite/core/TextFileView.h"

#include <memory>

class QLabel;
class QLineEdit;
class QComboBox;
class QTableView;

namespace Corbomite {
class FileManager;
class MetadataCache;
class Vault;
class WorkspaceLeaf;
}  // namespace Corbomite

namespace Corbomite::Bases {

class BasesCellDelegate;
class BasesTableModel;
class FunctionRegistry;
class QueryController;

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

private Q_SLOTS:
    void onHeaderClicked(int column);
    void onSearchChanged(const QString &text);
    void onViewSelectorChanged(const QString &name);

private:
    void rebuildLayout();
    void populateViewSelector();

    Vault *m_vault = nullptr;
    MetadataCache *m_cache = nullptr;
    FileManager *m_fm = nullptr;
    FunctionRegistry *m_funcs = nullptr;

    std::shared_ptr<BasesQuery> m_query;
    BasesViewConfig *m_activeView = nullptr;

    std::unique_ptr<QueryController> m_controller;
    std::unique_ptr<BasesTableModel> m_model;

    QTableView *m_table = nullptr;
    BasesCellDelegate *m_delegate = nullptr;
    QLabel *m_errorBanner = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_viewSelector = nullptr;
};

}  // namespace Corbomite::Bases
