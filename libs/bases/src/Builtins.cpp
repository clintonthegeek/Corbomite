// SPDX-License-Identifier: GPL-3.0-or-later
//
// Built-in function catalog — addendum §8 (globals + per-type methods).
// Hard-cased `if`, `list.map/filter/reduce`, `object.map/filter` live in
// Evaluator.cpp — registrations in this file must not override them.
#include "corbomite/bases/FunctionRegistry.h"

#include "corbomite/bases/EvalContext.h"
#include "corbomite/bases/Values.h"
#include "corbomite/bases/VaultResolver.h"

#include "corbomite/core/MomentFormatter.h"
#include "corbomite/vault/TFile.h"

#include <QRandomGenerator>
#include <QUrl>

#include <cmath>

namespace Corbomite::Bases {

namespace {

std::shared_ptr<FormulaErrorValue> err(const QString &m)
{
    return std::make_shared<FormulaErrorValue>(m);
}

QString toStr(const ValuePtr &v)
{
    return v ? v->toString() : QString{};
}

double toNumOrZero(const ValuePtr &v)
{
    if (auto *n = dynamic_cast<NumberValue *>(v.get())) return n->data();
    return 0.0;
}

// --- global functions ---

void registerGlobals(FunctionRegistry &r)
{
    r.addGlobal({
        QStringLiteral("now"), {},
        [](const EvalContext &, const QVector<ValuePtr> &) -> ValuePtr {
            return std::make_shared<DateValue>(QDateTime::currentDateTime(), true);
        }, QStringLiteral("Current date + time.")});

    r.addGlobal({
        QStringLiteral("today"), {},
        [](const EvalContext &, const QVector<ValuePtr> &) -> ValuePtr {
            return std::make_shared<DateValue>(
                QDateTime(QDate::currentDate(), QTime(0, 0)), false);
        }, QStringLiteral("Today (time zeroed).")});

    r.addGlobal({
        QStringLiteral("date"),
        {requiredParam(QStringLiteral("str"), {typeid(StringValue)})},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            if (args.size() < 1) return err(QStringLiteral("date(str) missing arg"));
            auto d = DateValue::parseFromString(toStr(args[0]));
            return d ? std::static_pointer_cast<Value>(d) : err(QStringLiteral("invalid date"));
        }});

    r.addGlobal({
        QStringLiteral("random"), {},
        [](const EvalContext &, const QVector<ValuePtr> &) -> ValuePtr {
            return std::make_shared<NumberValue>(QRandomGenerator::global()->generateDouble());
        }});

    r.addGlobal({
        QStringLiteral("min"),
        {variadicTail(QStringLiteral("values"), {typeid(NumberValue)})},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            if (args.isEmpty()) return NullValue::instance();
            double m = std::numeric_limits<double>::infinity();
            for (const auto &v : args) {
                auto *n = dynamic_cast<NumberValue *>(v.get());
                if (!n) return err(QStringLiteral("min: non-number arg"));
                m = std::min(m, n->data());
            }
            return std::make_shared<NumberValue>(m);
        }});

