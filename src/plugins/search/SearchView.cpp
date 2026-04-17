// SPDX-License-Identifier: GPL-3.0-or-later
#include "SearchView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/models/SearchResultsModel.h"
#include "corbomite/search/SearchDSL.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"

#include <KLocalizedString>
#include <QHBoxLayout>
#include <QIcon>
#include <QToolTip>
#include <QVBoxLayout>

namespace Corbomite {

SearchView::SearchView(SQLiteIndex *index,
                        MetadataCacheReader *metadata,
                        WorkspaceController *workspace,
                        QWidget *parent)
    : QWidget(parent)
    , m_searchInput(new QLineEdit(this))
    , m_helpButton(new QToolButton(this))
    , m_resultView(new QTreeView(this))
    , m_statusLabel(new QLabel(this))
    , m_index(index)
    , m_metadata(metadata)
    , m_workspace(workspace)
    , m_resultsModel(new SearchResultsModel(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *inputRow = new QHBoxLayout;
    inputRow->setContentsMargins(0, 0, 0, 0);
    inputRow->setSpacing(4);

    m_searchInput->setPlaceholderText(i18n("Search vault... (try tag:#topic)"));
    m_searchInput->setClearButtonEnabled(true);
    inputRow->addWidget(m_searchInput);

    m_helpButton->setIcon(QIcon::fromTheme(QStringLiteral("help-contextual")));
    m_helpButton->setText(QStringLiteral("?"));
    m_helpButton->setToolTip(i18n("Show search operators"));
    m_helpButton->setAutoRaise(true);
    inputRow->addWidget(m_helpButton);

    layout->addLayout(inputRow);

    m_statusLabel->setVisible(false);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_resultView->setHeaderHidden(true);
    m_resultView->setRootIsDecorated(true);
    m_resultView->setModel(m_resultsModel);
    m_resultView->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_resultView);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(300);

    connect(m_searchInput, &QLineEdit::textChanged, this, &SearchView::onSearchTextChanged);
    connect(&m_debounceTimer, &QTimer::timeout, this, &SearchView::executeSearch);
    connect(m_resultView, &QTreeView::doubleClicked, this, &SearchView::onResultClicked);
    connect(m_helpButton, &QToolButton::clicked, this, &SearchView::showOperatorHelp);

    if (m_metadata) {
        connect(m_metadata, &MetadataCacheReader::indexFinished, this, [this]() {
            if (!m_searchInput->text().trimmed().isEmpty()) executeSearch();
        });
    }
}

void SearchView::focusSearchInput()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

void SearchView::onSearchTextChanged(const QString &text)
{
    if (text.trimmed().isEmpty()) {
        m_resultsModel->clear();
        m_statusLabel->setVisible(false);
        return;
    }
    m_debounceTimer.start();
}

void SearchView::executeSearch()
{
    if (!m_index) return;
    const QString query = m_searchInput->text().trimmed();
    if (query.isEmpty()) return;

    auto parsed = SearchDSL::parse(query);
    if (!parsed.error.isEmpty()) {
        m_resultsModel->clear();
        m_statusLabel->setText(i18n("Search error: %1", parsed.error));
        m_statusLabel->setVisible(true);
        return;
    }
    if (!parsed.root) return;

    QVector<SearchMatch> results;
    QString unsupportedNote;
    auto plan = SearchDSL::compile(parsed.root);
    if (plan.fts5Query.isEmpty() && plan.requiredTags.isEmpty() && plan.excludedTags.isEmpty()) {
        results = m_index->search(query);
    } else {
        results = m_index->searchCompiled(plan.fts5Query, plan.requiredTags, plan.excludedTags);
    }
    if (!plan.unsupported.isEmpty()) {
        unsupportedNote = i18n(" (unsupported: %1)", plan.unsupported.join(QStringLiteral(", ")));
    }
    m_resultsModel->setResults(results);
    m_statusLabel->setText(i18n("%1 matches in %2 files%3",
                                m_resultsModel->totalMatchCount(),
                                m_resultsModel->fileCount(),
                                unsupportedNote));
    m_statusLabel->setVisible(true);
    m_resultView->expandAll();
}

void SearchView::onResultClicked(const QModelIndex &index)
{
    const QString path = index.data(SearchResultsModel::NotePathRole).toString();
    if (!path.isEmpty() && m_workspace) m_workspace->openFile(path);
}

void SearchView::showOperatorHelp()
{
    const QString text = i18n(
        "<b>Search operators</b><br>"
        "<code>tag:#name</code> — only notes tagged with #name<br>"
        "<code>path:folder</code> — restrict by file path<br>"
        "<code>file:name</code> — restrict by file name<br>"
        "<code>content:word</code> — only match in body content<br>"
        "<code>\"phrase\"</code> — exact phrase<br>"
        "<code>-word</code> — exclude word<br>"
        "<code>foo OR bar</code> — either match (OR is uppercase)<br>"
        "<code>(a OR b) c</code> — grouping<br>"
        "<i>regex, line:, block:, section: coming soon</i>");
    QToolTip::showText(m_helpButton->mapToGlobal(QPoint(0, m_helpButton->height())),
                       text, m_helpButton);
}

} // namespace Corbomite
