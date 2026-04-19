// SPDX-License-Identifier: GPL-3.0-or-later
#include "RenameDialog.h"

#include "../FileNameValidator.h"
#include "corbomite/vault/TAbstractFile.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Corbomite {

RenameDialog::RenameDialog(const TAbstractFile *file,
                           const Vault *vault,
                           QWidget *parent)
    : QDialog(parent), m_file(file), m_vault(vault)
{
    setWindowTitle(i18n("Rename"));

    auto *lay = new QVBoxLayout(this);

    auto *label = new QLabel(i18n("Enter new name:"), this);
    lay->addWidget(label);

    m_edit = new QLineEdit(this);
    if (file)
        m_edit->setText(file->name);
    lay->addWidget(m_edit);

    m_errorLabel = new QLabel(this);
    // Error styling — subtle, palette-aware so it renders sanely on any
    // theme. Not using a fixed colour; palette(link-visited) is reliably
    // distinct from the default text colour on both light and dark themes.
    m_errorLabel->setStyleSheet(QStringLiteral(
        "color: palette(link-visited);"));
    m_errorLabel->setWordWrap(true);
    lay->addWidget(m_errorLabel);

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    lay->addWidget(m_buttonBox);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_edit, &QLineEdit::textChanged,
            this, &RenameDialog::onTextChanged);

    // Pre-select basename (portion before the final '.').
    if (file && file->name.contains(QLatin1Char('.'))) {
        const int dotIdx = file->name.lastIndexOf(QLatin1Char('.'));
        m_edit->setSelection(0, dotIdx);
    } else {
        m_edit->selectAll();
    }

    // Initial validation pass so the Save button reflects the starting
    // state accurately (renaming to the original name is valid — the
    // validator treats the source file as its own non-collision).
    onTextChanged(m_edit->text());
}

void RenameDialog::onTextChanged(const QString &newText)
{
    const QString err = validateFileName(
        newText, m_file, m_vault, /*isFinal=*/false);
    m_errorLabel->setText(err);
    if (auto *save = m_buttonBox->button(QDialogButtonBox::Save))
        save->setEnabled(err.isEmpty());
}

bool RenameDialog::isSaveEnabled() const
{
    auto *save = m_buttonBox->button(QDialogButtonBox::Save);
    return save && save->isEnabled();
}

QString RenameDialog::proposedNewName() const
{
    return m_edit ? m_edit->text() : QString();
}

}  // namespace Corbomite
