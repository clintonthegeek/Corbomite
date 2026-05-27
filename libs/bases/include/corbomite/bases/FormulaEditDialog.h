// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FormulaCandidates.h"

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QDialogButtonBox;

namespace Corbomite::Bases {

class FormulaInput;

/// Add/edit dialog for a named or summary formula. The owner supplies the
/// candidate list and the set of names already taken (excluding the one being
/// edited) for collision checking.
class FormulaEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FormulaEditDialog(FormulaCandidates::Mode mode, QWidget *parent = nullptr);

    void setCandidates(const QStringList &candidates);
    void setExistingNames(const QStringList &names);   ///< names that collide
    void setInitial(const QString &name, const QString &source);  ///< edit mode

    QString formulaName() const;
    QString formulaSource() const;

private Q_SLOTS:
    void updateOkState();

private:
    QLineEdit *m_name = nullptr;
    FormulaInput *m_input = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QStringList m_existing;
};

}  // namespace Corbomite::Bases
