// SPDX-License-Identifier: GPL-3.0-or-later
#include "QuickSwitcher.h"
#include "QuickSwitcherDelegate.h"
#include "corbomite/models/VaultModel.h"

#include <KFuzzyMatcher>
#include <KLocalizedString>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QSortFilterProxyModel>
#include <QGraphicsDropShadowEffect>
#include <QApplication>

namespace Corbomite {

// Custom proxy that uses KFuzzyMatcher for filtering and scoring
class FuzzyFilterProxyModel : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setFilterPattern(const QString &pattern)
    {
        m_pattern = pattern;
        invalidateFilter();
        sort(0); // Re-sort by score
    }

    QString filterPattern() const { return m_pattern; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        Q_UNUSED(sourceParent)
        if (m_pattern.isEmpty()) return true;

        auto idx = sourceModel()->index(sourceRow, 0);
        QString name = idx.data(QuickSwitcherModel::NoteNameRole).toString();
        return KFuzzyMatcher::matchSimple(m_pattern, name);
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        if (m_pattern.isEmpty()) {
            // No filter: recent files first, then alphabetical
            bool leftRecent = left.data(QuickSwitcherModel::IsRecentRole).toBool();
            bool rightRecent = right.data(QuickSwitcherModel::IsRecentRole).toBool();
            if (leftRecent != rightRecent) return leftRecent;
            return left.data(QuickSwitcherModel::NoteNameRole).toString()
                       .compare(right.data(QuickSwitcherModel::NoteNameRole).toString(),
                                Qt::CaseInsensitive) < 0;
        }

        // With filter: sort by fuzzy score (higher = better = first)
        QString leftName = left.data(QuickSwitcherModel::NoteNameRole).toString();
        QString rightName = right.data(QuickSwitcherModel::NoteNameRole).toString();
        auto leftResult = KFuzzyMatcher::match(m_pattern, leftName);
        auto rightResult = KFuzzyMatcher::match(m_pattern, rightName);
        return leftResult.score > rightResult.score;
    }

private:
    QString m_pattern;
};

QuickSwitcher::QuickSwitcher(VaultModel *vault, const QStringList &recentPaths,
                               QWidget *parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedWidth(600);
    setMaximumHeight(400);
    setFrameShape(QFrame::StyledPanel);

    // Drop shadow
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 80));
    setGraphicsEffect(shadow);

    // Layout
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // Search input
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(i18n("Open note..."));
    m_input->setClearButtonEnabled(true);
    QFont inputFont = m_input->font();
    inputFont.setPointSize(inputFont.pointSize() + 2);
    m_input->setFont(inputFont);
    layout->addWidget(m_input);

    // Results list
    m_resultList = new QTreeView(this);
    m_resultList->setHeaderHidden(true);
    m_resultList->setRootIsDecorated(false);
    m_resultList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultList->setFrameShape(QFrame::NoFrame);
    m_resultList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_resultList);

    // Source model
    m_sourceModel = new QuickSwitcherModel(this);
    m_sourceModel->setNotes(vault->allNotes());
    m_sourceModel->setRecentPaths(recentPaths);

    // Proxy model with fuzzy filtering
    m_proxyModel = new FuzzyFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_sourceModel);
    m_proxyModel->sort(0);

    m_resultList->setModel(m_proxyModel);

    // Delegate for match highlighting
    m_delegate = new QuickSwitcherDelegate(this);
    m_resultList->setItemDelegate(m_delegate);

    // Select first item
    if (m_proxyModel->rowCount() > 0) {
        m_resultList->setCurrentIndex(m_proxyModel->index(0, 0));
    }

    // Connect filter
    connect(m_input, &QLineEdit::textChanged, this, &QuickSwitcher::updateFilter);

    // Install event filter for keyboard navigation
    m_input->installEventFilter(this);
    m_resultList->installEventFilter(this);

    // Double-click to open
    connect(m_resultList, &QTreeView::doubleClicked, this, [this](const QModelIndex &) {
        activateSelected();
    });

    m_input->setFocus();
}

void QuickSwitcher::updateFilter(const QString &text)
{
    m_currentFilter = text;
    static_cast<FuzzyFilterProxyModel *>(m_proxyModel)->setFilterPattern(text);
    m_delegate->setFilterPattern(text);

    // Select first result
    if (m_proxyModel->rowCount() > 0) {
        m_resultList->setCurrentIndex(m_proxyModel->index(0, 0));
    }

    m_resultList->viewport()->update(); // Repaint with new highlights
}

void QuickSwitcher::activateSelected()
{
    auto current = m_resultList->currentIndex();
    if (current.isValid()) {
        QString path = current.data(QuickSwitcherModel::NotePathRole).toString();
        Q_EMIT noteSelected(path);
    } else if (!m_currentFilter.isEmpty()) {
        // No match — create new note with the typed name
        Q_EMIT createNoteRequested(m_currentFilter);
    }
    close();
}

void QuickSwitcher::selectNext()
{
    auto current = m_resultList->currentIndex();
    int nextRow = current.isValid() ? current.row() + 1 : 0;
    if (nextRow < m_proxyModel->rowCount()) {
        m_resultList->setCurrentIndex(m_proxyModel->index(nextRow, 0));
    }
}

void QuickSwitcher::selectPrevious()
{
    auto current = m_resultList->currentIndex();
    int prevRow = current.isValid() ? current.row() - 1 : 0;
    if (prevRow >= 0) {
        m_resultList->setCurrentIndex(m_proxyModel->index(prevRow, 0));
    }
}

bool QuickSwitcher::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Escape:
            close();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            activateSelected();
            return true;
        case Qt::Key_Down:
            selectNext();
            return true;
        case Qt::Key_Up:
            selectPrevious();
            return true;
        default:
            // If typing in the result list, redirect to input
            if (obj == m_resultList && keyEvent->text().length() > 0
                && !keyEvent->text().at(0).isSpace()) {
                m_input->setFocus();
                QApplication::sendEvent(m_input, event);
                return true;
            }
            break;
        }
    }
    return QFrame::eventFilter(obj, event);
}

} // namespace Corbomite
