// SPDX-License-Identifier: GPL-3.0-or-later
#include "SearchView.h"

#include "SearchResultsDelegate.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/models/SearchResultsModel.h"
#include "corbomite/search/SearchDSL.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/storage/proxies/SearchProxy.h"

#include <KLocalizedString>
#include <QHBoxLayout>
#include <QIcon>
#include <QRegularExpression>
#include <QToolTip>
#include <QVBoxLayout>

namespace {

// Collect Text/Phrase literals that will end up as bare fts5Query terms —
// i.e. everything *except* operator operands (tag:/path:/file:/content:)
// and negated branches, which shouldn't be pulled into a case-sensitive
// post-filter just because the toolbar's "Match case" toggle is on.
void collectPlainTerms(const Corbomite::SearchNodePtr &node, QStringList &out)
{
    if (!node) return;
    using Kind = Corbomite::SearchNode::Kind;
    switch (node->kind) {
    case Kind::Text:
    case Kind::Phrase:
        if (!node->text.isEmpty()) out.append(node->text);
        return;
    case Kind::Not:
    case Kind::OpCall:
        return;
    default:
        for (const auto &c : node->children) collectPlainTerms(c, out);
        return;
    }
}

} // namespace

namespace Corbomite {

SearchDSL::CompiledPlan SearchView::planForQuery(const QString &query,
                                                 bool matchCase,
                                                 bool regex,
                                                 QString *error)
{
    if (error) error->clear();
    SearchDSL::CompiledPlan plan;

    if (regex) {
        QRegularExpression rx(query);
        if (!rx.isValid()) {
            if (error) *error = i18n("Failed to parse regular expression: %1", query);
            return plan;
        }
        if (!query.isEmpty()) plan.regexPatterns.append(query);
        return plan;
    }

    auto parsed = SearchDSL::parse(query);
    if (!parsed.error.isEmpty()) {
        if (error) *error = parsed.error;
        return plan;
    }
    if (!parsed.root) return plan;

    plan = SearchDSL::compile(parsed.root);
    if (matchCase) collectPlainTerms(parsed.root, plan.caseSensitiveTerms);
    return plan;
}

SearchView::SearchView(SearchProxy *search,
                        MetadataCacheReader *metadata,
                        WorkspaceController *workspace,
                        QWidget *parent)
    : QWidget(parent)
    , m_searchInput(new QLineEdit(this))
    , m_caseButton(new QToolButton(this))
    , m_regexButton(new QToolButton(this))
    , m_helpButton(new QToolButton(this))
    , m_resultView(new QTreeView(this))
    , m_statusLabel(new QLabel(this))
    , m_search(search)
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

    m_caseButton->setText(QStringLiteral("Aa"));
    m_caseButton->setToolTip(i18n("Match case"));
    m_caseButton->setCheckable(true);
    m_caseButton->setAutoRaise(true);
    inputRow->addWidget(m_caseButton);

    m_regexButton->setText(QStringLiteral(".*"));
    m_regexButton->setToolTip(i18n("Regex search"));
    m_regexButton->setCheckable(true);
    m_regexButton->setAutoRaise(true);
    inputRow->addWidget(m_regexButton);

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
    m_resultView->setItemDelegate(new SearchResultsDelegate(m_resultView));
    m_resultView->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_resultView);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(300);

    connect(m_searchInput, &QLineEdit::textChanged, this, &SearchView::onSearchTextChanged);
    connect(&m_debounceTimer, &QTimer::timeout, this, &SearchView::executeSearch);
    connect(m_resultView, &QTreeView::doubleClicked, this, &SearchView::onResultClicked);
    connect(m_helpButton, &QToolButton::clicked, this, &SearchView::showOperatorHelp);
    connect(m_caseButton, &QToolButton::toggled, this, [this](bool) {
        if (!m_searchInput->text().trimmed().isEmpty()) executeSearch();
    });
    connect(m_regexButton, &QToolButton::toggled, this, [this](bool) {
        if (!m_searchInput->text().trimmed().isEmpty()) executeSearch();
    });

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

void SearchView::setQuery(const QString &query)
{
    // setText drives textChanged -> onSearchTextChanged -> debounced executeSearch.
    m_searchInput->setText(query);
    m_searchInput->setFocus();
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
    if (!m_search) return;
    const QString query = m_searchInput->text().trimmed();
    if (query.isEmpty()) return;

    QString error;
    auto plan = planForQuery(query, m_caseButton->isChecked(), m_regexButton->isChecked(), &error);
    if (!error.isEmpty()) {
        m_resultsModel->clear();
        m_statusLabel->setText(i18n("Search error: %1", error));
        m_statusLabel->setVisible(true);
        return;
    }

    QVector<SearchMatch> results;
    QString unsupportedNote;
    const bool postFilter = !plan.regexPatterns.isEmpty()
                         || !plan.caseSensitiveTerms.isEmpty();
    const bool hasExclude = !plan.excludedFts5Query.isEmpty();
    if (plan.fts5Query.isEmpty() && plan.requiredTags.isEmpty()
        && plan.excludedTags.isEmpty() && !postFilter && !hasExclude) {
        results = m_search->search(query);
    } else if (hasExclude || postFilter) {
        results = m_search->searchCompiled(plan.fts5Query, plan.excludedFts5Query,
                                           plan.requiredTags, plan.excludedTags,
                                           plan.regexPatterns,
                                           plan.caseSensitiveTerms);
    } else {
        results = m_search->searchCompiled(plan.fts5Query, plan.requiredTags,
                                           plan.excludedTags);
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
        "Use the <b>Aa</b> / <b>.*</b> toolbar buttons for case-sensitive "
        "and regex search.<br>"
        "<i>line:, block:, section: coming soon</i>");
    QToolTip::showText(m_helpButton->mapToGlobal(QPoint(0, m_helpButton->height())),
                       text, m_helpButton);
}

} // namespace Corbomite
