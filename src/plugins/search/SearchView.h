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
class SQLiteIndex;
class SearchResultsModel;
class WorkspaceController;

class SearchView : public QWidget
{
    Q_OBJECT
public:
    SearchView(SQLiteIndex *index,
               MetadataCacheReader *metadata,
               WorkspaceController *workspace,
               QWidget *parent = nullptr);

    void focusSearchInput();

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

    SQLiteIndex *m_index = nullptr;
    MetadataCacheReader *m_metadata = nullptr;
    WorkspaceController *m_workspace = nullptr;
    SearchResultsModel *m_resultsModel;
};

} // namespace Corbomite