    r.addGlobal({
        QStringLiteral("max"),
        {variadicTail(QStringLiteral("values"), {typeid(NumberValue)})},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            if (args.isEmpty()) return NullValue::instance();
            double m = -std::numeric_limits<double>::infinity();
            for (const auto &v : args) {
                auto *n = dynamic_cast<NumberValue *>(v.get());
                if (!n) return err(QStringLiteral("max: non-number arg"));
                m = std::max(m, n->data());
            }
            return std::make_shared<NumberValue>(m);
        }});

    r.addGlobal({
        QStringLiteral("list"),
        {requiredParam(QStringLiteral("elem"))},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            if (args.isEmpty()) return std::make_shared<ListValue>();
            // If already a list, return it unchanged.
            if (dynamic_cast<ListValue *>(args[0].get())) return args[0];
            return std::make_shared<ListValue>(QVector<ValuePtr>{args[0]});
        }});

    r.addGlobal({
        QStringLiteral("link"),
        {requiredParam(QStringLiteral("path")), optionalParam(QStringLiteral("display"))},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            if (args.isEmpty()) return err(QStringLiteral("link: missing path"));
            const QString path = toStr(args[0]);
            const QString display = args.size() > 1 ? toStr(args[1]) : QString{};
            return std::make_shared<LinkValue>(path, QString{}, display);
        }});

    r.addGlobal({
        QStringLiteral("number"),
        {requiredParam(QStringLiteral("x"))},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            if (args.isEmpty()) return err(QStringLiteral("number: missing arg"));
            const auto &v = args[0];
            if (!v) return NullValue::instance();
            if (auto *n = dynamic_cast<NumberValue *>(v.get())) return v;
            if (auto *b = dynamic_cast<BooleanValue *>(v.get()))
                return std::make_shared<NumberValue>(b->data() ? 1.0 : 0.0);
            if (auto *d = dynamic_cast<DateValue *>(v.get()))
                return std::make_shared<NumberValue>(
                    static_cast<double>(d->dateTime().toMSecsSinceEpoch()));
            if (auto *s = dynamic_cast<StringValue *>(v.get())) {
                bool ok = false;
                const double d = s->data().toDouble(&ok);
                if (!ok) return err(QStringLiteral("number(): cannot parse"));
                return std::make_shared<NumberValue>(d);
            }
            return err(QStringLiteral("number(): unsupported type"));
        }});

    r.addGlobal({
        QStringLiteral("duration"),
        {requiredParam(QStringLiteral("str"), {typeid(StringValue)})},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            if (args.isEmpty()) return err(QStringLiteral("duration: missing arg"));
            auto d = DurationValue::parseFromString(toStr(args[0]));
            return d ? std::static_pointer_cast<Value>(d)
                     : err(QStringLiteral("invalid duration"));
        }});

    r.addGlobal({
        QStringLiteral("image"),
        {requiredParam(QStringLiteral("path"))},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<ImageValue>(args.isEmpty() ? QString{} : toStr(args[0]));
        }});

    r.addGlobal({
        QStringLiteral("icon"),
        {requiredParam(QStringLiteral("name"), {typeid(StringValue)})},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<IconValue>(args.isEmpty() ? QString{} : toStr(args[0]));
        }});

    r.addGlobal({
        QStringLiteral("file"),
        {requiredParam(QStringLiteral("path"))},
        [](const EvalContext &ctx, const QVector<ValuePtr> &args) -> ValuePtr {
            if (const VaultResolver *v = ctx.vault())
                return v->fileAt(args.isEmpty() ? QString{} : toStr(args[0]));
            return NullValue::instance();
        }});

    r.addGlobal({
        QStringLiteral("html"),
        {requiredParam(QStringLiteral("str"), {typeid(StringValue)})},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<HTMLValue>(args.isEmpty() ? QString{} : toStr(args[0]));
        }});

    r.addGlobal({
        QStringLiteral("escapeHTML"),
        {requiredParam(QStringLiteral("str"), {typeid(StringValue)})},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            QString s = args.isEmpty() ? QString{} : toStr(args[0]);
            s.replace(QLatin1Char('&'), QLatin1String("&amp;"));
            s.replace(QLatin1Char('<'), QLatin1String("&lt;"));
            s.replace(QLatin1Char('>'), QLatin1String("&gt;"));
            s.replace(QLatin1Char('"'), QLatin1String("&quot;"));
            s.replace(QLatin1Char('\''), QLatin1String("&#39;"));
            return std::make_shared<StringValue>(s);
        }});
}

// --- per-Value methods (Value parent — applicable to any subclass) ---

void registerValueMethods(FunctionRegistry &r)
{
    r.addForType(typeid(Value), {
        QStringLiteral("toString"), {},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<StringValue>(toStr(args.value(0)));
        }});

    r.addForType(typeid(Value), {
        QStringLiteral("isTruthy"), {},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<BooleanValue>(args.value(0) && args[0]->isTruthy());
        }});

    r.addForType(typeid(Value), {
        QStringLiteral("isType"),
        {requiredParam(QStringLiteral("name"), {typeid(StringValue)})},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            const QString want = args.size() > 1 ? toStr(args[1]) : QString{};
            return std::make_shared<BooleanValue>(args.value(0) && args[0]->type() == want);
        }});

    r.addForType(typeid(Value), {
        QStringLiteral("isEmpty"), {},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<BooleanValue>(args.value(0) && args[0]->isEmpty());
        }});
}

// --- String methods ---

