// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QLineEdit>
#include <QTreeView>
#include "QuickSwitcherModel.h"

class QSortFilterProxyModel;

namespace Corbomite {

class QuickSwitcherDelegate;
class Vault;

class QuickSwitcher : public QFrame {
    Q_OBJECT

public:
    explicit QuickSwitcher(Vault *vault, const QStringList &recentPaths,
                           QWidget *parent = nullptr);

Q_SIGNALS:
    void noteSelected(const QString &relativePath);
    void createNoteRequested(const QString &name);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updateFilter(const QString &text);
    void activateSelected();
    void selectNext();
    void selectPrevious();

    QLineEdit *m_input;
    QTreeView *m_resultList;
    QuickSwitcherModel *m_sourceModel;
    QSortFilterProxyModel *m_proxyModel;
    QuickSwitcherDelegate *m_delegate;
    QString m_currentFilter;
    // Future: accept alias data for frontmatter alias matching
};

} // namespace Corbomite
