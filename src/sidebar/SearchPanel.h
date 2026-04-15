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
    void showOperatorHelp();

    QLineEdit *m_searchInput;
    QToolButton *m_helpButton;
    QTreeView *m_resultView;
    QLabel *m_statusLabel;
    QTimer m_debounceTimer;

    SQLiteIndex *m_index = nullptr;
    SearchResultsModel *m_resultsModel;
};

} // namespace Corbomite