void registerStringMethods(FunctionRegistry &r)
{
    auto subject = [](const QVector<ValuePtr> &args) -> QString { return toStr(args.value(0)); };

    r.addForType(typeid(StringValue), { QStringLiteral("startsWith"),
        {requiredParam(QStringLiteral("q"), {typeid(StringValue)})},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<BooleanValue>(subject(args).startsWith(toStr(args.value(1))));
        }});
    r.addForType(typeid(StringValue), { QStringLiteral("endsWith"),
        {requiredParam(QStringLiteral("q"), {typeid(StringValue)})},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<BooleanValue>(subject(args).endsWith(toStr(args.value(1))));
        }});
    r.addForType(typeid(StringValue), { QStringLiteral("trim"), {},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<StringValue>(subject(args).trimmed());
        }});
    r.addForType(typeid(StringValue), { QStringLiteral("title"), {},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            QString s = subject(args);
            bool newWord = true;
            for (int i = 0; i < s.size(); ++i) {
                if (s[i].isSpace()) newWord = true;
                else { s[i] = newWord ? s[i].toUpper() : s[i]; newWord = false; }
            }
            return std::make_shared<StringValue>(s);
        }});
    r.addForType(typeid(StringValue), { QStringLiteral("replace"),
        {requiredParam(QStringLiteral("pattern")), requiredParam(QStringLiteral("replacement"), {typeid(StringValue)})},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            const QString repl = toStr(args.value(2));
            QString s = subject(args);
            if (auto *re = dynamic_cast<RegExpValue *>(args.value(1).get())) {
                s.replace(re->regex(), repl);
            } else {
                s.replace(toStr(args.value(1)), repl);
            }
            return std::make_shared<StringValue>(s);
        }});
    r.addForType(typeid(StringValue), { QStringLiteral("reverse"), {},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            QString s = subject(args);
            std::reverse(s.begin(), s.end());
            return std::make_shared<StringValue>(s);
        }});
    r.addForType(typeid(StringValue), { QStringLiteral("lower"), {},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<StringValue>(subject(args).toLower());
        }});
    r.addForType(typeid(StringValue), { QStringLiteral("split"),
        {requiredParam(QStringLiteral("sep")), optionalParam(QStringLiteral("n"), {typeid(NumberValue)})},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            const QString s = subject(args);
            QStringList parts;
            if (auto *re = dynamic_cast<RegExpValue *>(args.value(1).get())) {
                parts = s.split(re->regex());
            } else {
                parts = s.split(toStr(args.value(1)));
            }
            if (args.size() > 2) {
                int n = static_cast<int>(toNumOrZero(args[2]));
                if (n >= 0 && n < parts.size()) parts = parts.mid(0, n);
            }
            QVector<ValuePtr> out;
            for (const auto &p : parts) out.push_back(std::make_shared<StringValue>(p));
            return std::make_shared<ListValue>(out);
        }});
    r.addForType(typeid(StringValue), { QStringLiteral("contains"),
        {requiredParam(QStringLiteral("v"), {typeid(StringValue)})},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<BooleanValue>(subject(args).contains(toStr(args.value(1))));
        }});
    auto containsVariadic = [subject](bool all) {
        return [subject, all](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            const QString s = subject(args);
            for (int i = 1; i < args.size(); ++i) {
                const bool found = s.contains(toStr(args[i]));
                if (all && !found) return std::make_shared<BooleanValue>(false);
                if (!all && found) return std::make_shared<BooleanValue>(true);
            }
            return std::make_shared<BooleanValue>(all);
        };
    };
    r.addForType(typeid(StringValue), { QStringLiteral("containsAny"),
        {variadicTail(QStringLiteral("vs"), {typeid(StringValue)})}, containsVariadic(false)});
    r.addForType(typeid(StringValue), { QStringLiteral("containsAll"),
        {variadicTail(QStringLiteral("vs"), {typeid(StringValue)})}, containsVariadic(true)});
    r.addForType(typeid(StringValue), { QStringLiteral("slice"),
        {requiredParam(QStringLiteral("start"), {typeid(NumberValue)}),
         optionalParam(QStringLiteral("end"), {typeid(NumberValue)})},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            const QString s = subject(args);
            const int n = s.size();
            int start = args.size() > 1 ? static_cast<int>(toNumOrZero(args[1])) : 0;
            int end   = args.size() > 2 ? static_cast<int>(toNumOrZero(args[2])) : n;
            if (start < 0) start = std::max(0, n + start);
            if (end < 0)   end   = std::max(0, n + end);
            start = std::clamp(start, 0, n);
            end   = std::clamp(end, start, n);
            return std::make_shared<StringValue>(s.mid(start, end - start));
        }});
    r.addForType(typeid(StringValue), { QStringLiteral("repeat"),
        {requiredParam(QStringLiteral("n"), {typeid(NumberValue)})},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            const int n = args.size() > 1 ? static_cast<int>(toNumOrZero(args[1])) : 0;
            return std::make_shared<StringValue>(subject(args).repeated(std::max(0, n)));
        }});
    r.addForType(typeid(StringValue), { QStringLiteral("isEmpty"), {},
        [subject](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<BooleanValue>(subject(args).isEmpty());
        }});
}

