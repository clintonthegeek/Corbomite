// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ValuePtr.h"

#include <QRect>
#include <QString>
#include <QVector>

class QFontMetrics;

namespace Corbomite::Bases {

/// What lies under a point inside a Bases cell. `Whitespace` means the caller
/// should fall back to default selection / double-click-to-edit behaviour.
struct CellHit {
    enum Kind { Whitespace, Checkbox, Link, Tag, Url } kind = Whitespace;
    int tagIndex = -1;     ///< index into the tag list when kind == Tag
    QString payload;       ///< Link: link target; Url: url; Tag: tag text
};

/// Centered, fixed-size square where the boolean ballot glyph is drawn.
QRect checkboxGlyphRect(const QRect &cellRect);
/// Left-aligned bounding rect of `text` (font-measured, clipped to the cell).
QRect linkTextRect(const QString &text, const QRect &cellRect, const QFontMetrics &fm);
QRect urlTextRect(const QString &text, const QRect &cellRect, const QFontMetrics &fm);
/// Per-tag chip rects, left-to-right, for a ListValue of TagValues (or a single
/// TagValue). Empty if `value` carries no tags.
QVector<QRect> tagChipRects(const ValuePtr &value, const QRect &cellRect, const QFontMetrics &fm);

/// Hit-test `point` (viewport coords, same space as `cellRect`) against the
/// interactive element the delegate paints for `valueType`/`value`.
CellHit hitTestCell(const QString &valueType, const ValuePtr &value,
                    const QRect &cellRect, const QPoint &point, const QFontMetrics &fm);

}  // namespace Corbomite::Bases
