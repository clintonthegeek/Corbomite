// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/BasesCellDelegate.h"

#include "corbomite/bases/BasesTreeModel.h"
#include "corbomite/bases/CellHitTest.h"
#include "corbomite/bases/Values.h"

#include "corbomite/core/LucideIconRegistry.h"

#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QIcon>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QTextDocument>

namespace Corbomite::Bases {

BasesCellDelegate::BasesCellDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

BasesCellDelegate::~BasesCellDelegate() = default;

QWidget *BasesCellDelegate::createEditor(QWidget *parent,
                                         const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const
{
    Q_UNUSED(option);
    const QString type = index.data(BasesTreeModel::ValueTypeRole).toString();

    if (type == QLatin1String("Number")) {
        auto *sb = new QDoubleSpinBox(parent);
        sb->setDecimals(6);
        sb->setRange(-1e15, 1e15);
        return sb;
    }
    if (type == QLatin1String("Date")) {
        auto valueVar = index.data(BasesTreeModel::ValuePtrRole);
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
    const QString type = index.data(BasesTreeModel::ValueTypeRole).toString();

    if (auto *sb = qobject_cast<QDoubleSpinBox *>(editor)) {
        auto valueVar = index.data(BasesTreeModel::ValuePtrRole);
        ValuePtr v = valueVar.value<ValuePtr>();
        auto *n = dynamic_cast<NumberValue *>(v.get());
        sb->setValue(n ? n->data() : 0.0);
        return;
    }
    if (auto *de = qobject_cast<QDateTimeEdit *>(editor)) {
        auto valueVar = index.data(BasesTreeModel::ValuePtrRole);
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

    const QString type = index.data(BasesTreeModel::ValueTypeRole).toString();
    if (type == QLatin1String("Icon")) {
        const QString name = index.data(Qt::DisplayRole).toString();
        const QIcon ic = Corbomite::LucideIconRegistry::instance().get(name);
        painter->save();
        if (!ic.isNull()) ic.paint(painter, option.rect, Qt::AlignCenter);
        else painter->drawText(option.rect, Qt::AlignCenter, name);     // fallback
        painter->restore();
        return;
    }
    if (type == QLatin1String("Image")) {
        const QString ref = index.data(Qt::DisplayRole).toString();
        QPixmap pm(ref);                       // absolute/relative-to-cwd path
        painter->save();
        if (!pm.isNull())
            painter->drawPixmap(option.rect,
                pm.scaled(option.rect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else painter->drawText(option.rect, Qt::AlignVCenter | Qt::AlignLeft, ref);
        painter->restore();
        return;
    }
    if (type == QLatin1String("HTML")) {
        const QString html = index.data(Qt::DisplayRole).toString();
        QTextDocument doc; doc.setHtml(html); doc.setTextWidth(option.rect.width());
        painter->save();
        painter->translate(option.rect.topLeft());
        doc.drawContents(painter, QRectF(0, 0, option.rect.width(), option.rect.height()));
        painter->restore();
        return;
    }
    // "Markdown" intentionally falls through to the plain-text fallback (deferred).
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
        // Paint into the same rect CellHitTest hit-tests so click and glyph align.
        const auto valueVar = index.data(BasesTreeModel::ValuePtrRole);
        ValuePtr v = valueVar.value<ValuePtr>();
        auto *b = dynamic_cast<BooleanValue *>(v.get());
        painter->save();
        const QString glyph = b && b->data()
            ? QStringLiteral("\u2611")   // ballot box with check
            : QStringLiteral("\u2610");  // ballot box
        painter->drawText(checkboxGlyphRect(option.rect), Qt::AlignCenter, glyph);
        painter->restore();
        return;
    }
    if (type == QLatin1String("Link") || type == QLatin1String("URL")) {
        auto *s = dynamic_cast<StringValue *>(index.data(BasesTreeModel::ValuePtrRole)
                                                   .value<ValuePtr>().get());
        const QString text = s ? s->data() : index.data(Qt::DisplayRole).toString();
        const QFontMetrics fm(option.font);
        painter->save();
        painter->setPen(option.palette.link().color());
        painter->drawText(linkTextRect(text, option.rect, fm),
                          Qt::AlignVCenter | Qt::AlignLeft, text);
        painter->restore();
        return;
    }
    {
        // Tag chips (a ListValue of TagValues, or a lone TagValue).
        const ValuePtr v = index.data(BasesTreeModel::ValuePtrRole).value<ValuePtr>();
        const QFontMetrics fm(option.font);
        const QVector<QRect> chips = tagChipRects(v, option.rect, fm);
        if (!chips.isEmpty()) {
            QStringList tags;
            if (auto *list = dynamic_cast<ListValue *>(v.get())) {
                for (const auto &e : list->data())
                    if (auto *t = dynamic_cast<TagValue *>(e.get())) tags << t->data();
            } else if (auto *t = dynamic_cast<TagValue *>(v.get())) {
                tags << t->data();
            }
            painter->save();
            for (int i = 0; i < chips.size() && i < tags.size(); ++i) {
                painter->setBrush(option.palette.alternateBase());
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(chips[i], 6, 6);
                painter->setPen(option.palette.text().color());
                painter->drawText(chips[i], Qt::AlignCenter, tags[i]);
            }
            painter->restore();
            return;
        }
    }
    QStyledItemDelegate::paint(painter, option, index);
}

bool BasesCellDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                    const QStyleOptionViewItem &option,
                                    const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        const QString type = index.data(BasesTreeModel::ValueTypeRole).toString();
        const ValuePtr value = index.data(BasesTreeModel::ValuePtrRole).value<ValuePtr>();
        const QFontMetrics fm(option.font);
        const CellHit hit = hitTestCell(type, value, option.rect, me->position().toPoint(), fm);

        if (hit.kind == CellHit::Checkbox && me->button() == Qt::LeftButton) {
            auto *b = dynamic_cast<BooleanValue *>(value.get());
            model->setData(index, !(b && b->data()), Qt::EditRole);
            return true;
        }
        if (hit.kind == CellHit::Link
            && (me->button() == Qt::LeftButton || me->button() == Qt::MiddleButton)) {
            // Middle-click behaves like Ctrl+click (open in new tab).
            Qt::KeyboardModifiers mods = me->modifiers();
            if (me->button() == Qt::MiddleButton) mods |= Qt::ControlModifier;
            Q_EMIT linkClicked(hit.payload, mods);
            return true;
        }
        if (hit.kind == CellHit::Tag && me->button() == Qt::LeftButton) {
            Q_EMIT tagClicked(hit.payload);
            return true;
        }
        if (hit.kind == CellHit::Url && me->button() == Qt::LeftButton) {
            Q_EMIT urlClicked(hit.payload);
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

}  // namespace Corbomite::Bases