// --- Number methods ---

void registerNumberMethods(FunctionRegistry &r)
{
    auto subj = [](const QVector<ValuePtr> &args) -> double {
        auto *n = dynamic_cast<NumberValue *>(args.value(0).get());
        return n ? n->data() : 0.0;
    };
    r.addForType(typeid(NumberValue), { QStringLiteral("round"),
        {optionalParam(QStringLiteral("digits"), {typeid(NumberValue)})},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            const double x = subj(args);
            if (args.size() <= 1) return std::make_shared<NumberValue>(std::round(x));
            const double d = toNumOrZero(args[1]);
            const double factor = std::pow(10.0, d);
            return std::make_shared<NumberValue>(std::round(x * factor) / factor);
        }});
    r.addForType(typeid(NumberValue), { QStringLiteral("ceil"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<NumberValue>(std::ceil(subj(args)));
        }});
    r.addForType(typeid(NumberValue), { QStringLiteral("floor"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<NumberValue>(std::floor(subj(args)));
        }});
    r.addForType(typeid(NumberValue), { QStringLiteral("abs"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            return std::make_shared<NumberValue>(std::fabs(subj(args)));
        }});
    r.addForType(typeid(NumberValue), { QStringLiteral("toFixed"),
        {requiredParam(QStringLiteral("precision"), {typeid(NumberValue)})},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            const int p = args.size() > 1 ? static_cast<int>(toNumOrZero(args[1])) : 0;
            return std::make_shared<StringValue>(QString::number(subj(args), 'f', p));
        }});
    r.addForType(typeid(NumberValue), { QStringLiteral("isEmpty"), {},
        [](const EvalContext &, const QVector<ValuePtr> &) -> ValuePtr {
            return std::make_shared<BooleanValue>(false);
        }});
}

// --- Date methods ---

void registerDateMethods(FunctionRegistry &r)
{
    auto subj = [](const QVector<ValuePtr> &args) -> DateValue * {
        return dynamic_cast<DateValue *>(args.value(0).get());
    };
    r.addForType(typeid(DateValue), { QStringLiteral("format"),
        {requiredParam(QStringLiteral("fmt"), {typeid(StringValue)})},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *d = subj(args);
            if (!d) return NullValue::instance();
            const QString fmt = toStr(args.value(1));
            return std::make_shared<StringValue>(
                Corbomite::MomentFormatter::format(d->dateTime(), fmt));
        }});
    r.addForType(typeid(DateValue), { QStringLiteral("date"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *d = subj(args);
            if (!d) return NullValue::instance();
            return std::make_shared<DateValue>(
                QDateTime(d->dateTime().date(), QTime(0, 0)), false);
        }});
    r.addForType(typeid(DateValue), { QStringLiteral("time"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *d = subj(args);
            if (!d) return NullValue::instance();
            return std::make_shared<StringValue>(d->dateTime().time().toString(QStringLiteral("HH:mm:ss")));
        }});
    r.addForType(typeid(DateValue), { QStringLiteral("relative"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *d = subj(args);
            if (!d) return NullValue::instance();
            RelativeDateValue rel(d->dateTime(), d->hasTime());
            return std::make_shared<StringValue>(rel.toString());
        }});
    r.addForType(typeid(DateValue), { QStringLiteral("isEmpty"), {},
        [](const EvalContext &, const QVector<ValuePtr> &) -> ValuePtr {
            return std::make_shared<BooleanValue>(false);
        }});
}

// --- List methods (non-lambda) ---

