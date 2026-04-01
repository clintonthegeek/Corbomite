// SPDX-License-Identifier: GPL-3.0-or-later
#include "CreateVaultDialog.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Corbomite {

CreateVaultDialog::CreateVaultDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Create New Vault"));
    setMinimumWidth(450);

    auto *layout = new QVBoxLayout(this);

    // Description
    auto *desc = new QLabel(i18n("A vault is a folder where your notes are stored."), this);
    desc->setWordWrap(true);
    layout->addWidget(desc);
    layout->addSpacing(8);

    // Form
    auto *form = new QFormLayout();

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(i18n("My Vault"));
    form->addRow(i18n("Vault name:"), m_nameEdit);

    // Location row: line edit + browse button
    auto *locationRow = new QHBoxLayout();
    m_locationEdit = new QLineEdit(this);
    m_locationEdit->setText(QDir::homePath() + QStringLiteral("/Documents"));
    m_locationEdit->setPlaceholderText(QDir::homePath());
    locationRow->addWidget(m_locationEdit);

    auto *browseButton = new QPushButton(i18n("Browse..."), this);
    connect(browseButton, &QPushButton::clicked, this, &CreateVaultDialog::browse);
    locationRow->addWidget(browseButton);

    form->addRow(i18n("Location:"), locationRow);

    layout->addLayout(form);
    layout->addSpacing(16);

    // Buttons
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    m_okButton->setText(i18n("Create"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // Enable/disable Create button based on name
    connect(m_nameEdit, &QLineEdit::textChanged, this, &CreateVaultDialog::updateOkButton);
    updateOkButton();

    m_nameEdit->setFocus();
}

QString CreateVaultDialog::vaultPath() const
{
    return m_locationEdit->text() + QDir::separator() + m_nameEdit->text();
}

void CreateVaultDialog::browse()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, i18n("Choose Location"), m_locationEdit->text());
    if (!dir.isEmpty()) {
        m_locationEdit->setText(dir);
    }
}

void CreateVaultDialog::updateOkButton()
{
    m_okButton->setEnabled(!m_nameEdit->text().trimmed().isEmpty());
}

} // namespace Corbomite
