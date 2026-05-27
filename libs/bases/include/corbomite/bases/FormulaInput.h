// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QLineEdit>
#include <QStringList>

class QAction;
class QCompleter;
class QStringListModel;

namespace Corbomite::Bases {

/// Single-line expression editor with live parse-level validation (trailing
/// valid/invalid indicator + error tooltip) and flat token autocomplete.
/// Validation mirrors Obsidian's green-check: a transient Formula is re-parsed
/// on every keystroke; runtime (evaluation) errors are not reflected here.
class FormulaInput : public QLineEdit
{
    Q_OBJECT
public:
    explicit FormulaInput(QWidget *parent = nullptr);

    bool isExpressionValid() const { return m_valid; }

    /// Replace the autocomplete candidate tokens.
    void setCandidates(const QStringList &candidates);

Q_SIGNALS:
    void validityChanged(bool valid);

private Q_SLOTS:
    void revalidate();
    void onCompletionActivated(const QString &completion);
    void maybePopupCompleter();

private:
    QAction *m_indicator = nullptr;
    QCompleter *m_completer = nullptr;
    QStringListModel *m_candModel = nullptr;
    bool m_valid = true;   // empty == valid (neutral)
};

}  // namespace Corbomite::Bases