void registerListMethods(FunctionRegistry &r)
{
    auto subj = [](const QVector<ValuePtr> &args) -> ListValue * {
        return dynamic_cast<ListValue *>(args.value(0).get());
    };
    auto wrap0 = [subj](auto lv_method) {
        return [subj, lv_method](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *l = subj(args);
            return l ? (l->*lv_method)() : NullValue::instance();
        };
    };
    r.addForType(typeid(ListValue), { QStringLiteral("earliest"), {}, wrap0(&ListValue::earliest)});
    r.addForType(typeid(ListValue), { QStringLiteral("latest"),   {}, wrap0(&ListValue::latest)});
    r.addForType(typeid(ListValue), { QStringLiteral("median"),   {}, wrap0(&ListValue::median)});
    r.addForType(typeid(ListValue), { QStringLiteral("mean"),     {}, wrap0(&ListValue::mean)});
    r.addForType(typeid(ListValue), { QStringLiteral("max"),      {}, wrap0(&ListValue::max)});
    r.addForType(typeid(ListValue), { QStringLiteral("min"),      {}, wrap0(&ListValue::min)});
    r.addForType(typeid(ListValue), { QStringLiteral("sum"),      {}, wrap0(&ListValue::sum)});
    r.addForType(typeid(ListValue), { QStringLiteral("stddev"),   {}, wrap0(&ListValue::stddev)});

    r.addForType(typeid(ListValue), { QStringLiteral("join"),
        {requiredParam(QStringLiteral("sep"), {typeid(StringValue)})},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *l = subj(args);
            if (!l) return NullValue::instance();
            return std::make_shared<StringValue>(l->join(toStr(args.value(1))));
        }});
    r.addForType(typeid(ListValue), { QStringLiteral("reverse"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *l = subj(args);
            return l ? std::static_pointer_cast<Value>(l->reverse()) : NullValue::instance();
        }});
    r.addForType(typeid(ListValue), { QStringLiteral("flat"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *l = subj(args);
            return l ? std::static_pointer_cast<Value>(l->flatten()) : NullValue::instance();
        }});
    r.addForType(typeid(ListValue), { QStringLiteral("unique"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *l = subj(args);
            return l ? std::static_pointer_cast<Value>(l->unique()) : NullValue::instance();
        }});
    r.addForType(typeid(ListValue), { QStringLiteral("sort"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *l = subj(args);
            return l ? std::static_pointer_cast<Value>(l->sort()) : NullValue::instance();
        }});
    r.addForType(typeid(ListValue), { QStringLiteral("contains"),
        {requiredParam(QStringLiteral("v"))},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *l = subj(args);
            if (!l) return std::make_shared<BooleanValue>(false);
            return std::make_shared<BooleanValue>(l->includes(args.value(1)));
        }});
    auto containsVariadic = [subj](bool all) {
        return [subj, all](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *l = subj(args);
            if (!l) return std::make_shared<BooleanValue>(all);
            for (int i = 1; i < args.size(); ++i) {
                const bool found = l->includes(args[i]);
                if (all && !found) return std::make_shared<BooleanValue>(false);
                if (!all && found) return std::make_shared<BooleanValue>(true);
            }
            return std::make_shared<BooleanValue>(all);
        };
    };
    r.addForType(typeid(ListValue), { QStringLiteral("containsAny"),
        {variadicTail(QStringLiteral("vs"))}, containsVariadic(false)});
    r.addForType(typeid(ListValue), { QStringLiteral("containsAll"),
        {variadicTail(QStringLiteral("vs"))}, containsVariadic(true)});
    r.addForType(typeid(ListValue), { QStringLiteral("slice"),
        {requiredParam(QStringLiteral("start"), {typeid(NumberValue)}),
         optionalParam(QStringLiteral("end"), {typeid(NumberValue)})},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *l = subj(args);
            if (!l) return NullValue::instance();
            const int start = args.size() > 1 ? static_cast<int>(toNumOrZero(args[1])) : 0;
            const int end   = args.size() > 2 ? static_cast<int>(toNumOrZero(args[2])) : -1;
            return l->slice(start, end);
        }});
    r.addForType(typeid(ListValue), { QStringLiteral("isEmpty"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *l = subj(args);
            return std::make_shared<BooleanValue>(!l || l->isEmpty());
        }});
}

// --- Object methods (non-lambda) ---

void registerObjectMethods(FunctionRegistry &r)
{
    auto subj = [](const QVector<ValuePtr> &args) -> ObjectValue * {
        return dynamic_cast<ObjectValue *>(args.value(0).get());
    };
    r.addForType(typeid(ObjectValue), { QStringLiteral("isEmpty"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *o = subj(args);
            return std::make_shared<BooleanValue>(!o || o->isEmpty());
        }});
    r.addForType(typeid(ObjectValue), { QStringLiteral("keys"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *o = subj(args);
            if (!o) return std::make_shared<ListValue>();
            QVector<ValuePtr> out;
            for (const auto &k : o->keys()) out.push_back(std::make_shared<StringValue>(k));
            return std::make_shared<ListValue>(out);
        }});
    r.addForType(typeid(ObjectValue), { QStringLiteral("values"), {},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *o = subj(args);
            if (!o) return std::make_shared<ListValue>();
            return std::make_shared<ListValue>(o->values());
        }});
}

