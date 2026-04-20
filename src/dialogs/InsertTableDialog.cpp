// SPDX-License-Identifier: GPL-3.0-or-later
#include "InsertTableDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

#include <KLocalizedString>

namespace Corbomite {

InsertTableDialog::InsertTableDialog(QWidget *parent)
    : QDialog(parent)
    , m_rows(new QSpinBox(this))
    , m_cols(new QSpinBox(this))
    , m_header(new QCheckBox(i18n("First row is a header"), this))
{
    setWindowTitle(i18n("Insert Table"));

    m_rows->setRange(1, 20);
    m_rows->setValue(3);
    m_cols->setRange(1, 10);
    m_cols->setValue(3);
    m_header->setChecked(true);

    auto *form = new QFormLayout;
    form->addRow(i18n("Rows:"),    m_rows);
    form->addRow(i18n("Columns:"), m_cols);
    form->addRow(m_header);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

int InsertTableDialog::rows() const          { return m_rows->value(); }
int InsertTableDialog::cols() const          { return m_cols->value(); }
bool InsertTableDialog::firstRowAsHeader() const { return m_header->isChecked(); }

} // namespace Corbomite
