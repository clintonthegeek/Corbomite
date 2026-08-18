// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompletionPopup.h"
#include "CompletionDelegate.h"

#include "corbomite/search/FuzzyMatcher.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QScrollBar>
#include <QVBoxLayout>

namespace Corbomite {

// Show up to this many rows before the list starts scrolling. Keeps the
// popup from swallowing the editor while still revealing multi-candidate
// sets (path-disambiguated names, headings, blocks).
static constexpr int kMaxVisibleRows = 10;

// Width clamps: grow to fit display + detail, but never absurdly wide.
static constexpr int kMinWidth = 240;
static constexpr int kMaxWidth = 640;

// Fuzzy proxy for completion — shares the Corbomite::FuzzyMatcher pipeline with
// QuickSwitcher and (eventually) every other suggester so ranking is uniform.
class CompletionFilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setFilterPattern(const QString &pattern)
    {
        m_pattern = pattern;
        m_prepared = FuzzyMatcher::prepareQuery(pattern);
// beginFilterChange landed in 6.9; endFilterChange only in 6.10. On 6.9.x
        // (Ubuntu 25.10) keep using invalidateFilter().
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        endFilterChange();
#else
        invalidateFilter();
#endif
        sort(0);
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        Q_UNUSED(sourceParent)
        if (m_prepared.isEmpty()) return true;
        auto idx = sourceModel()->index(sourceRow, 0);
        const QString text = idx.data(Qt::DisplayRole).toString();
        return FuzzyMatcher::fuzzySearch(m_prepared, text).has_value();
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        if (m_prepared.isEmpty()) {
            return left.data(Qt::DisplayRole).toString()
                       .compare(right.data(Qt::DisplayRole).toString(), Qt::CaseInsensitive) < 0;
        }
        const QString leftText = left.data(Qt::DisplayRole).toString();
        const QString rightText = right.data(Qt::DisplayRole).toString();
        const double l = FuzzyMatcher::fuzzySearch(m_prepared, leftText).value_or(FuzzyMatch{}).score;
        const double r = FuzzyMatcher::fuzzySearch(m_prepared, rightText).value_or(FuzzyMatch{}).score;
        return l > r;
    }

private:
    QString m_pattern;
    PreparedQuery m_prepared;
};

CompletionPopup::CompletionPopup(QAbstractItemModel *sourceModel, QWidget *parent)
    : QFrame(parent)
{
    Q_ASSERT_X(parent, "CompletionPopup",
               "Popup requires a real parent widget; it is intentionally "
               "NOT a top-level Qt::Popup window.");

    // Non-focus-stealing: keystrokes stay with the editor; the popup
    // is driven externally via selectNext/selectPrevious/acceptCurrent.
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // Both dimensions are content-driven (resizeToContents()): width grows
    // to fit the widest display + dim detail, height to the row count.
    setFrameShape(QFrame::StyledPanel);
    raise();

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(12);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(0, 0, 0, 60));
    setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    m_listView = new QListView(this);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setFocusPolicy(Qt::NoFocus);
    m_listView->viewport()->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_listView);

    m_proxyModel = new CompletionFilterProxy(this);
    m_proxyModel->setSourceModel(sourceModel);
    m_proxyModel->sort(0);
    m_listView->setModel(m_proxyModel);

    m_delegate = new CompletionDelegate(this);
    m_listView->setItemDelegate(m_delegate);

    // Mouse click → accept (editor never gets focus from this).
    connect(m_listView, &QListView::clicked, this, &CompletionPopup::onActivated);
}

void CompletionPopup::setFilterText(const QString &text)
{
    m_filterText = text;
    static_cast<CompletionFilterProxy *>(m_proxyModel)->setFilterPattern(text);
    m_delegate->setFilterPattern(text);

    if (m_proxyModel->rowCount() > 0) {
        m_listView->setCurrentIndex(m_proxyModel->index(0, 0));
    }

    resizeToContents();
    m_listView->viewport()->update();
}

int CompletionPopup::visibleRowCount() const
{
    return m_proxyModel->rowCount();
}

