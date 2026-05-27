// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FormulaEditDialog.h"

#include "corbomite/bases/FormulaInput.h"

#include <KLocalizedString>

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace Corbomite::Bases {

FormulaEditDialog::FormulaEditDialog(FormulaCandidates::Mode mode, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(mode == FormulaCandidates::Mode::SummaryFormula
                       ? i18n("Edit summary formula")
                       : i18n("Edit formula"));

    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_name = new QLineEdit(this);          // constructed before FormulaInput
    form->addRow(i18n("Name:"), m_name);

    m_input = new FormulaInput(this);
    form->addRow(i18n("Expression:"), m_input);
    root->addLayout(form);

    auto *help = new QLabel(
        i18n("<a href=\"https://help.obsidian.md/bases/functions\">Functions reference</a>"),
        this);
    help->setOpenExternalLinks(false);
    connect(help, &QLabel::linkActivated, this,
            [](const QString &url) { QDesktopServices::openUrl(QUrl(url)); });
    root->addWidget(help);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(m_buttons);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_name, &QLineEdit::textChanged, this, &FormulaEditDialog::updateOkState);
    connect(m_input, &FormulaInput::validityChanged, this, &FormulaEditDialog::updateOkState);

    updateOkState();
}

void FormulaEditDialog::setCandidates(const QStringList &candidates)
{
    m_input->setCandidates(candidates);
}

void FormulaEditDialog::setExistingNames(const QStringList &names)
{
    m_existing = names;
    updateOkState();
}

void FormulaEditDialog::setInitial(const QString &name, const QString &source)
{
    m_name->setText(name);
    m_input->setText(source);
    updateOkState();
}

QString FormulaEditDialog::formulaName() const { return m_name->text().trimmed(); }
QString FormulaEditDialog::formulaSource() const { return m_input->text(); }

void FormulaEditDialog::updateOkState()
{
    const QString name = m_name->text().trimmed();
    const bool ok = !name.isEmpty()
                    && !m_existing.contains(name)
                    && m_input->isExpressionValid()
                    && !m_input->text().trimmed().isEmpty();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

}  // namespace Corbomite::Bases
