// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QLabel;
class QLineEdit;
class QListWidget;
class QRadioButton;
class QStackedWidget;

namespace Corbomite {

/// "Merge entire file with…" modal (Cluster F-a P2), Obsidian's Note Composer.
/// Two stages in one dialog: a fuzzy file picker (all `.md` files except the
/// source) then an append/prepend confirmation carrying the destructive-
/// consequence and link-rewrite copy. It only *chooses*; the merge itself is
/// FileManager::mergeFileInto, driven by the host.
class MergeFileDialog : public QDialog
{
    Q_OBJECT
public:
    MergeFileDialog(QString sourceRelativePath,
                    QStringList candidatePaths,
                    QWidget *parent = nullptr);

    /// The chosen target (empty until a target is confirmed).
    QString selectedTarget() const { return m_selected; }
    /// True when prepend was chosen; false means append (the default).
    bool prependSelected() const;

private:
    void applyFilter(const QString &text);
    void showConfirmFor(const QString &target);

    QString m_source;
    QStringList m_candidates;
    QString m_selected;
    QLineEdit *m_filter = nullptr;
    QListWidget *m_list = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_confirmText = nullptr;
    QRadioButton *m_append = nullptr;
    QRadioButton *m_prepend = nullptr;
};

}  // namespace Corbomite
