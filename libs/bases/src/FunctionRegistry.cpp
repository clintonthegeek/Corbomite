// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FunctionRegistry.h"

#include "corbomite/bases/Values.h"

namespace Corbomite::Bases {

FnParam requiredParam(QString name, QVector<std::type_index> types)
{
    return FnParam{std::move(name), std::move(types), /*optional=*/false, /*variadic=*/false};
}

FnParam optionalParam(QString name, QVector<std::type_index> types)
{
    return FnParam{std::move(name), std::move(types), /*optional=*/true, /*variadic=*/false};
}

FnParam variadicTail(QString name, QVector<std::type_index> types)
{
    return FnParam{std::move(name), std::move(types), /*optional=*/false, /*variadic=*/true};
}

void FunctionRegistry::addGlobal(BasesFunction fn)
{
    m_global.insert(fn.name.toLower(), std::move(fn));
}

void FunctionRegistry::addForType(std::type_index valueClass, BasesFunction fn)
{
    m_byType[valueClass].insert(fn.name.toLower(), std::move(fn));
}

const BasesFunction *FunctionRegistry::findGlobal(const QString &name) const
{
    auto it = m_global.constFind(name.toLower());
    return it != m_global.constEnd() ? &it.value() : nullptr;
}

namespace {

// Given a live subject, walk its class chain from most-derived to
// Value-base. Order matches addendum §1 hierarchy + §8 dispatch rules.
QVector<std::type_index> classChain(const Value *subject)
{
    if (!subject) return {};
    const QString t = subject->type();
    // Most-derived first.
    if (t == QLatin1String("Null"))     return { typeid(NullValue), typeid(Value) };
    if (t == QLatin1String("Boolean"))  return { typeid(BooleanValue), typeid(Value) };
    if (t == QLatin1String("Number"))   return { typeid(NumberValue), typeid(Value) };
    if (t == QLatin1String("String"))   return { typeid(StringValue), typeid(Value) };
    if (t == QLatin1String("Tag"))      return { typeid(TagValue), typeid(StringValue), typeid(Value) };
    if (t == QLatin1String("Link"))     return { typeid(LinkValue), typeid(StringValue), typeid(Value) };
    if (t == QLatin1String("URL"))      return { typeid(UrlValue), typeid(StringValue), typeid(Value) };
    if (t == QLatin1String("Icon"))     return { typeid(IconValue), typeid(StringValue), typeid(Value) };
    if (t == QLatin1String("Image"))    return { typeid(ImageValue), typeid(StringValue), typeid(Value) };
    if (t == QLatin1String("HTML"))     return { typeid(HTMLValue), typeid(StringValue), typeid(Value) };
    if (t == QLatin1String("Markdown")) return { typeid(MarkdownValue), typeid(StringValue), typeid(Value) };
    if (t == QLatin1String("Date"))     return { typeid(DateValue), typeid(Value) };
    if (t == QLatin1String("Duration")) return { typeid(DurationValue), typeid(Value) };
    if (t == QLatin1String("List"))     return { typeid(ListValue), typeid(Value) };
    if (t == QLatin1String("Object"))   return { typeid(ObjectValue), typeid(Value) };
    if (t == QLatin1String("File"))     return { typeid(FileValue), typeid(Value) };
    if (t == QLatin1String("ThisFile")) return { typeid(ThisFileValue), typeid(FileValue), typeid(Value) };
    if (t == QLatin1String("Regex"))    return { typeid(RegExpValue), typeid(Value) };
    if (t == QLatin1String("Error"))    return { typeid(FormulaErrorValue), typeid(Value) };
    return { typeid(Value) };
}

}  // namespace

const BasesFunction *FunctionRegistry::findInstance(const Value *subject,
                                                    const QString &name) const
{
    const QString lname = name.toLower();
    for (const auto &cls : classChain(subject)) {
        auto outer = m_byType.constFind(cls);
        if (outer == m_byType.constEnd()) continue;
        auto inner = outer->constFind(lname);
        if (inner != outer->constEnd()) return &inner.value();
    }
    return findGlobal(name);
}

void FunctionRegistry::removeGlobal(const QString &name)
{
    m_global.remove(name.toLower());
}

void FunctionRegistry::removeForType(std::type_index valueClass, const QString &name)
{
    auto it = m_byType.find(valueClass);
    if (it == m_byType.end()) return;
    it->remove(name.toLower());
    if (it->isEmpty()) m_byType.erase(it);
}

FunctionRegistry &FunctionRegistry::global()
{
    static FunctionRegistry s_instance = []() {
        FunctionRegistry r;
        registerBuiltins(r);
        return r;
    }();
    return s_instance;
}

}  // namespace Corbomite::Bases
