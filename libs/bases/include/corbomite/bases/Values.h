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

class DateValue : public Value
{
public:
    DateValue(QDateTime dt, bool hasTime) : m_dt(std::move(dt)), m_hasTime(hasTime) {}

    const QDateTime &dateTime() const { return m_dt; }
    bool hasTime() const { return m_hasTime; }

    QString type() const override { return QStringLiteral("Date"); }
    bool isTruthy() const override { return true; }
    bool isEmpty() const override { return false; }
    QString toString() const override;
    bool equals(const Value &other) const override;
    bool looseEquals(const Value &other) const override;
    ValuePtr objectAccess(const QString &key) const override;
    QStringList keys() const override;

    /// `YYYY-MM-DD` (date-only) or `YYYY-MM-DD[ T]HH:MM[:SS[.ms]][TZ]`.
    /// Returns nullptr on malformed input. Matches addendum §6.1.
    static std::shared_ptr<DateValue> parseFromString(const QString &text);

protected:
    QDateTime m_dt;
    bool m_hasTime;
};

class RelativeDateValue : public DateValue
{
public:
    RelativeDateValue(QDateTime dt, bool hasTime) : DateValue(std::move(dt), hasTime) {}

    QString toString() const override;  // "3 days ago"
};

/// Tag — StringValue with hierarchical-prefix matching.
class TagValue : public StringValue
{
public:
    explicit TagValue(QString tag) : StringValue(std::move(tag)) {}
    QString type() const override { return QStringLiteral("Tag"); }
    /// `#parent` matches `#parent/child` via `/` boundary. Exact match
    /// also qualifies.
    bool tagMatches(const QString &other) const;
};

/// `[[link]]` / `[[link|display]]`. Subclass of StringValue where m_data
/// holds the normalised link target; display + sourcePath are separate.
class LinkValue : public StringValue
{
public:
    LinkValue(QString link, QString sourcePath = {}, QString display = {})
        : StringValue(std::move(link)),
          m_sourcePath(std::move(sourcePath)),
          m_display(std::move(display)) {}

    const QString &sourcePath() const { return m_sourcePath; }
    const QString &display() const { return m_display; }

    QString type() const override { return QStringLiteral("Link"); }
    QString toString() const override;  // `[[data|display]]` or `[[data]]`
    bool looseEquals(const Value &other) const override;

    /// Parse `[[...]]` or `[[...|display]]`. Returns nullptr if not a
    /// wikilink literal.
    static std::shared_ptr<LinkValue> parseFromString(const QString &text,
                                                      const QString &sourcePath = {});

private:
    QString m_sourcePath;
    QString m_display;
};

class UrlValue : public StringValue
{
public:
    explicit UrlValue(QString url, QString display = {})
        : StringValue(std::move(url)), m_display(std::move(display)) {}
    const QString &display() const { return m_display; }

    QString type() const override { return QStringLiteral("URL"); }

private:
    QString m_display;
};

class IconValue : public StringValue
{
public:
    explicit IconValue(QString name) : StringValue(std::move(name)) {}
    QString type() const override { return QStringLiteral("Icon"); }
};

class ImageValue : public StringValue
{
public:
    explicit ImageValue(QString pathOrUrl) : StringValue(std::move(pathOrUrl)) {}
    QString type() const override { return QStringLiteral("Image"); }
};

class HTMLValue : public StringValue
{
public:
    explicit HTMLValue(QString html) : StringValue(std::move(html)) {}
    QString type() const override { return QStringLiteral("HTML"); }
};

class MarkdownValue : public StringValue
{
public:
    explicit MarkdownValue(QString md) : StringValue(std::move(md)) {}
    QString type() const override { return QStringLiteral("Markdown"); }
};

class FormulaErrorValue : public Value
{
public:
    explicit FormulaErrorValue(QString msg) : m_msg(std::move(msg)) {}

    const QString &message() const { return m_msg; }

    QString type() const override { return QStringLiteral("Error"); }
    bool isTruthy() const override { return false; }
    QString toString() const override { return m_msg; }

private:
    QString m_msg;
};

class ObjectValue : public Value
{
public:
    ObjectValue() = default;

    /// Build from an Obsidian-shape frontmatter QJsonObject. Strings that
    /// look like wikilinks → LinkValue; URLs → UrlValue; ISO dates → DateValue;
    /// the `tags` key → ListValue<TagValue>. Nested arrays/objects recurse.
    static std::shared_ptr<ObjectValue> fromFrontMatter(const QJsonObject &fm);

    void set(const QString &key, ValuePtr value);
    ValuePtr get(const QString &key) const;              // exact-case
    ValuePtr getInsensitive(const QString &key) const;   // case-insensitive

    QString type() const override { return QStringLiteral("Object"); }
    bool isTruthy() const override { return !m_order.isEmpty(); }
    bool isEmpty() const override { return m_order.isEmpty(); }
    bool equals(const Value &other) const override;
    /// Case-insensitive (audit §8 invariant).
    ValuePtr objectAccess(const QString &key) const override;
    QStringList keys() const override { return m_order; }

    QVector<ValuePtr> values() const;
    QVector<std::pair<QString, ValuePtr>> entries() const;

private:
    QStringList m_order;
    QHash<QString, ValuePtr> m_data;
};

/// Small adapter Value produced by BasesEntry::getByIdentifier("formula"):
/// objectAccess forwards through a closure that resolves by formula name.
class LambdaObjectValue : public Value
{
public:
    using Resolver = std::function<ValuePtr(const QString &)>;
    explicit LambdaObjectValue(Resolver r) : m_r(std::move(r)) {}

