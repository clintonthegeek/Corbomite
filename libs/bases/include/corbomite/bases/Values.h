// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Value.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <functional>
#include <memory>

namespace Corbomite {
class TFile;
class Vault;
class MetadataCache;
}  // namespace Corbomite

namespace Corbomite::Bases {

/// Singleton "no value". `NullValue::instance()` is the only way to
/// obtain one — constructor is private.
class NullValue : public Value
{
public:
    static ValuePtr instance();

    QString type() const override { return QStringLiteral("Null"); }
    bool isTruthy() const override { return false; }
    bool isEmpty() const override { return true; }
    QString toString() const override { return {}; }

private:
    NullValue() = default;
    friend struct NullValueAccess;
};

class BooleanValue : public Value
{
public:
    explicit BooleanValue(bool v) : m_data(v) {}

    bool data() const { return m_data; }

    QString type() const override { return QStringLiteral("Boolean"); }
    bool isTruthy() const override { return m_data; }
    bool isEmpty() const override { return !m_data; }
    QString toString() const override
    {
        return m_data ? QStringLiteral("true") : QStringLiteral("false");
    }
    bool equals(const Value &other) const override;
    bool looseEquals(const Value &other) const override;

private:
    bool m_data;
};

class NumberValue : public Value
{
public:
    explicit NumberValue(double v) : m_data(v) {}

    double data() const { return m_data; }

    QString type() const override { return QStringLiteral("Number"); }
    bool isTruthy() const override;             // 0 and NaN are falsy
    bool isEmpty() const override { return false; }  // per addendum §8.3
    QString toString() const override;          // "∞" for infinities
    bool equals(const Value &other) const override;
    bool looseEquals(const Value &other) const override;

private:
    double m_data;
};

class StringValue : public Value
{
public:
    explicit StringValue(QString v) : m_data(std::move(v)) {}

    const QString &data() const { return m_data; }

    QString type() const override { return QStringLiteral("String"); }
    bool isTruthy() const override { return !m_data.isEmpty(); }
    bool isEmpty() const override { return m_data.isEmpty(); }
    QString toString() const override { return m_data; }
    bool equals(const Value &other) const override;

    /// `"length"` → `NumberValue(data.size())`.
    ValuePtr objectAccess(const QString &key) const override;
    QStringList keys() const override;

protected:
    // Protected so Link/Url/Tag/Icon/Image/HTML/Markdown subclass cleanly
    // and the helper string methods can reuse m_data semantics.
    QString m_data;
};

class ListValue : public Value
{
public:
    ListValue() = default;
    explicit ListValue(QVector<ValuePtr> items) : m_data(std::move(items)) {}

    const QVector<ValuePtr> &data() const { return m_data; }

    QString type() const override { return QStringLiteral("List"); }
    bool isTruthy() const override { return !m_data.isEmpty(); }
    bool isEmpty() const override { return m_data.isEmpty(); }
    QString toString() const override;
    bool equals(const Value &other) const override;
    ValuePtr objectAccess(const QString &key) const override;
    QStringList keys() const override;

    // --- iteration helpers ---
    int length() const { return static_cast<int>(m_data.size()); }
    ValuePtr get(int i) const;
    bool includes(const ValuePtr &v) const;
    std::shared_ptr<ListValue> concat(const ListValue &other) const;
    std::shared_ptr<ListValue> reverse() const;
    std::shared_ptr<ListValue> flatten() const;   // one-level (audit §8.7 `flat`)
    std::shared_ptr<ListValue> unique() const;
    std::shared_ptr<ListValue> sort() const;
    std::shared_ptr<ListValue> slice(int start, int endExclusive = -1) const;
    QString join(const QString &sep) const;

    // --- numeric aggregates — non-Number elements propagate NullValue ---
    ValuePtr min() const;
    ValuePtr max() const;
    ValuePtr sum() const;
    ValuePtr mean() const;
    ValuePtr median() const;
    ValuePtr stddev() const;   // population stddev (see addendum §14)

    // --- date aggregates — non-Date elements propagate NullValue ---
    ValuePtr earliest() const;
    ValuePtr latest() const;

private:
    QVector<ValuePtr> m_data;
};

}  // namespace Corbomite::Bases
