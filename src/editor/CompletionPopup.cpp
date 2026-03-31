// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompletionPopup.h"
#include "CompletionDelegate.h"

#include <KFuzzyMatcher>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QApplication>

namespace Corbomite {

// Fuzzy proxy for completion
class CompletionFilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setFilterPattern(const QString &pattern)
    {
        m_pattern = pattern;
        beginFilterChange();
        endFilterChange();
        sort(0);
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        Q_UNUSED(sourceParent)
        if (m_pattern.isEmpty()) return true;

        auto idx = sourceModel()->index(sourceRow, 0);
        QString text = idx.data(Qt::DisplayRole).toString();
        return KFuzzyMatcher::matchSimple(m_pattern, text);
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        if (m_pattern.isEmpty()) {
            return left.data(Qt::DisplayRole).toString()
                       .compare(right.data(Qt::DisplayRole).toString(), Qt::CaseInsensitive) < 0;
        }
        QString leftText = left.data(Qt::DisplayRole).toString();
        QString rightText = right.data(Qt::DisplayRole).toString();
        return KFuzzyMatcher::match(m_pattern, leftText).score
             > KFuzzyMatcher::match(m_pattern, rightText).score;
    }

private:
    QString m_pattern;
};

CompletionPopup::CompletionPopup(QAbstractItemModel *sourceModel, QWidget *parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedWidth(300);
    setMaximumHeight(200);
    setFrameShape(QFrame::StyledPanel);

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
    layout->addWidget(m_listView);

    m_proxyModel = new CompletionFilterProxy(this);
    m_proxyModel->setSourceModel(sourceModel);
    m_proxyModel->sort(0);
    m_listView->setModel(m_proxyModel);

    m_delegate = new CompletionDelegate(this);
    m_listView->setItemDelegate(m_delegate);

    connect(m_listView, &QListView::activated, this, &CompletionPopup::onActivated);
}

void CompletionPopup::setFilterText(const QString &text)
{
    m_filterText = text;
    static_cast<CompletionFilterProxy *>(m_proxyModel)->setFilterPattern(text);
    m_delegate->setFilterPattern(text);

    if (m_proxyModel->rowCount() > 0) {
        m_listView->setCurrentIndex(m_proxyModel->index(0, 0));
    }

    m_listView->viewport()->update();

    // Auto-dismiss if no matches
    if (m_proxyModel->rowCount() == 0 && !text.isEmpty()) {
        // Keep open — user might still be typing
    }
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
    if (m_proxyModel->rowCount() > 0) {
        m_listView->setCurrentIndex(m_proxyModel->index(0, 0));
    }
}

void CompletionPopup::onActivated(const QModelIndex &index)
{
    if (!index.isValid()) return;
    Q_EMIT itemSelected(index.data(Qt::DisplayRole).toString(),
                        index.data(Qt::UserRole + 1).toString());
    close();
}

} // namespace Corbomite
