// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QTreeView>
#include <QLabel>
#include <QTimer>

namespace Corbomite {

class SQLiteIndex;
class SearchResultsModel;

class SearchPanel : public QWidget {
    Q_OBJECT

public:
    explicit SearchPanel(QWidget *parent = nullptr);

    void setIndex(SQLiteIndex *index);
    void focusSearchInput();

Q_SIGNALS:
    void noteActivated(const QString &relativePath);

private:
    void onSearchTextChanged(const QString &text);
    void executeSearch();
    void onResultClicked(const QModelIndex &index);

    QLineEdit *m_searchInput;
    QTreeView *m_resultView;
    QLabel *m_statusLabel;
    QTimer m_debounceTimer;

    SQLiteIndex *m_index = nullptr;
    SearchResultsModel *m_resultsModel;
};

} // namespace Corbomite
