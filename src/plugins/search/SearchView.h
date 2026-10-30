// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

#include "corbomite/search/SearchDSL.h"

namespace Corbomite {

class IgnoreFilter;
class MetadataCacheReader;
class SearchProxy;
class SearchResultsModel;
struct SearchMatch;
class VaultProxy;
class WorkspaceController;

class SearchView : public QWidget
{
    Q_OBJECT
public:
    SearchView(SearchProxy *search,
               MetadataCacheReader *metadata,
               WorkspaceController *workspace,
               VaultProxy *vault,
               QWidget *parent = nullptr);

    void focusSearchInput();

    /// Set the query text and run it (debounced via the existing input path).
    /// Used by hosts that launch a pre-filled search, e.g. a Bases tag click.
    /// Q_INVOKABLE so the host can call it across the plugin .so boundary via
    /// QMetaObject::invokeMethod without linking the plugin's symbols.
    Q_INVOKABLE void setQuery(const QString &query);

    /// Build the compiled search plan for a raw query string given the
    /// "Match case" / "Regex" toolbar toggle state, without touching the
    /// storage backend. Pure/static so it's directly unit-testable and so
    /// executeSearch() has a single source of truth for the query -> plan
    /// translation. On a parse/regex error, returns an empty CompiledPlan
    /// and (if non-null) sets *error to a user-facing message.
    static SearchDSL::CompiledPlan planForQuery(const QString &query,
                                                bool matchCase,
                                                bool regex,
                                                QString *error = nullptr);

    /// Drop every match whose file path the filter ignores. Pure/static so
    /// it is directly unit-testable — same pattern as `planForQuery`.
    /// `userIgnoreFilters` is a search-results gate (`app/own/1951-e3.js:77`),
    /// not an index gate: ignored files stay parsed, linked and renameable.
    static QVector<SearchMatch> dropIgnoredPaths(
        const QVector<SearchMatch> &results, const IgnoreFilter &ignore);

private:
    void onSearchTextChanged(const QString &text);
    void executeSearch();
    void onResultClicked(const QModelIndex &index);
    void showOperatorHelp();

    /// Rebuild the ignore filter from `.obsidian/app.json`. Cheap; called
    /// per search so a config edit takes effect on the next query.
    IgnoreFilter ignoreFilter() const;

    QLineEdit *m_searchInput;
    QToolButton *m_caseButton;
    QToolButton *m_regexButton;
    QToolButton *m_helpButton;
    QTreeView *m_resultView;
    QLabel *m_statusLabel;
    QTimer m_debounceTimer;

    SearchProxy *m_search = nullptr;
    MetadataCacheReader *m_metadata = nullptr;
    WorkspaceController *m_workspace = nullptr;
    VaultProxy *m_vault = nullptr;
    SearchResultsModel *m_resultsModel;
};

} // namespace Corbomite
