// SPDX-License-Identifier: GPL-3.0-or-later
#include "SearchPanel.h"
#include "corbomite/models/SearchResultsModel.h"
#include "corbomite/search/SearchDSL.h"
#include "corbomite/storage/SQLiteIndex.h"

#include <KLocalizedString>
#include <QHBoxLayout>
#include <QIcon>
#include <QToolTip>
#include <QVBoxLayout>

namespace Corbomite {

SearchPanel::SearchPanel(QWidget *parent)
    : QWidget(parent)
    , m_searchInput(new QLineEdit(this))
    , m_helpButton(new QToolButton(this))
    , m_resultView(new QTreeView(this))
    , m_statusLabel(new QLabel(this))
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

    connect(m_searchInput, &QLineEdit::textChanged, this, &SearchPanel::onSearchTextChanged);
    connect(&m_debounceTimer, &QTimer::timeout, this, &SearchPanel::executeSearch);
    connect(m_resultView, &QTreeView::doubleClicked, this, &SearchPanel::onResultClicked);
    connect(m_helpButton, &QToolButton::clicked, this, &SearchPanel::showOperatorHelp);
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

    const QString query = m_searchInput->text().trimmed();
    if (query.isEmpty()) return;

    auto parsed = SearchDSL::parse(query);
    if (!parsed.error.isEmpty()) {
        m_resultsModel->clear();
        m_statusLabel->setText(i18n("Search error: %1", parsed.error));
        m_statusLabel->setVisible(true);
        return;
    }

    QVector<SearchMatch> results;
    QString unsupportedNote;

    if (!parsed.root) {
        // Empty parse — happens on whitespace-only inputs we already short-circuit.
        return;
    }

    auto plan = SearchDSL::compile(parsed.root);
    if (plan.fts5Query.isEmpty() && plan.requiredTags.isEmpty() && plan.excludedTags.isEmpty()) {
        // Parser produced an AST but compile dropped everything (e.g. only
        // unsupported operators). Fall back to literal-string FTS5 search so
        // the user still sees something useful.
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

void SearchPanel::onResultClicked(const QModelIndex &index)
{
    QString path = index.data(SearchResultsModel::NotePathRole).toString();
    if (!path.isEmpty()) {
        Q_EMIT noteActivated(path);
    }
}

void SearchPanel::showOperatorHelp()
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
