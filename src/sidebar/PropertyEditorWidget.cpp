// SPDX-License-Identifier: GPL-3.0-or-later
#include "PropertyEditorWidget.h"

#include <markoff/parser/YamlValue.h>

#include <QCheckBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Corbomite {

// --- Base ---------------------------------------------------------------

PropertyEditorWidget::PropertyEditorWidget(QWidget *parent)
    : QWidget(parent)
{
}

// --- TextPropertyEditor -------------------------------------------------

TextPropertyEditor::TextPropertyEditor(QWidget *parent)
    : PropertyEditorWidget(parent)
    , m_edit(new QLineEdit(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_edit);
    connect(m_edit, &QLineEdit::textEdited,
            this, &PropertyEditorWidget::valueChanged);
}

void TextPropertyEditor::setValue(const Markoff::YamlValue &value)
{
    QSignalBlocker blocker(m_edit);
    if (value.isString()) {
        m_edit->setText(value.asString());
    } else if (value.isBool()) {
        m_edit->setText(value.asBool() ? QStringLiteral("true") : QStringLiteral("false"));
    } else if (value.isInt()) {
        m_edit->setText(QString::number(value.asInt()));
    } else if (value.isDouble()) {
        m_edit->setText(QString::number(value.asDouble()));
    } else if (value.isNull()) {
        m_edit->clear();
    } else {
        // Fallback: stringify
        m_edit->setText(value.asString());
    }
}

Markoff::YamlValue TextPropertyEditor::currentValue() const
{
    auto root = Markoff::YamlValue::emptyMap();
    root.setString(QStringLiteral("_"), m_edit->text());
    return root.get(QStringLiteral("_"));
}

// --- NumberPropertyEditor -----------------------------------------------

NumberPropertyEditor::NumberPropertyEditor(QWidget *parent)
    : PropertyEditorWidget(parent)
    , m_spin(new QDoubleSpinBox(this))
{
    m_spin->setDecimals(6);
    m_spin->setRange(-1e15, 1e15);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_spin);
    connect(m_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PropertyEditorWidget::valueChanged);
}

void NumberPropertyEditor::setValue(const Markoff::YamlValue &value)
{
    QSignalBlocker blocker(m_spin);
    if (value.isInt()) {
        m_isInt = true;
        m_spin->setDecimals(0);
        m_spin->setValue(static_cast<double>(value.asInt()));
    } else if (value.isDouble()) {
        m_isInt = false;
        m_spin->setDecimals(6);
        m_spin->setValue(value.asDouble());
    } else if (value.isString()) {
        // Try to parse string as number
        bool ok = false;
        const QString s = value.asString();
        double d = s.toDouble(&ok);
        if (ok) {
            m_isInt = !s.contains(QLatin1Char('.'));
            m_spin->setDecimals(m_isInt ? 0 : 6);
            m_spin->setValue(d);
        } else {
            m_spin->setValue(0.0);
        }
    } else {
        m_spin->setValue(0.0);
    }
}

Markoff::YamlValue NumberPropertyEditor::currentValue() const
{
    auto root = Markoff::YamlValue::emptyMap();
    const double v = m_spin->value();
    if (m_isInt || m_spin->decimals() == 0 || v == static_cast<double>(static_cast<int64_t>(v))) {
        root.setInt(QStringLiteral("_"), static_cast<int64_t>(v));
    } else {
        root.setDouble(QStringLiteral("_"), v);
    }
    return root.get(QStringLiteral("_"));
}

// --- CheckboxPropertyEditor ---------------------------------------------

CheckboxPropertyEditor::CheckboxPropertyEditor(QWidget *parent)
    : PropertyEditorWidget(parent)
    , m_check(new QCheckBox(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_check);
    connect(m_check, &QCheckBox::toggled,
            this, &PropertyEditorWidget::valueChanged);
}

void CheckboxPropertyEditor::setValue(const Markoff::YamlValue &value)
{
    QSignalBlocker blocker(m_check);
    if (value.isBool()) {
        m_check->setChecked(value.asBool());
    } else if (value.isString()) {
        const QString s = value.asString().toLower();
        m_check->setChecked(s == QStringLiteral("true") || s == QStringLiteral("yes"));
    } else {
        m_check->setChecked(false);
    }
}

Markoff::YamlValue CheckboxPropertyEditor::currentValue() const
{
    auto root = Markoff::YamlValue::emptyMap();
    root.setBool(QStringLiteral("_"), m_check->isChecked());
    return root.get(QStringLiteral("_"));
}

// --- DatePropertyEditor -------------------------------------------------

DatePropertyEditor::DatePropertyEditor(QWidget *parent)
    : PropertyEditorWidget(parent)
    , m_date(new QDateEdit(this))
{
    m_date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_date->setCalendarPopup(true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_date);
    connect(m_date, &QDateEdit::dateChanged,
            this, &PropertyEditorWidget::valueChanged);
}

void DatePropertyEditor::setValue(const Markoff::YamlValue &value)
{
    QSignalBlocker blocker(m_date);
    if (value.isString()) {
        const QDate d = QDate::fromString(value.asString(), Qt::ISODate);
        if (d.isValid()) {
            m_date->setDate(d);
            return;
        }
    }
    m_date->setDate(QDate::currentDate());
}

