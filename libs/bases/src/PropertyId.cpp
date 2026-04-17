// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/PropertyId.h"

namespace Corbomite::Bases {

QString PropertyId::toString() const
{
    return buildPropertyId(*this);
}

PropertyId parsePropertyId(const QString &s)
{
    const int dot = s.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        // Unprefixed → `note.<name>` (audit §11).
        return { PropertyKind::Note, s };
    }
    const QString prefix = s.left(dot);
    const QString name = s.mid(dot + 1);
    if (prefix == QLatin1String("note"))    return { PropertyKind::Note, name };
    if (prefix == QLatin1String("file"))    return { PropertyKind::File, name };
    if (prefix == QLatin1String("formula")) return { PropertyKind::Formula, name };
    // Unknown prefix: conservatively treat whole string as a note-property
    // key name (preserves user's intent; forward-compat).
    return { PropertyKind::Note, s };
}

QString buildPropertyId(const PropertyId &id)
{
    switch (id.kind) {
    case PropertyKind::Note:    return QStringLiteral("note.") + id.name;
    case PropertyKind::File:    return QStringLiteral("file.") + id.name;
    case PropertyKind::Formula: return QStringLiteral("formula.") + id.name;
    }
    return id.name;
}

const QStringList &filePropertyMembers()
{
    static const QStringList kMembers {
        QStringLiteral("file"),     QStringLiteral("name"),
        QStringLiteral("basename"), QStringLiteral("fullname"),
        QStringLiteral("path"),     QStringLiteral("folder"),
        QStringLiteral("ext"),      QStringLiteral("ctime"),
        QStringLiteral("mtime"),    QStringLiteral("size"),
        QStringLiteral("links"),    QStringLiteral("backlinks"),
        QStringLiteral("embeds"),   QStringLiteral("tags"),
    };
    return kMembers;
}

}  // namespace Corbomite::Bases
