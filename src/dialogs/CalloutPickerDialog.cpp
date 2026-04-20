// SPDX-License-Identifier: GPL-3.0-or-later
#include "CalloutPickerDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <KLocalizedString>

namespace Corbomite {

namespace {

// Obsidian's 26 built-in callout types (canonical + aliases). Order
// follows the Obsidian help page so the first entry is the common
// default and aliases sit next to their canonical form.
static const char *const kCalloutTypes[] = {
    "note",
    "abstract", "summary", "tldr",
    "info",
    "todo",
    "tip", "hint", "important",
    "success", "check", "done",
    "question", "help", "faq",
    "warning", "caution", "attention",
    "failure", "fail", "missing",
    "danger", "error",
    "bug",
    "example",
    "quote",
};

} // namespace

CalloutPickerDialog::CalloutPickerDialog(QWidget *parent)
    : QDialog(parent)
    , m_combo(new QComboBox(this))
    , m_title(new QLineEdit(this))
    , m_preview(new QLabel(this))
{
    setWindowTitle(i18n("Insert Callout"));

    for (const char *type : kCalloutTypes)
        m_combo->addItem(QString::fromLatin1(type));

    auto *form = new QFormLayout;
    form->addRow(i18n("Type:"),  m_combo);
    form->addRow(i18n("Title:"), m_title);

    m_preview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_preview->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel(i18n("Preview:"), this));
    layout->addWidget(m_preview);
    layout->addWidget(buttons);

    connect(m_combo, &QComboBox::currentTextChanged,
            this, &CalloutPickerDialog::updatePreview);
    connect(m_title, &QLineEdit::textChanged,
            this, &CalloutPickerDialog::updatePreview);
    updatePreview();
}

QString CalloutPickerDialog::selectedType() const
{
    return m_combo->currentText();
}

QString CalloutPickerDialog::title() const
{
    return m_title->text();
}

void CalloutPickerDialog::updatePreview()
{
    const QString type = selectedType();
    const QString t = title();
    QString first = QStringLiteral("> [!%1]").arg(type);
    if (!t.isEmpty())
        first += QLatin1Char(' ') + t;
    m_preview->setText(first + QStringLiteral("\n> "));
}

} // namespace Corbomite
