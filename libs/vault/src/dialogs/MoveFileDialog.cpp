// SPDX-License-Identifier: GPL-3.0-or-later
#include "MoveFileDialog.h"

#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFolder.h"
#include "corbomite/vault/Vault.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Corbomite {

MoveFileDialog::MoveFileDialog(const TAbstractFile *file,
                               const Vault *vault,
                               QWidget *parent)
    : QDialog(parent), m_file(file), m_vault(vault)
{
    setWindowTitle(i18n("Move file"));
    resize(400, 360);

    auto *lay = new QVBoxLayout(this);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(i18n("Type folder to search"));
    lay->addWidget(m_filterEdit);

    m_listWidget = new QListWidget(this);
    lay->addWidget(m_listWidget, /*stretch=*/1);

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    lay->addWidget(m_buttonBox);
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &MoveFileDialog::onAccepted);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    populateFolderList();

    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &MoveFileDialog::onFilterChanged);
    connect(m_listWidget, &QListWidget::itemActivated,
            this, &MoveFileDialog::onItemActivated);
}

void MoveFileDialog::populateFolderList()
{
    m_allPaths.clear();
    if (!m_vault) return;

    // Source's current parent (the folder the file lives in) — excluded
    // so the list never offers "move to where you already are".
    QString sourceParent;
    if (m_file && m_file->parent)
        sourceParent = m_file->parent->path;

    // Root is always a valid destination (unless it IS the source parent).
    if (sourceParent != QStringLiteral("/"))
        m_allPaths.append(QStringLiteral("/"));

    // Every TFolder except the source's parent. Root is already handled
    // above so skip it during the walk too.
    for (const auto *f : m_vault->getAllLoadedFiles()) {
        const auto *folder = dynamic_cast<const TFolder *>(f);
        if (!folder) continue;
        if (folder->path == sourceParent) continue;
        if (folder->path == QStringLiteral("/")) continue;
        m_allPaths.append(folder->path);
    }
    m_allPaths.sort();

    for (const QString &p : m_allPaths)
        m_listWidget->addItem(p);
}

void MoveFileDialog::onFilterChanged(const QString &text)
{
    m_listWidget->clear();
    if (text.isEmpty()) {
        for (const QString &p : m_allPaths)
            m_listWidget->addItem(p);
        return;
    }
    // Substring match — deliberate MVP. FuzzyMatcher doesn't exist in this
    // tree yet; a dedicated ranker is a later follow-up.
    for (const QString &p : m_allPaths) {
        if (p.contains(text, Qt::CaseInsensitive))
            m_listWidget->addItem(p);
    }
}

void MoveFileDialog::setFilterText(const QString &filter)
{
    m_filterEdit->setText(filter);
}

void MoveFileDialog::onItemActivated()
{
    auto *item = m_listWidget->currentItem();
    if (!item) return;
    m_selection = item->text();
    accept();
}

void MoveFileDialog::onAccepted()
{
    if (auto *item = m_listWidget->currentItem())
        m_selection = item->text();
    accept();
}

QStringList MoveFileDialog::availableFolderPaths() const
{
    return m_allPaths;
}

QString MoveFileDialog::selectedFolderPath() const
{
    return m_selection;
}

}  // namespace Corbomite
