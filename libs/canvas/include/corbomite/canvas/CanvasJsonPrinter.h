// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QString>

namespace Corbomite::Canvas {

// Byte-compatible port of Obsidian 1.13.7's `.canvas` serializer — `$d`
// (app/own/0563-$d.js:2) over `Zd` (app/own/0564-Zd.js:2-38): tab indent, an
// all-primitive array or object collapsed onto a single compact line, no
// space after the colon, no trailing newline.
//
// QJsonObject cannot be the carrier: it stores keys sorted by byte value, so
// both parsing and toJson() destroy the order this has to reproduce. Hence
// this insertion-ordered value tree; CanvasDocument is its only producer.
//
// One order cannot be reproduced: a payload Corbomite keeps verbatim (an
// unmodelled node object, an unmodelled field) arrives as a QJsonObject,
// which Qt already sorted when parsing, so those keys come out
// alphabetically instead of in file order. Every key Corbomite models is
// emitted in Obsidian's exact position.
class OrderedJson
{
public:
    OrderedJson();
    explicit OrderedJson(const QJsonValue &leaf);

    static OrderedJson array();
    static OrderedJson object();

    // Object member; re-setting an existing key overwrites it in place rather
    // than appending a duplicate.
    OrderedJson &set(const QString &key, const QJsonValue &value);
    OrderedJson &setObject(const QString &key, const OrderedJson &value);

    void append(const QJsonValue &value);
    void appendObject(const OrderedJson &value);

    QJsonValue toQJsonValue() const;

    QByteArray print() const;

private:
    void printInto(QStringList &out) const;
    void appendCompact(QStringList &out) const;
    void appendExpanded(QStringList &out) const;
    // `Qd` applied to the immediate children only — a container whose children
    // are all primitives-and-themselves-containers still expands, because
    // typeof {} === "object".
    bool childrenArePrimitive() const;

    enum class Kind { Leaf, Array, Object };
    Kind m_kind = Kind::Leaf;
    QJsonValue m_leaf;
    QList<OrderedJson> m_items;
    QList<QPair<QString, OrderedJson>> m_members;
};

} // namespace Corbomite::Canvas