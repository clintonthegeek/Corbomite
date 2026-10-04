// SPDX-License-Identifier: GPL-3.0-or-later
#include "canvas/CanvasJsonPrinter.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Canvas {

namespace {

// Zd treats a value as printable-in-place when `Qd(e)` is true, i.e.
// `"object" != typeof e` (app/own/0562-Qd.js:2). `typeof null === "object"`,
// so a JSON null is *not* primitive: it forces the container to expand, and
// only then renders as the bare token `null`.
bool jsPrimitive(const QJsonValue &v)
{
    return !(v.isObject() || v.isArray() || v.isNull());
}

// JSON.stringify of a single scalar, via Qt's own writer so its escaping stays
// in step with the JS one (`"`, `\`, control chars as \u00xx, raw UTF-8).
QByteArray scalarText(const QJsonValue &v)
{
    QJsonArray wrap;
    wrap.append(v);
    const QByteArray b = QJsonDocument(wrap).toJson(QJsonDocument::Compact);
    return b.mid(1, b.size() - 2);
}

// A verbatim payload arrives as a plain QJsonValue. Promoting it to an ordered
// tree is what makes a nested container inside one expand the way Zd does
// instead of coming out compact; its keys are already in Qt's sorted order.
OrderedJson orderedFrom(const QJsonValue &v)
{
    if (v.isObject()) {
        OrderedJson o = OrderedJson::object();
        const QJsonObject obj = v.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
            o.setObject(it.key(), orderedFrom(it.value()));
        return o;
    }
    if (v.isArray()) {
        OrderedJson a = OrderedJson::array();
        const QJsonArray arr = v.toArray();
        for (const auto &item : arr)
            a.appendObject(orderedFrom(item));
        return a;
    }
    return OrderedJson(v);
}

} // namespace

OrderedJson::OrderedJson() = default;

OrderedJson::OrderedJson(const QJsonValue &leaf)
    : m_kind(Kind::Leaf)
    , m_leaf(leaf)
{
}

OrderedJson OrderedJson::array()
{
    OrderedJson j;
    j.m_kind = Kind::Array;
    return j;
}

OrderedJson OrderedJson::object()
{
    OrderedJson j;
    j.m_kind = Kind::Object;
    return j;
}

OrderedJson &OrderedJson::set(const QString &key, const QJsonValue &value)
{
    return setObject(key, OrderedJson(value));
}

OrderedJson &OrderedJson::setObject(const QString &key, const OrderedJson &value)
{
    if (m_kind != Kind::Object)
        *this = object();
    for (auto &member : m_members) {
        if (member.first == key) {
            member.second = value;
            return *this;
        }
    }
    m_members.append({key, value});
    return *this;
}

void OrderedJson::append(const QJsonValue &value)
{
    appendObject(OrderedJson(value));
}

void OrderedJson::appendObject(const OrderedJson &value)
{
    if (m_kind != Kind::Array)
        *this = array();
    m_items.append(value);
}

QJsonValue OrderedJson::toQJsonValue() const
{
    switch (m_kind) {
    case Kind::Leaf:
        return m_leaf;
    case Kind::Array: {
        QJsonArray out;
        for (const auto &item : m_items)
            out.append(item.toQJsonValue());
        return out;
    }
    case Kind::Object: {
        QJsonObject out;
        for (const auto &member : m_members)
            out.insert(member.first, member.second.toQJsonValue());
        return out;
    }
    }
    return QJsonValue(QJsonValue::Null);
}

bool OrderedJson::childrenArePrimitive() const
{
    switch (m_kind) {
    case Kind::Leaf:
        return jsPrimitive(m_leaf);
    case Kind::Array:
        for (const auto &item : m_items) {
            if (item.m_kind != Kind::Leaf || !jsPrimitive(item.m_leaf))
                return false;
        }
        return true;
    case Kind::Object:
        for (const auto &member : m_members) {
            if (member.second.m_kind != Kind::Leaf || !jsPrimitive(member.second.m_leaf))
                return false;
        }
        return true;
    }
    return false;
}

void OrderedJson::appendCompact(QStringList &out) const
{
    switch (m_kind) {
    case Kind::Leaf:
        out.append(QString::fromUtf8(scalarText(m_leaf)));
        return;
    case Kind::Array: {
        QByteArray b = "[";
        for (int i = 0; i < m_items.size(); ++i) {
            if (i)
                b += ',';
            b += scalarText(m_items[i].m_leaf);
        }
        out.append(QString::fromUtf8(b + ']'));
        return;
    }
    case Kind::Object: {
        QByteArray b = "{";
        for (int i = 0; i < m_members.size(); ++i) {
            if (i)
                b += ',';
            b += scalarText(QJsonValue(m_members[i].first));
            b += ':';
            b += scalarText(m_members[i].second.m_leaf);
        }
        out.append(QString::fromUtf8(b + '}'));
        return;
    }
    }
}

void OrderedJson::appendExpanded(QStringList &out) const
{
    const bool isArray = m_kind == Kind::Array;
    const int count = isArray ? m_items.size() : m_members.size();
    out.append(isArray ? QStringLiteral("[") : QStringLiteral("{"));

    for (int i = 0; i < count; ++i) {
        QStringList lines;
        if (isArray) {
            m_items[i].printInto(lines);
        } else {
            m_members[i].second.printInto(lines);
            lines[0] = QString::fromUtf8(scalarText(QJsonValue(m_members[i].first)))
                + QLatin1Char(':') + lines[0];
        }
        for (int j = 0; j < lines.size(); ++j) {
            QString line = QLatin1Char('\t') + lines[j];
            if (j == lines.size() - 1 && i != count - 1)
                line += QLatin1Char(',');
            out.append(line);
        }
    }
    out.append(isArray ? QStringLiteral("]") : QStringLiteral("}"));
}

void OrderedJson::printInto(QStringList &out) const
{
    if (m_kind == Kind::Leaf) {
        // Only a container leaf needs promoting; null is non-primitive for
        // the collapse test but is not a container, so it prints inline.
        if (!m_leaf.isObject() && !m_leaf.isArray()) {
            out.append(QString::fromUtf8(scalarText(m_leaf)));
            return;
        }
        orderedFrom(m_leaf).printInto(out);
        return;
    }
    if (childrenArePrimitive()) {
        appendCompact(out);
        return;
    }
    appendExpanded(out);
}

QByteArray OrderedJson::print() const
{
    QStringList out;
    printInto(out);
    return out.join(QLatin1Char('\n')).toUtf8();
}

} // namespace Canvas