Markoff::YamlValue DatePropertyEditor::currentValue() const
{
    auto root = Markoff::YamlValue::emptyMap();
    root.setString(QStringLiteral("_"),
                   m_date->date().toString(Qt::ISODate));
    return root.get(QStringLiteral("_"));
}

// --- DateTimePropertyEditor ---------------------------------------------

DateTimePropertyEditor::DateTimePropertyEditor(QWidget *parent)
    : PropertyEditorWidget(parent)
    , m_dt(new QDateTimeEdit(this))
{
    m_dt->setDisplayFormat(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    m_dt->setCalendarPopup(true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_dt);
    connect(m_dt, &QDateTimeEdit::dateTimeChanged,
            this, &PropertyEditorWidget::valueChanged);
}

void DateTimePropertyEditor::setValue(const Markoff::YamlValue &value)
{
    QSignalBlocker blocker(m_dt);
    if (value.isString()) {
        QDateTime dt = QDateTime::fromString(value.asString(), Qt::ISODate);
        if (!dt.isValid()) {
            dt = QDateTime::fromString(value.asString(), Qt::ISODateWithMs);
        }
        if (dt.isValid()) {
            m_dt->setDateTime(dt);
            return;
        }
    }
    m_dt->setDateTime(QDateTime::currentDateTime());
}

Markoff::YamlValue DateTimePropertyEditor::currentValue() const
{
    auto root = Markoff::YamlValue::emptyMap();
    // Emit without milliseconds by default (matches Obsidian).
    root.setString(QStringLiteral("_"),
                   m_dt->dateTime().toString(Qt::ISODate));
    return root.get(QStringLiteral("_"));
}

// --- ListPropertyEditor -------------------------------------------------

ListPropertyEditor::ListPropertyEditor(QWidget *parent)
    : PropertyEditorWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);

    m_rowsContainer = new QWidget(this);
    m_rowsLayout = new QVBoxLayout(m_rowsContainer);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(2);
    outer->addWidget(m_rowsContainer);

    auto *addButton = new QPushButton(tr("+ Add item"), this);
    connect(addButton, &QPushButton::clicked, this, [this] {
        addRow();
        Q_EMIT valueChanged();
    });
    outer->addWidget(addButton);
}

void ListPropertyEditor::addRow(const QString &initial)
{
    auto *row = new QWidget(m_rowsContainer);
    auto *hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(2);

    auto *edit = new QLineEdit(initial, row);
    auto *rm = new QPushButton(QStringLiteral("-"), row);
    rm->setFixedWidth(24);

    hl->addWidget(edit);
    hl->addWidget(rm);
    m_rowsLayout->addWidget(row);

    m_edits.append(edit);
    m_rows.append(row);

    connect(edit, &QLineEdit::textEdited,
            this, &PropertyEditorWidget::valueChanged);
    connect(rm, &QPushButton::clicked, this, [this, row] {
        removeRow(row);
        Q_EMIT valueChanged();
    });
}

void ListPropertyEditor::removeRow(QWidget *row)
{
    const int idx = m_rows.indexOf(row);
    if (idx < 0) return;
    m_rows.removeAt(idx);
    m_edits.removeAt(idx);
    row->setParent(nullptr);
    row->deleteLater();
}

void ListPropertyEditor::setValue(const Markoff::YamlValue &value)
{
    // Clear all rows.
    for (auto *r : m_rows) {
        r->setParent(nullptr);
        r->deleteLater();
    }
    m_rows.clear();
    m_edits.clear();

    if (!value.isSeq()) return;
    const int n = value.size();
    for (int i = 0; i < n; ++i) {
        const auto item = value.at(i);
        QString s;
        if (item.isString()) s = item.asString();
        else if (item.isBool()) s = item.asBool() ? QStringLiteral("true") : QStringLiteral("false");
        else if (item.isInt()) s = QString::number(item.asInt());
        else if (item.isDouble()) s = QString::number(item.asDouble());
        addRow(s);
    }
}

Markoff::YamlValue ListPropertyEditor::currentValue() const
{
    auto root = Markoff::YamlValue::emptyMap();
    QStringList values;
    values.reserve(m_edits.size());
    for (auto *e : m_edits) {
        values << e->text();
    }
    root.setSeq(QStringLiteral("_"), values);
    return root.get(QStringLiteral("_"));
}

// --- Factory ------------------------------------------------------------

PropertyEditorWidget *makePropertyEditor(PropertyType type,
                                         const Markoff::YamlValue &initialValue,
                                         QWidget *parent)
{
    PropertyEditorWidget *w = nullptr;
    switch (type) {
    case PropertyType::Checkbox: w = new CheckboxPropertyEditor(parent); break;
    case PropertyType::Number:   w = new NumberPropertyEditor(parent); break;
    case PropertyType::Date:     w = new DatePropertyEditor(parent); break;
    case PropertyType::DateTime: w = new DateTimePropertyEditor(parent); break;
    case PropertyType::List:     w = new ListPropertyEditor(parent); break;
    case PropertyType::Text:
    default:                     w = new TextPropertyEditor(parent); break;
    }
    w->setValue(initialValue);
    return w;
}

}  // namespace Corbomite
