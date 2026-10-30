// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QLineEdit>
#include <QTreeView>
#include "QuickSwitcherModel.h"

#include <QSortFilterProxyModel>

namespace Corbomite {

class QuickSwitcherDelegate;
class MetadataCacheReader;
class Vault;

class QuickSwitcher : public QFrame {
    Q_OBJECT

public:
    explicit QuickSwitcher(Vault *vault, const QStringList &recentPaths,
                           MetadataCacheReader *metadata = nullptr,
                           QWidget *parent = nullptr);

    // ---- test seams (the proxy is private; these drive the real path) --
    /// Set the filter exactly as a keystroke would (updateFilter path).
    void setFilterTextForTest(const QString &text) { updateFilter(text); }
    /// The matched rows' vault-relative paths in shell order (post
    /// filter+sort), i.e. what the user sees.
    QStringList visiblePathsForTest() const
    {
        QStringList out;
        for (int i = 0; i < m_proxyModel->rowCount(); ++i) {
            out << m_proxyModel->index(i, 0)
                       .data(QuickSwitcherModel::NotePathRole).toString();
        }
        return out;
    }

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
