// SPDX-License-Identifier: GPL-3.0-or-later
#include "DeleteConfirmDialog.h"

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/vault/Vault.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Corbomite {

namespace {

// Read the current `[Files]/TrashOption` from the same KConfigXT store that
// the SettingsDialog writes to. Default matches corbomite.kcfg.
QString readTrashOption()
{
    KConfigGroup grp(KSharedConfig::openConfig(), QStringLiteral("Files"));
    return grp.readEntry(QStringLiteral("TrashOption"),
                         QStringLiteral("system"));
}

void writePromptDelete(bool enabled)
{
    KConfigGroup grp(KSharedConfig::openConfig(), QStringLiteral("Files"));
    grp.writeEntry(QStringLiteral("PromptDelete"), enabled);
    grp.sync();
}

}  // namespace

DeleteConfirmDialog::DeleteConfirmDialog(const TAbstractFile *file,
                                         Vault *vault,
                                         QWidget *parent)
    : QDialog(parent), m_file(file), m_vault(vault)
{
    const bool isFolder = dynamic_cast<const TFolder *>(file) != nullptr;
    setWindowTitle(isFolder ? i18n("Delete folder") : i18n("Delete file"));

    auto *main = new QVBoxLayout(this);

    auto *iconRow = new QHBoxLayout;
    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(
        QIcon::fromTheme(QStringLiteral("dialog-warning")).pixmap(48, 48));
    iconRow->addWidget(iconLabel);

    // Compose body text once at construction so bodyText() stays pure.
    {
        const QString trashOpt = readTrashOption();
        QString trashBlurb;
        if (trashOpt == QStringLiteral("system"))
            trashBlurb = i18n("It will be moved to your system trash.");
        else if (trashOpt == QStringLiteral("vault"))
            trashBlurb = i18n("It will be moved to Corbomite's trash folder.");
        else
            trashBlurb = i18n("It will be permanently deleted.");

        const QString name = file ? file->name : QString();
        if (isFolder) {
            m_body = i18n("Are you sure you want to delete folder \"%1\"?\n\n%2",
                          name, trashBlurb);
        } else {
            m_body = i18n("Are you sure you want to delete \"%1\"?\n\n%2",
                          name, trashBlurb);
        }
    }

    m_bodyLabel = new QLabel(m_body, this);
    m_bodyLabel->setWordWrap(true);
    iconRow->addWidget(m_bodyLabel, /*stretch=*/1);
    main->addLayout(iconRow);

    // Don't-ask-again — files only (folder deletions always prompt).
    if (!isFolder) {
        m_dontAsk = new QCheckBox(i18n("Don't ask again"), this);
        main->addWidget(m_dontAsk);
    }

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Yes, this);
    auto *deleteBtn = m_buttonBox->button(QDialogButtonBox::Yes);
    deleteBtn->setText(i18n("Delete"));
    deleteBtn->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    deleteBtn->setDefault(false);
    deleteBtn->setAutoDefault(false);

    auto *cancelBtn = m_buttonBox->button(QDialogButtonBox::Cancel);
    cancelBtn->setDefault(true);
    cancelBtn->setAutoDefault(true);

    main->addWidget(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

QString DeleteConfirmDialog::bodyText() const
{
    return m_body;
}

void DeleteConfirmDialog::setDontAskAgain(bool on)
{
    if (m_dontAsk) m_dontAsk->setChecked(on);
}

void DeleteConfirmDialog::accept()
{
    if (m_dontAsk && m_dontAsk->isChecked())
        writePromptDelete(false);
    QDialog::accept();
}

}  // namespace Corbomite
