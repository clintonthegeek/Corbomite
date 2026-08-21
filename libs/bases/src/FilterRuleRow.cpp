// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/FilterRuleRow.h"

#include "corbomite/bases/FormulaInput.h"

#include <KLocalizedString>

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QIcon>
#include <QLineEdit>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QToolButton>

namespace Corbomite::Bases {

namespace {

enum class OpKind {
    IsNotEmpty, IsEmpty,
    Eq, Neq, Contains, NotContains, StartsWith, EndsWith,
    NumEq, NumNeq, Gt, Lt, Gte, Lte,
    IsTrue, IsFalse,
    DateOn, DateBefore, DateAfter, DateOnOrBefore, DateOnOrAfter,
    ListContains, ListNotContains,
};

enum class ValueKind { None, Text, Number, Date };

ValueKind valueKindFor(OpKind k)
{
    switch (k) {
    case OpKind::Eq: case OpKind::Neq:
    case OpKind::Contains: case OpKind::NotContains:
    case OpKind::StartsWith: case OpKind::EndsWith:
    case OpKind::ListContains: case OpKind::ListNotContains:
        return ValueKind::Text;
    case OpKind::NumEq: case OpKind::NumNeq:
    case OpKind::Gt: case OpKind::Lt: case OpKind::Gte: case OpKind::Lte:
        return ValueKind::Number;
    case OpKind::DateOn: case OpKind::DateBefore: case OpKind::DateAfter:
    case OpKind::DateOnOrBefore: case OpKind::DateOnOrAfter:
        return ValueKind::Date;
    default:
        return ValueKind::None;
    }
}

QString operatorLabel(OpKind k)
{
    switch (k) {
    case OpKind::IsNotEmpty:       return i18nc("@item:inlistbox filter operator", "is not empty");
    case OpKind::IsEmpty:          return i18nc("@item:inlistbox filter operator", "is empty");
    case OpKind::Eq:               return i18nc("@item:inlistbox filter operator", "is");
    case OpKind::Neq:              return i18nc("@item:inlistbox filter operator", "is not");
    case OpKind::Contains:         return i18nc("@item:inlistbox filter operator", "contains");
    case OpKind::NotContains:      return i18nc("@item:inlistbox filter operator", "does not contain");
    case OpKind::StartsWith:       return i18nc("@item:inlistbox filter operator", "starts with");
    case OpKind::EndsWith:         return i18nc("@item:inlistbox filter operator", "ends with");
    case OpKind::NumEq:            return i18nc("@item:inlistbox filter operator", "is equal to");
    case OpKind::NumNeq:           return i18nc("@item:inlistbox filter operator", "is not equal to");
    case OpKind::Gt:               return i18nc("@item:inlistbox filter operator", "is greater than");
    case OpKind::Lt:               return i18nc("@item:inlistbox filter operator", "is less than");
    case OpKind::Gte:              return i18nc("@item:inlistbox filter operator", "is greater than or equal to");
    case OpKind::Lte:              return i18nc("@item:inlistbox filter operator", "is less than or equal to");
    case OpKind::IsTrue:           return i18nc("@item:inlistbox filter operator", "is true");
    case OpKind::IsFalse:          return i18nc("@item:inlistbox filter operator", "is false");
    case OpKind::DateOn:           return i18nc("@item:inlistbox filter operator", "is on");
    case OpKind::DateBefore:       return i18nc("@item:inlistbox filter operator", "is before");
    case OpKind::DateAfter:        return i18nc("@item:inlistbox filter operator", "is after");
    case OpKind::DateOnOrBefore:   return i18nc("@item:inlistbox filter operator", "is on or before");
    case OpKind::DateOnOrAfter:    return i18nc("@item:inlistbox filter operator", "is on or after");
    case OpKind::ListContains:     return i18nc("@item:inlistbox filter operator", "contains");
    case OpKind::ListNotContains:  return i18nc("@item:inlistbox filter operator", "does not contain");
    }
    return {};
}

QVector<OpKind> operatorsForType(const QString &valueType)
{
    QVector<OpKind> ops{ OpKind::IsNotEmpty, OpKind::IsEmpty };
    if (valueType == QLatin1String("Number")) {
        ops += { OpKind::NumEq, OpKind::NumNeq, OpKind::Gt, OpKind::Lt, OpKind::Gte, OpKind::Lte };
    } else if (valueType == QLatin1String("Boolean")) {
        ops += { OpKind::IsTrue, OpKind::IsFalse };
    } else if (valueType == QLatin1String("Date")) {
        ops += { OpKind::DateOn, OpKind::DateBefore, OpKind::DateAfter,
                 OpKind::DateOnOrBefore, OpKind::DateOnOrAfter };
    } else if (valueType == QLatin1String("List")) {
        ops += { OpKind::ListContains, OpKind::ListNotContains };
    } else {
        // String / Tag / Link / File / Object / default (unknown-type fallback).
        ops += { OpKind::Eq, OpKind::Neq, OpKind::Contains, OpKind::NotContains,
                 OpKind::StartsWith, OpKind::EndsWith };
    }
    return ops;
}

QString escapeFormulaString(QString s)
{
    s.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    s.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return s;
}

bool isValidIdentifier(const QString &s)
{
    if (s.isEmpty()) return false;
    const QChar c0 = s.at(0);
    if (!(c0.isLetter() || c0 == QLatin1Char('_') || c0 == QLatin1Char('$'))) return false;
    for (int i = 1; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (!(c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('$'))) return false;
    }
    return true;
}

/// The formula-source access token for a property — dot form (`note.Key`)
/// when the frontmatter key is a valid bare identifier, bracket form
/// (`note["My Key"]`) otherwise (audit: "Access object elements using
/// square brackets and the element name").
QString propertyAccessToken(const PropertyId &id)
{
    QString root;
    switch (id.kind) {
    case PropertyKind::Note:    root = QStringLiteral("note");    break;
    case PropertyKind::File:    root = QStringLiteral("file");    break;
    case PropertyKind::Formula: root = QStringLiteral("formula"); break;
    }
    if (isValidIdentifier(id.name))
        return root + QLatin1Char('.') + id.name;
    return root + QStringLiteral("[\"") + escapeFormulaString(id.name) + QStringLiteral("\"]");
}

QString synthesize(const QString &token, OpKind kind, const QString &value)
{
    switch (kind) {
    case OpKind::IsNotEmpty:      return QStringLiteral("!%1.isEmpty()").arg(token);
    case OpKind::IsEmpty:         return QStringLiteral("%1.isEmpty()").arg(token);
    case OpKind::Eq:              return QStringLiteral("%1 == \"%2\"").arg(token, escapeFormulaString(value));
    case OpKind::Neq:             return QStringLiteral("%1 != \"%2\"").arg(token, escapeFormulaString(value));
    case OpKind::Contains:
    case OpKind::ListContains:    return QStringLiteral("%1.contains(\"%2\")").arg(token, escapeFormulaString(value));
    case OpKind::NotContains:
    case OpKind::ListNotContains: return QStringLiteral("!%1.contains(\"%2\")").arg(token, escapeFormulaString(value));
    case OpKind::StartsWith:      return QStringLiteral("%1.startsWith(\"%2\")").arg(token, escapeFormulaString(value));
    case OpKind::EndsWith:        return QStringLiteral("%1.endsWith(\"%2\")").arg(token, escapeFormulaString(value));
    case OpKind::NumEq:           return QStringLiteral("%1 == %2").arg(token, value);
    case OpKind::NumNeq:          return QStringLiteral("%1 != %2").arg(token, value);
    case OpKind::Gt:              return QStringLiteral("%1 > %2").arg(token, value);
    case OpKind::Lt:              return QStringLiteral("%1 < %2").arg(token, value);
    case OpKind::Gte:             return QStringLiteral("%1 >= %2").arg(token, value);
    case OpKind::Lte:             return QStringLiteral("%1 <= %2").arg(token, value);
    case OpKind::IsTrue:          return QStringLiteral("%1 == true").arg(token);
    case OpKind::IsFalse:         return QStringLiteral("%1 == false").arg(token);
    case OpKind::DateOn:          return QStringLiteral("%1.date() == date(\"%2\")").arg(token, value);
    case OpKind::DateBefore:      return QStringLiteral("%1 < date(\"%2\")").arg(token, value);
    case OpKind::DateAfter:       return QStringLiteral("%1 > date(\"%2\")").arg(token, value);
    case OpKind::DateOnOrBefore:  return QStringLiteral("%1 <= date(\"%2\")").arg(token, value);
    case OpKind::DateOnOrAfter:   return QStringLiteral("%1 >= date(\"%2\")").arg(token, value);
    }
    return {};
}

const QString kStrValue = QStringLiteral("((?:[^\"\\\\]|\\\\.)*)");  // matches a possibly-escaped "..." body
const QString kNumValue = QStringLiteral("(-?\\d+(?:\\.\\d+)?)");
const QString kDateValue = QStringLiteral("([\\d-]+)");

/// (OpKind, regex-with-TOKEN-placeholder, capture-group-is-escaped-string).
/// Order matters only where two patterns could both match the same text —
/// they can't here, since every pair differs by a literal marker
/// (leading `!`, or the exact comparator/quote shape).
struct ParsePattern { OpKind kind; QString pattern; bool escapedCapture; };
const QVector<ParsePattern> &parsePatterns()
{
    static const QVector<ParsePattern> kPatterns = {
        { OpKind::IsNotEmpty, QStringLiteral("^!TOKEN\\.isEmpty\\(\\)$"), false },
        { OpKind::IsEmpty,    QStringLiteral("^TOKEN\\.isEmpty\\(\\)$"), false },
        { OpKind::IsTrue,     QStringLiteral("^TOKEN == true$"), false },
        { OpKind::IsFalse,    QStringLiteral("^TOKEN == false$"), false },
        { OpKind::DateOn,         QStringLiteral("^TOKEN\\.date\\(\\) == date\\(\"") + kDateValue + QStringLiteral("\"\\)$"), false },
        { OpKind::DateOnOrBefore, QStringLiteral("^TOKEN <= date\\(\"") + kDateValue + QStringLiteral("\"\\)$"), false },
        { OpKind::DateOnOrAfter,  QStringLiteral("^TOKEN >= date\\(\"") + kDateValue + QStringLiteral("\"\\)$"), false },
        { OpKind::DateBefore,     QStringLiteral("^TOKEN < date\\(\"") + kDateValue + QStringLiteral("\"\\)$"), false },
        { OpKind::DateAfter,      QStringLiteral("^TOKEN > date\\(\"") + kDateValue + QStringLiteral("\"\\)$"), false },
        { OpKind::Eq,          QStringLiteral("^TOKEN == \"") + kStrValue + QStringLiteral("\"$"), true },
        { OpKind::Neq,         QStringLiteral("^TOKEN != \"") + kStrValue + QStringLiteral("\"$"), true },
        { OpKind::NotContains, QStringLiteral("^!TOKEN\\.contains\\(\"") + kStrValue + QStringLiteral("\"\\)$"), true },
        { OpKind::Contains,    QStringLiteral("^TOKEN\\.contains\\(\"") + kStrValue + QStringLiteral("\"\\)$"), true },
        { OpKind::StartsWith,  QStringLiteral("^TOKEN\\.startsWith\\(\"") + kStrValue + QStringLiteral("\"\\)$"), true },
        { OpKind::EndsWith,    QStringLiteral("^TOKEN\\.endsWith\\(\"") + kStrValue + QStringLiteral("\"\\)$"), true },
        { OpKind::Gte,    QStringLiteral("^TOKEN >= ") + kNumValue + QStringLiteral("$"), false },
        { OpKind::Lte,    QStringLiteral("^TOKEN <= ") + kNumValue + QStringLiteral("$"), false },
        { OpKind::Gt,     QStringLiteral("^TOKEN > ")  + kNumValue + QStringLiteral("$"), false },
        { OpKind::Lt,     QStringLiteral("^TOKEN < ")  + kNumValue + QStringLiteral("$"), false },
        { OpKind::NumEq,  QStringLiteral("^TOKEN == ") + kNumValue + QStringLiteral("$"), false },
        { OpKind::NumNeq, QStringLiteral("^TOKEN != ") + kNumValue + QStringLiteral("$"), false },
    };
    return kPatterns;
}

QString unescapeFormulaString(QString s)
{
    s.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    s.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return s;
}

}  // namespace

FilterRuleRow::FilterRuleRow(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_modeStack = new QStackedWidget(this);

    // --- Simple page ---
    auto *simple = new QWidget(m_modeStack);
    auto *simpleLayout = new QHBoxLayout(simple);
    simpleLayout->setContentsMargins(0, 0, 0, 0);

    m_propCombo = new QComboBox(simple);
    m_propCombo->setObjectName(QStringLiteral("propertyCombo"));
    simpleLayout->addWidget(m_propCombo, 1);

    m_opCombo = new QComboBox(simple);
    m_opCombo->setObjectName(QStringLiteral("operatorCombo"));
    simpleLayout->addWidget(m_opCombo);

    m_valueStack = new QStackedWidget(simple);
    m_textValue = new QLineEdit(m_valueStack);
    m_textValue->setObjectName(QStringLiteral("valueText"));
    m_dateValue = new QDateEdit(m_valueStack);
    m_dateValue->setObjectName(QStringLiteral("valueDate"));
    m_dateValue->setCalendarPopup(true);
    m_dateValue->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_dateValue->setDate(QDate::currentDate());
    m_valueStack->addWidget(m_textValue);  // page 0
    m_valueStack->addWidget(m_dateValue);  // page 1
    simpleLayout->addWidget(m_valueStack, 1);

    m_modeStack->addWidget(simple);  // page 0

    // --- Advanced page ---
    m_advanced = new FormulaInput(m_modeStack);
    m_modeStack->addWidget(m_advanced);  // page 1

    root->addWidget(m_modeStack, 1);

    m_modeToggle = new QToolButton(this);
    m_modeToggle->setObjectName(QStringLiteral("advancedModeToggle"));
    m_modeToggle->setCheckable(true);
    m_modeToggle->setIcon(QIcon::fromTheme(QStringLiteral("code-context")));
    m_modeToggle->setToolTip(i18nc("@action:button", "Advanced (raw formula)"));
    root->addWidget(m_modeToggle);

    rebuildOperatorCombo();
    rebuildValueEditor();

    connect(m_propCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { onPropertyChanged(); });
    connect(m_opCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { onOperatorChanged(); });
    connect(m_textValue, &QLineEdit::textChanged, this, [this](const QString &) { onValueEdited(); });
    connect(m_dateValue, &QDateEdit::dateChanged, this, [this](const QDate &) { onValueEdited(); });
    connect(m_advanced, &QLineEdit::textChanged, this, [this](const QString &) { onAnyChange(); });
    connect(m_advanced, &FormulaInput::validityChanged, this, [this](bool) { onAnyChange(); });
    connect(m_modeToggle, &QToolButton::toggled, this, [this](bool advanced) { setSimpleMode(!advanced); });
}

void FilterRuleRow::setProperties(const QVector<FilterPropertyInfo> &props)
{
    m_syncing = true;
    m_props = props;
    m_propCombo->clear();
    for (const FilterPropertyInfo &p : m_props)
        m_propCombo->addItem(p.displayName);
    m_syncing = false;

    if (m_props.isEmpty()) {
        // Nothing to point-and-click at — force advanced mode.
        m_modeToggle->setChecked(true);
        m_modeToggle->setEnabled(false);
    } else {
        m_modeToggle->setEnabled(true);
        rebuildOperatorCombo();
        rebuildValueEditor();
    }
}

void FilterRuleRow::setCandidates(const QStringList &candidates)
{
    m_candidates = candidates;
    m_advanced->setCandidates(candidates);
}

void FilterRuleRow::rebuildOperatorCombo(int extraKindToInclude)
{
    const int propIdx = m_propCombo->currentIndex();
    const QString type = (propIdx >= 0 && propIdx < m_props.size())
        ? m_props.at(propIdx).valueType : QStringLiteral("String");

    QVector<OpKind> ops = operatorsForType(type);
    if (extraKindToInclude >= 0) {
        const auto extra = static_cast<OpKind>(extraKindToInclude);
        if (!ops.contains(extra)) ops.push_back(extra);
    }

    const bool wasSyncing = m_syncing;
    m_syncing = true;
    m_opCombo->clear();
    for (OpKind k : ops)
        m_opCombo->addItem(operatorLabel(k), int(k));
    if (extraKindToInclude >= 0) {
        const int idx = m_opCombo->findData(extraKindToInclude);
        if (idx >= 0) m_opCombo->setCurrentIndex(idx);
    }
    m_syncing = wasSyncing;
}

void FilterRuleRow::rebuildValueEditor()
{
    const OpKind kind = static_cast<OpKind>(m_opCombo->currentData().toInt());
    const ValueKind vk = valueKindFor(kind);
    m_valueStack->setVisible(vk != ValueKind::None);
    if (vk == ValueKind::Date) {
        m_valueStack->setCurrentWidget(m_dateValue);
    } else {
        m_valueStack->setCurrentWidget(m_textValue);
        if (vk == ValueKind::Number)
            m_textValue->setValidator(new QDoubleValidator(m_textValue));
        else
            m_textValue->setValidator(nullptr);
    }
}

void FilterRuleRow::onPropertyChanged()
{
    if (m_syncing) return;
    rebuildOperatorCombo();
    rebuildValueEditor();
    onAnyChange();
}

void FilterRuleRow::onOperatorChanged()
{
    if (m_syncing) return;
    rebuildValueEditor();
    onAnyChange();
}

void FilterRuleRow::onValueEdited()
{
    if (m_syncing) return;
    onAnyChange();
}

QString FilterRuleRow::expression() const
{
    if (!m_simple) return m_advanced->text();

    const int propIdx = m_propCombo->currentIndex();
    if (propIdx < 0 || propIdx >= m_props.size()) return {};
    const QString token = propertyAccessToken(m_props.at(propIdx).id);
    const OpKind kind = static_cast<OpKind>(m_opCombo->currentData().toInt());
    const ValueKind vk = valueKindFor(kind);

    QString value;
    if (vk == ValueKind::Date) value = m_dateValue->date().toString(Qt::ISODate);
    else if (vk != ValueKind::None) value = m_textValue->text();

    return synthesize(token, kind, value);
}

bool FilterRuleRow::tryParseSimple(const QString &expr)
{
    const QString trimmed = expr.trimmed();
    if (trimmed.isEmpty()) return true;  // blank leaf: stays in (empty) simple mode, dropped by toFilter

    for (int propIdx = 0; propIdx < m_props.size(); ++propIdx) {
        const QString token = propertyAccessToken(m_props.at(propIdx).id);
        const QString escapedToken = QRegularExpression::escape(token);
        for (const ParsePattern &pp : parsePatterns()) {
            QString pattern = pp.pattern;
            pattern.replace(QStringLiteral("TOKEN"), escapedToken);
            const QRegularExpression re(pattern);
            const QRegularExpressionMatch m = re.match(trimmed);
            if (!m.hasMatch()) continue;

            m_syncing = true;
            m_propCombo->setCurrentIndex(propIdx);
            rebuildOperatorCombo(int(pp.kind));
            const int opIdx = m_opCombo->findData(int(pp.kind));
            if (opIdx >= 0) m_opCombo->setCurrentIndex(opIdx);
            rebuildValueEditor();
            if (m.lastCapturedIndex() >= 1) {
                const QString raw = m.captured(1);
                if (pp.escapedCapture) {
                    m_textValue->setText(unescapeFormulaString(raw));
                } else if (valueKindFor(pp.kind) == ValueKind::Date) {
                    m_dateValue->setDate(QDate::fromString(raw, Qt::ISODate));
                } else {
                    m_textValue->setText(raw);
                }
            }
            m_syncing = false;
            return true;
        }
    }
    return false;
}

void FilterRuleRow::setExpression(const QString &expr)
{
    m_syncing = true;
    m_advanced->setText(expr);
    m_syncing = false;

    const bool parsed = !m_props.isEmpty() && tryParseSimple(expr);
    setSimpleMode(parsed);
    m_lastValid = isExpressionValid();
}

void FilterRuleRow::setSimpleMode(bool simple)
{
    simple = simple && !m_props.isEmpty();
    m_simple = simple;
    m_modeStack->setCurrentIndex(simple ? 0 : 1);
    const bool wasSyncing = m_syncing;
    m_syncing = true;
    m_modeToggle->setChecked(!simple);
    m_syncing = wasSyncing;
    if (!simple && m_advanced->text().isEmpty()) {
        // Switched to advanced with nothing typed yet — seed it from the
        // simple fields' current (possibly empty) synthesis so the user
        // isn't staring at a blank box that silently drops their picks.
        m_advanced->setText(expression());
    }
}

bool FilterRuleRow::isExpressionValid() const
{
    if (!m_simple) return m_advanced->isExpressionValid();

    const int propIdx = m_propCombo->currentIndex();
    if (propIdx < 0) return m_props.isEmpty();  // nothing to select yet == neutral/empty
    const OpKind kind = static_cast<OpKind>(m_opCombo->currentData().toInt());
    if (valueKindFor(kind) == ValueKind::Number) {
        bool ok = false;
        m_textValue->text().toDouble(&ok);
        return ok;
    }
    return true;
}

void FilterRuleRow::onAnyChange()
{
    if (m_syncing) return;
    Q_EMIT changed();
    const bool v = isExpressionValid();
    if (v != m_lastValid) {
        m_lastValid = v;
        Q_EMIT validityChanged(v);
    }
}

}  // namespace Corbomite::Bases
