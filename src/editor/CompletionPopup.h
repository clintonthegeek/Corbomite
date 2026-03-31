// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QListView>
#include <QSortFilterProxyModel>

class QAbstractItemModel;

namespace Corbomite {

class CompletionDelegate;

class CompletionPopup : public QFrame {
    Q_OBJECT

public:
    explicit CompletionPopup(QAbstractItemModel *sourceModel, QWidget *parent = nullptr);

    void setFilterText(const QString &text);
    void selectNext();
    void selectPrevious();
    QString selectedText() const;
    QString selectedData() const;
    bool hasSelection() const;

Q_SIGNALS:
    void itemSelected(const QString &text, const QString &data);
    void dismissed();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void onActivated(const QModelIndex &index);

    QListView *m_listView;
    QSortFilterProxyModel *m_proxyModel;
    CompletionDelegate *m_delegate;
    QString m_filterText;
};

} // namespace Corbomite
