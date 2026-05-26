// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

namespace Corbomite {

class MetadataCacheReader;
class SearchProxy;
class SearchResultsModel;
class WorkspaceController;

class SearchView : public QWidget
{
    Q_OBJECT
public:
    SearchView(SearchProxy *search,
               MetadataCacheReader *metadata,
               WorkspaceController *workspace,
               QWidget *parent = nullptr);

    void focusSearchInput();

    /// Set the query text and run it (debounced via the existing input path).
    /// Used by hosts that launch a pre-filled search, e.g. a Bases tag click.
    /// Q_INVOKABLE so the host can call it across the plugin .so boundary via
    /// QMetaObject::invokeMethod without linking the plugin's symbols.
    Q_INVOKABLE void setQuery(const QString &query);

private:
    void onSearchTextChanged(const QString &text);
    void executeSearch();
    void onResultClicked(const QModelIndex &index);
    void showOperatorHelp();

    QLineEdit *m_searchInput;
    QToolButton *m_helpButton;
    QTreeView *m_resultView;
    QLabel *m_statusLabel;
    QTimer m_debounceTimer;

    SearchProxy *m_search = nullptr;
    MetadataCacheReader *m_metadata = nullptr;
    WorkspaceController *m_workspace = nullptr;
    SearchResultsModel *m_resultsModel;
};

} // namespace Corbomite
