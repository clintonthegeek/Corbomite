// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/CellHitTest.h"

#include "corbomite/bases/Values.h"

#include <QFontMetrics>

namespace Corbomite::Bases {

namespace {
constexpr int kGlyph = 18;      // checkbox glyph box
constexpr int kPad = 4;         // left text padding / chip inner padding
constexpr int kChipSpacing = 4; // gap between tag chips

// Pull the displayed text out of a StringValue subclass (Link/Url/Tag/String).
QString stringData(const ValuePtr &v) {
    if (auto *s = dynamic_cast<StringValue *>(v.get())) return s->data();
    return v ? v->toString() : QString{};
}
}  // namespace

QRect checkboxGlyphRect(const QRect &cellRect)
{
    QRect r(0, 0, kGlyph, kGlyph);
    r.moveCenter(cellRect.center());
    return r;
}

QRect linkTextRect(const QString &text, const QRect &cellRect, const QFontMetrics &fm)
{
    const int w = fm.horizontalAdvance(text);
    return QRect(cellRect.left() + kPad, cellRect.top(),
                 qMin(w, cellRect.width() - kPad), cellRect.height());
}

QRect urlTextRect(const QString &text, const QRect &cellRect, const QFontMetrics &fm)
{
    return linkTextRect(text, cellRect, fm);
}

QVector<QRect> tagChipRects(const ValuePtr &value, const QRect &cellRect, const QFontMetrics &fm)
{
    // Collect tag texts: a ListValue of TagValues, or a single TagValue.
    QStringList tags;
    if (auto *list = dynamic_cast<ListValue *>(value.get())) {
        for (const auto &e : list->data())
            if (auto *t = dynamic_cast<TagValue *>(e.get())) tags << t->data();
    } else if (auto *t = dynamic_cast<TagValue *>(value.get())) {
        tags << t->data();
    }

    QVector<QRect> rects;
    int x = cellRect.left() + kPad;
    const int h = qMin(cellRect.height() - 2, fm.height() + 2 * kPad);
    const int y = cellRect.top() + (cellRect.height() - h) / 2;
    for (const QString &tag : tags) {
        const int w = fm.horizontalAdvance(tag) + 2 * kPad;
        rects.append(QRect(x, y, w, h));
        x += w + kChipSpacing;
    }
    return rects;
}

CellHit hitTestCell(const QString &valueType, const ValuePtr &value,
                    const QRect &cellRect, const QPoint &point, const QFontMetrics &fm)
{
    if (!value) return {};

    if (valueType == QLatin1String("Boolean")) {
        if (checkboxGlyphRect(cellRect).contains(point))
            return { CellHit::Checkbox, -1, {} };
        return {};
    }
    if (valueType == QLatin1String("Link")) {
        const QString target = stringData(value);
        if (linkTextRect(target, cellRect, fm).contains(point))
            return { CellHit::Link, -1, target };
        return {};
    }
    if (valueType == QLatin1String("URL")) {
        const QString url = stringData(value);
        if (urlTextRect(url, cellRect, fm).contains(point))
            return { CellHit::Url, -1, url };
        return {};
    }
    if (valueType == QLatin1String("Tag") || valueType == QLatin1String("List")) {
        const QVector<QRect> chips = tagChipRects(value, cellRect, fm);
        QStringList tags;
        if (auto *list = dynamic_cast<ListValue *>(value.get())) {
            for (const auto &e : list->data())
                if (auto *t = dynamic_cast<TagValue *>(e.get())) tags << t->data();
        } else if (auto *t = dynamic_cast<TagValue *>(value.get())) {
            tags << t->data();
        }
        for (int i = 0; i < chips.size(); ++i)
            if (chips[i].contains(point))
                return { CellHit::Tag, i, tags.value(i) };
        return {};
    }
    return {};
}

}  // namespace Corbomite::Bases
