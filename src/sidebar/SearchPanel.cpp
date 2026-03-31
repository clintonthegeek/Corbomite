// SPDX-License-Identifier: GPL-3.0-or-later
#include "SearchPanel.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/models/SearchResultsModel.h"

#include <KLocalizedString>
#include <QVBoxLayout>

namespace Corbomite {

SearchPanel::SearchPanel(QWidget *parent)
    : QWidget(parent)
    , m_searchInput(new QLineEdit(this))
    , m_resultView(new QTreeView(this))
    , m_statusLabel(new QLabel(this))
    , m_resultsModel(new SearchResultsModel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_searchInput->setPlaceholderText(i18n("Search vault..."));
    m_searchInput->setClearButtonEnabled(true);
    layout->addWidget(m_searchInput);

    m_statusLabel->setVisible(false);
    layout->addWidget(m_statusLabel);

    m_resultView->setHeaderHidden(true);
    m_resultView->setRootIsDecorated(true);
    m_resultView->setModel(m_resultsModel);
    m_resultView->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_resultView);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(300);

    connect(m_searchInput, &QLineEdit::textChanged, this, &SearchPanel::onSearchTextChanged);
    connect(&m_debounceTimer, &QTimer::timeout, this, &SearchPanel::executeSearch);
    connect(m_resultView, &QTreeView::doubleClicked, this, &SearchPanel::onResultClicked);
}

void SearchPanel::setIndex(SQLiteIndex *index)
{
    m_index = index;
}

void SearchPanel::focusSearchInput()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

void SearchPanel::onSearchTextChanged(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        m_resultsModel->clear();
        m_statusLabel->setVisible(false);
        return;
    }
    m_debounceTimer.start();
}

void SearchPanel::executeSearch()
{
    if (!m_index) return;

    QString query = m_searchInput->text().trimmed();
    if (query.isEmpty()) return;

    // TODO: Support Obsidian search operators (file:, path:, tag:, regex)
    auto results = m_index->search(query);
    m_resultsModel->setResults(results);

    m_statusLabel->setText(i18n("%1 matches in %2 files",
                                m_resultsModel->totalMatchCount(),
                                m_resultsModel->fileCount()));
    m_statusLabel->setVisible(true);

    m_resultView->expandAll();
}

void SearchPanel::onResultClicked(const QModelIndex &index)
{
    QString path = index.data(SearchResultsModel::NotePathRole).toString();
    if (!path.isEmpty()) {
        Q_EMIT noteActivated(path);
    }
}

} // namespace Corbomite
