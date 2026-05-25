// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesCellDelegate.h"

#include "corbomite/bases/BasesTableModel.h"
#include "corbomite/bases/BasesTreeModel.h"
#include "corbomite/bases/Values.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPainter>
#include <QStyle>

namespace Corbomite::Bases {

BasesCellDelegate::BasesCellDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

BasesCellDelegate::~BasesCellDelegate() = default;

QWidget *BasesCellDelegate::createEditor(QWidget *parent,
                                         const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const
{
    Q_UNUSED(option);
    const QString type = index.data(BasesTableModel::ValueTypeRole).toString();

    if (type == QLatin1String("Boolean")) {
        auto *cb = new QCheckBox(parent);
        cb->setTristate(false);
        return cb;
    }
    if (type == QLatin1String("Number")) {
        auto *sb = new QDoubleSpinBox(parent);
        sb->setDecimals(6);
        sb->setRange(-1e15, 1e15);
        return sb;
    }
    if (type == QLatin1String("Date")) {
        auto valueVar = index.data(BasesTableModel::ValuePtrRole);
        ValuePtr v = valueVar.value<ValuePtr>();
        auto *d = dynamic_cast<DateValue *>(v.get());
        if (d && d->hasTime()) {
            auto *e = new QDateTimeEdit(parent);
            e->setCalendarPopup(true);
            return e;
        }
        auto *e = new QDateEdit(parent);
        e->setCalendarPopup(true);
        return e;
    }
    return new QLineEdit(parent);
}

void BasesCellDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const QString type = index.data(BasesTableModel::ValueTypeRole).toString();

    if (auto *cb = qobject_cast<QCheckBox *>(editor)) {
        auto valueVar = index.data(BasesTableModel::ValuePtrRole);
        ValuePtr v = valueVar.value<ValuePtr>();
        auto *b = dynamic_cast<BooleanValue *>(v.get());
        cb->setChecked(b && b->data());
        return;
    }
    if (auto *sb = qobject_cast<QDoubleSpinBox *>(editor)) {
        auto valueVar = index.data(BasesTableModel::ValuePtrRole);
        ValuePtr v = valueVar.value<ValuePtr>();
        auto *n = dynamic_cast<NumberValue *>(v.get());
        sb->setValue(n ? n->data() : 0.0);
        return;
    }
    if (auto *de = qobject_cast<QDateTimeEdit *>(editor)) {
        auto valueVar = index.data(BasesTableModel::ValuePtrRole);
        ValuePtr v = valueVar.value<ValuePtr>();
        if (auto *d = dynamic_cast<DateValue *>(v.get())) {
            de->setDateTime(d->dateTime());
            return;
        }
        de->setDateTime(QDateTime::currentDateTime());
        return;
    }
    if (auto *le = qobject_cast<QLineEdit *>(editor)) {
        le->setText(index.data(Qt::EditRole).toString());
    }
    Q_UNUSED(type);
}

void BasesCellDelegate::setModelData(QWidget *editor,
                                     QAbstractItemModel *model,
                                     const QModelIndex &index) const
{
    if (auto *cb = qobject_cast<QCheckBox *>(editor)) {
        model->setData(index, cb->isChecked(), Qt::EditRole);
        return;
    }
    if (auto *sb = qobject_cast<QDoubleSpinBox *>(editor)) {
        model->setData(index, sb->value(), Qt::EditRole);
        return;
    }
    if (auto *de = qobject_cast<QDateTimeEdit *>(editor)) {
        model->setData(index, de->dateTime().toString(Qt::ISODate), Qt::EditRole);
        return;
    }
    if (auto *le = qobject_cast<QLineEdit *>(editor)) {
        model->setData(index, le->text(), Qt::EditRole);
    }
}

void BasesCellDelegate::paint(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    // Group-heading row: bold label + count (column 0), summary cells otherwise.
    if (index.data(BasesTreeModel::IsGroupRowRole).toBool()) {
        painter->save();
        painter->fillRect(option.rect, option.palette.alternateBase());
        QFont f = option.font; f.setBold(true); painter->setFont(f);
        QString text = index.data(Qt::DisplayRole).toString();
        if (index.column() == 0) {
            const int n = index.data(BasesTreeModel::GroupCountRole).toInt();
            text = QStringLiteral("%1  (%2)").arg(text).arg(n);
        }
        painter->drawText(option.rect.adjusted(4, 0, -4, 0),
                          Qt::AlignVCenter | Qt::AlignLeft, text);
        painter->restore();
        return;
    }

    const QString type = index.data(BasesTableModel::ValueTypeRole).toString();
    if (type == QLatin1String("Error")) {
        // Subtle warning tint for error cells.
        painter->save();
        painter->fillRect(option.rect,
                          option.palette.color(QPalette::Highlight).lighter(180));
        painter->restore();
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    if (type == QLatin1String("Boolean")) {
        // Render as a checkmark glyph rather than "true"/"false" text.
        const auto valueVar = index.data(BasesTableModel::ValuePtrRole);
        ValuePtr v = valueVar.value<ValuePtr>();
        auto *b = dynamic_cast<BooleanValue *>(v.get());
        painter->save();
        const QString glyph = b && b->data()
            ? QStringLiteral("\u2611")   // ballot box with check
            : QStringLiteral("\u2610");  // ballot box
        painter->drawText(option.rect, Qt::AlignCenter, glyph);
        painter->restore();
        return;
    }
    QStyledItemDelegate::paint(painter, option, index);
}

}  // namespace Corbomite::Bases