int CompletionPopup::contentHeight() const
{
    const int rows = qBound(1, m_proxyModel->rowCount(), kMaxVisibleRows);
    // Mirror CompletionDelegate::sizeHint (fontMetrics.height() + 8).
    const int rowH = QFontMetrics(m_listView->font()).height() + 8;
    const QMargins m = layout()->contentsMargins();
    return rows * rowH + frameWidth() * 2 + m.top() + m.bottom();
}

int CompletionPopup::contentWidth() const
{
    const QFontMetrics fm(m_listView->font());
    int widest = 0;
    for (int r = 0; r < m_proxyModel->rowCount(); ++r) {
        const QModelIndex idx = m_proxyModel->index(r, 0);
        widest = qMax(widest,
                      CompletionDelegate::rowNaturalWidth(
                          fm, idx.data(Qt::DisplayRole).toString(),
                          idx.data(Qt::UserRole + 2).toString()));
    }
    const QMargins m = layout()->contentsMargins();
    int chrome = frameWidth() * 2 + m.left() + m.right();
    if (m_proxyModel->rowCount() > kMaxVisibleRows)
        chrome += m_listView->verticalScrollBar()->sizeHint().width();
    return qBound(kMinWidth, widest + chrome, kMaxWidth);
}

void CompletionPopup::resizeToContents()
{
    const int w = contentWidth();
    const int h = contentHeight();
    setFixedSize(w, h);   // apply immediately so size()/positioning are correct pre-show
    // Force every row to the viewport width so the dim detail right-aligns
    // to a stable column instead of the row's own (ragged) natural edge.
    const QMargins m = layout()->contentsMargins();
    int viewportW = w - frameWidth() * 2 - m.left() - m.right();
    if (m_proxyModel->rowCount() > kMaxVisibleRows)
        viewportW -= m_listView->verticalScrollBar()->sizeHint().width();
    m_delegate->setRowWidth(viewportW);
    m_listView->doItemsLayout();   // pick up the new per-row width
    updateGeometry();
}

QSize CompletionPopup::sizeHint() const
{
    return QSize(contentWidth(), contentHeight());
}

void CompletionPopup::selectNext()
{
    auto current = m_listView->currentIndex();
    int next = current.isValid() ? current.row() + 1 : 0;
    if (next < m_proxyModel->rowCount()) {
        m_listView->setCurrentIndex(m_proxyModel->index(next, 0));
    }
}

void CompletionPopup::selectPrevious()
{
    auto current = m_listView->currentIndex();
    int prev = current.isValid() ? current.row() - 1 : 0;
    if (prev >= 0) {
        m_listView->setCurrentIndex(m_proxyModel->index(prev, 0));
    }
}

bool CompletionPopup::acceptCurrent()
{
    auto current = m_listView->currentIndex();
    if (!current.isValid()) return false;
    onActivated(current);
    return true;
}

QString CompletionPopup::selectedText() const
{
    auto current = m_listView->currentIndex();
    if (!current.isValid()) return {};
    return current.data(Qt::DisplayRole).toString();
}

QString CompletionPopup::selectedData() const
{
    auto current = m_listView->currentIndex();
    if (!current.isValid()) return {};
    return current.data(Qt::UserRole + 1).toString();
}

bool CompletionPopup::hasSelection() const
{
    return m_listView->currentIndex().isValid();
}

void CompletionPopup::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    raise();
    resizeToContents();
    if (m_proxyModel->rowCount() > 0) {
        m_listView->setCurrentIndex(m_proxyModel->index(0, 0));
    }
}

void CompletionPopup::hideEvent(QHideEvent *event)
{
    QFrame::hideEvent(event);
    Q_EMIT dismissed();
}

void CompletionPopup::onActivated(const QModelIndex &index)
{
    if (!index.isValid()) return;
    Q_EMIT itemSelected(index.data(Qt::DisplayRole).toString(),
                        index.data(Qt::UserRole + 1).toString());
    hide();
}

} // namespace Corbomite