    QString type() const override { return QStringLiteral("Object"); }
    bool isTruthy() const override { return true; }
    ValuePtr objectAccess(const QString &key) const override;

private:
    Resolver m_r;
};

class FileValue : public Value, public std::enable_shared_from_this<FileValue>
{
public:
    FileValue(TFile *file, MetadataCache *cache);
    ~FileValue() override;

    TFile *file() const { return m_file; }
    MetadataCache *cache() const { return m_cache; }

    QString type() const override { return QStringLiteral("File"); }
    bool isTruthy() const override { return m_file != nullptr; }
    QString toString() const override;  // file.name
    bool equals(const Value &other) const override;
    bool looseEquals(const Value &other) const override;
    ValuePtr objectAccess(const QString &key) const override;
    QStringList keys() const override;

    // --- aggregate accessors — populated lazily from MetadataCache ---
    std::shared_ptr<ListValue> getLinks() const;
    std::shared_ptr<ListValue> getBacklinks() const;
    std::shared_ptr<ListValue> getEmbeds() const;
    std::shared_ptr<ListValue> getTags() const;
    std::shared_ptr<ObjectValue> getProperties() const;

    // --- predicates for built-in file.* methods (Phase 5) ---
    bool hasLink(const ValuePtr &other) const;
    bool inFolder(const QString &folderPath) const;
    bool hasTag(const QStringList &tags) const;
    bool hasProperty(const QString &name) const;

    static const QStringList &filePropertyMembers();  // 14 names per audit §2

protected:
    TFile *m_file;
    MetadataCache *m_cache;

    mutable std::shared_ptr<ListValue>   m_cachedLinks;
    mutable std::shared_ptr<ListValue>   m_cachedBacklinks;
    mutable std::shared_ptr<ListValue>   m_cachedEmbeds;
    mutable std::shared_ptr<ListValue>   m_cachedTags;
    mutable std::shared_ptr<ObjectValue> m_cachedProperties;
};

/// `this` binding. Forwards objectAccess to the enclosing entry's
/// getByIdentifier (set by BasesEntry on evaluation entry).
class ThisFileValue : public FileValue
{
public:
    using Forwarder = std::function<ValuePtr(const QString &)>;

    ThisFileValue(TFile *file, MetadataCache *cache, Forwarder forwarder);

    QString type() const override { return QStringLiteral("ThisFile"); }
    ValuePtr objectAccess(const QString &key) const override;

private:
    Forwarder m_forwarder;
};

class RegExpValue : public Value
{
public:
    explicit RegExpValue(QRegularExpression re, QString source = {}, QString flags = {})
        : m_re(std::move(re)), m_source(std::move(source)), m_flags(std::move(flags)) {}

    const QRegularExpression &regex() const { return m_re; }
    const QString &source() const { return m_source; }
    const QString &flags() const { return m_flags; }

    bool matches(const QString &s) const { return m_re.match(s).hasMatch(); }

    QString type() const override { return QStringLiteral("Regex"); }
    bool isTruthy() const override { return true; }
    QString toString() const override;

    /// Parse `/body/flags`. Flags: g|i|m|s|u|y (preserved for round-trip
    /// but only `i` and `m` affect QRegularExpression). Returns nullptr
    /// on malformed input.
    static std::shared_ptr<RegExpValue> parseFromString(const QString &literal);

private:
    QRegularExpression m_re;
    QString m_source;
    QString m_flags;
};

/// 7-field calendar duration.
struct DurationComponents
{
    qint64 years = 0, months = 0, days = 0;
    qint64 hours = 0, minutes = 0, seconds = 0, milliseconds = 0;

    bool isZero() const
    {
        return !years && !months && !days && !hours
            && !minutes && !seconds && !milliseconds;
    }
};

class DurationValue : public Value
{
public:
    explicit DurationValue(DurationComponents c) : m_c(c) {}

    const DurationComponents &components() const { return m_c; }

    /// Approximation: years*365.25*86400000 + months*30*86400000 + ...
    /// Used by relational comparison and `Du - Du` reduction. Exact for
    /// zero-year/zero-month durations; approximate elsewhere.
    qint64 totalMilliseconds() const;

    QString type() const override { return QStringLiteral("Duration"); }
    bool isTruthy() const override { return !m_c.isZero(); }
    QString toString() const override;           // humanised
    bool equals(const Value &other) const override;
    bool looseEquals(const Value &other) const override;
    ValuePtr objectAccess(const QString &key) const override;
    QStringList keys() const override;

    /// Date arithmetic. Calendar-aware via QDate::addYears/addMonths/addDays.
    std::shared_ptr<DateValue> addToDate(const DateValue &d, bool subtract = false) const;

    /// Componentwise combine.
    DurationComponents plus(const DurationComponents &o) const;
    DurationComponents minus(const DurationComponents &o) const;
    DurationComponents timesScalar(double n) const;

    /// ISO-8601 PnYnMnWnDTnHnMnS or shorthand.
    /// Shorthand honours the case-sensitive `M`=months / `m`=minutes split
    /// and includes `ms` / `millisecond` / `milliseconds` (addendum §6.1,
    /// §14 divergence-from-docs note).
    static std::shared_ptr<DurationValue> parseFromString(const QString &text);

    /// Build from a millisecond count (for `D - D` reduction).
    static std::shared_ptr<DurationValue> fromMilliseconds(qint64 ms);

private:
    DurationComponents m_c;
};

}  // namespace Corbomite::Bases
