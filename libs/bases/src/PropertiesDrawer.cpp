// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/PropertiesDrawer.h"

#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/PropertyId.h"
#include "corbomite/bases/Values.h"

#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/TFile.h"

#include <KLocalizedString>

#include <QCheckBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVariantMap>

namespace Corbomite::Bases {

PropertiesDrawer::PropertiesDrawer(QWidget *parent) : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_title = new QLabel(i18n("(no selection)"), this);
    QFont f = m_title->font(); f.setBold(true); m_title->setFont(f);
    root->addWidget(m_title);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *formHost = new QWidget(scroll);
    m_form = new QFormLayout(formHost);
    scroll->setWidget(formHost);
    root->addWidget(scroll, 1);

    auto *addField = new QPushButton(i18n("+ Add field"), this);
    root->addWidget(addField);
    connect(addField, &QPushButton::clicked, this, [this]() {
        if (!m_file) return;
        bool ok = false;
        const QString key = QInputDialog::getText(this, i18n("Add field"),
            i18n("Property name:"), QLineEdit::Normal, QString{}, &ok);
        if (ok && !key.isEmpty()) commit(key, QString{});
    });
}

void PropertiesDrawer::clearForm()
{
    while (m_form->rowCount() > 0) m_form->removeRow(0);
}

void PropertiesDrawer::showEntry(BasesEntry *entry)
{
    clearForm();
    m_file = entry ? entry->file() : nullptr;
    if (!entry || !m_file) {
        m_title->setText(i18n("(no selection)"));
        return;
    }
    m_title->setText(m_file->path);

    const QStringList keys = entry->getPropertyKeys();
    for (const QString &key : keys) {
        const PropertyId pid{PropertyKind::Note, key};
        const ValuePtr v = entry->getValue(pid);
        const QString type = v ? v->type() : QStringLiteral("String");

        if (type == QLatin1String("Boolean")) {
            auto *cb = new QCheckBox(this);
            auto *b = dynamic_cast<BooleanValue *>(v.get());
            cb->setChecked(b && b->data());
            connect(cb, &QCheckBox::toggled, this, [this, key](bool on) { commit(key, on); });
            m_form->addRow(key, cb);
        } else if (type == QLatin1String("Number")) {
            auto *sb = new QDoubleSpinBox(this);
            sb->setDecimals(6); sb->setRange(-1e15, 1e15);
            auto *n = dynamic_cast<NumberValue *>(v.get());
            sb->setValue(n ? n->data() : 0.0);
            connect(sb, &QDoubleSpinBox::editingFinished, this,
                    [this, key, sb]() { commit(key, sb->value()); });
            m_form->addRow(key, sb);
        } else if (type == QLatin1String("Date")) {
            auto *de = new QDateEdit(this);
            de->setCalendarPopup(true);
            if (auto *d = dynamic_cast<DateValue *>(v.get())) de->setDate(d->dateTime().date());
            connect(de, &QDateEdit::editingFinished, this,
                    [this, key, de]() { commit(key, de->date().toString(Qt::ISODate)); });
            m_form->addRow(key, de);
        } else {
            auto *le = new QLineEdit(this);
            le->setText(v ? v->toString() : QString{});
            connect(le, &QLineEdit::editingFinished, this,
                    [this, key, le]() { commit(key, le->text()); });
            m_form->addRow(key, le);
        }
    }
}

void PropertiesDrawer::commit(const QString &key, const QVariant &value)
{
    if (!m_fm || !m_file) return;
    // processFrontMatter is synchronous: the lambda runs immediately, so the
    // by-reference captures are valid for its full duration.
    m_fm->processFrontMatter(m_file, [&](QVariantMap &fm) { fm.insert(key, value); });
}

}  // namespace Corbomite::Bases
