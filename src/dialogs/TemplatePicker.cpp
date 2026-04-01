// SPDX-License-Identifier: GPL-3.0-or-later
#include "TemplatePicker.h"

#include <KLocalizedString>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>

namespace Corbomite {

TemplatePicker::TemplatePicker(const QStringList &templates, QWidget *parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
{
    setWindowTitle(i18n("Insert Template"));
    resize(300, 400);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(i18n("Select a template:"), this));

    m_list->addItems(templates);
    if (!templates.isEmpty()) m_list->setCurrentRow(0);
    layout->addWidget(m_list);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
}

QString TemplatePicker::selectedTemplate() const
{
    auto *item = m_list->currentItem();
    return item ? item->text() : QString();
}

} // namespace Corbomite