// --- RegExp methods ---

void registerRegexMethods(FunctionRegistry &r)
{
    r.addForType(typeid(RegExpValue), { QStringLiteral("matches"),
        {requiredParam(QStringLiteral("s"), {typeid(StringValue)})},
        [](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *re = dynamic_cast<RegExpValue *>(args.value(0).get());
            if (!re) return std::make_shared<BooleanValue>(false);
            return std::make_shared<BooleanValue>(re->matches(toStr(args.value(1))));
        }});
}

// --- Link methods ---

void registerLinkMethods(FunctionRegistry &r)
{
    auto subj = [](const QVector<ValuePtr> &args) -> LinkValue * {
        return dynamic_cast<LinkValue *>(args.value(0).get());
    };
    r.addForType(typeid(LinkValue), { QStringLiteral("asFile"), {},
        [subj](const EvalContext &ctx, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *lv = subj(args);
            if (!lv) return NullValue::instance();
            if (const VaultResolver *v = ctx.vault()) {
                const QString path = v->resolveLinkTarget(lv->data(), lv->sourcePath());
                if (!path.isEmpty()) return v->fileAt(path);
            }
            return NullValue::instance();
        }});
    r.addForType(typeid(LinkValue), { QStringLiteral("linksTo"),
        {requiredParam(QStringLiteral("other"))},
        [subj](const EvalContext &ctx, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *lv = subj(args);
            if (!lv) return std::make_shared<BooleanValue>(false);
            const QString other = toStr(args.value(1));
            if (const VaultResolver *v = ctx.vault()) {
                const QString tgt = v->resolveLinkTarget(lv->data(), lv->sourcePath());
                const QString oth = v->resolveLinkTarget(other, lv->sourcePath());
                return std::make_shared<BooleanValue>(!tgt.isEmpty() && tgt == oth);
            }
            return std::make_shared<BooleanValue>(lv->data() == other);
        }});
}

// --- File methods ---

void registerFileMethods(FunctionRegistry &r)
{
    auto subj = [](const QVector<ValuePtr> &args) -> FileValue * {
        return dynamic_cast<FileValue *>(args.value(0).get());
    };
    r.addForType(typeid(FileValue), { QStringLiteral("asLink"),
        {optionalParam(QStringLiteral("display"), {typeid(StringValue)})},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *f = subj(args);
            if (!f || !f->file()) return NullValue::instance();
            const QString display = args.size() > 1 ? toStr(args[1]) : QString{};
            return std::make_shared<LinkValue>(f->file()->path, QString{}, display);
        }});
    r.addForType(typeid(FileValue), { QStringLiteral("hasLink"),
        {requiredParam(QStringLiteral("other"))},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *f = subj(args);
            return std::make_shared<BooleanValue>(f && f->hasLink(args.value(1)));
        }});
    r.addForType(typeid(FileValue), { QStringLiteral("inFolder"),
        {requiredParam(QStringLiteral("path"), {typeid(StringValue)})},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *f = subj(args);
            return std::make_shared<BooleanValue>(f && f->inFolder(toStr(args.value(1))));
        }});
    r.addForType(typeid(FileValue), { QStringLiteral("hasTag"),
        {variadicTail(QStringLiteral("tags"), {typeid(StringValue)})},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *f = subj(args);
            if (!f) return std::make_shared<BooleanValue>(false);
            QStringList tags;
            for (int i = 1; i < args.size(); ++i) tags << toStr(args[i]);
            return std::make_shared<BooleanValue>(f->hasTag(tags));
        }});
    r.addForType(typeid(FileValue), { QStringLiteral("hasProperty"),
        {requiredParam(QStringLiteral("name"), {typeid(StringValue)})},
        [subj](const EvalContext &, const QVector<ValuePtr> &args) -> ValuePtr {
            auto *f = subj(args);
            return std::make_shared<BooleanValue>(f && f->hasProperty(toStr(args.value(1))));
        }});
}

}  // namespace

void registerBuiltins(FunctionRegistry &r)
{
    registerGlobals(r);
    registerValueMethods(r);
    registerStringMethods(r);
    registerNumberMethods(r);
    registerDateMethods(r);
    registerListMethods(r);
    registerObjectMethods(r);
    registerRegexMethods(r);
    registerLinkMethods(r);
    registerFileMethods(r);
}

}  // namespace Corbomite::Bases
