// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterPropertyInfo.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QStackedWidget;
class QLineEdit;
class QDateEdit;
class QToolButton;

namespace Corbomite::Bases {

class FormulaInput;

/// One filter leaf's editor. Defaults to a point-and-click "simple" mode —
/// Property / Operator / Value dropdowns, the operator list re-populated
/// per the selected property's inferred type (see FilterPropertyInfo) — that
/// synthesizes a formula-expression string under the hood. A toggle button
/// switches to "advanced" mode: a bare FormulaInput text box for expressions
/// the simple editor can't represent (mirrors Obsidian's own point-and-click
/// filter UI + advanced-editor escape hatch).
///
/// setExpression() tries to reverse-parse the incoming text against the
/// fixed set of templates the simple editor itself can produce; a match
/// switches to simple mode with the fields populated, a non-match falls back
/// to advanced mode showing the raw text verbatim — the fallback is always
/// lossless.
class FilterRuleRow : public QWidget
{
    Q_OBJECT
public:
    explicit FilterRuleRow(QWidget *parent = nullptr);

    /// Property list for the "simple" mode dropdown. Also (re)enables
    /// simple mode if it was force-disabled by a previously-empty list.
    void setProperties(const QVector<FilterPropertyInfo> &props);

    /// Advanced-mode (FormulaInput) autocomplete tokens.
    void setCandidates(const QStringList &candidates);

    void setExpression(const QString &expr);
    QString expression() const;

    bool isExpressionValid() const;

Q_SIGNALS:
    void changed();
    void validityChanged(bool valid);

private:
    void rebuildOperatorCombo(int extraKindToInclude = -1);
    void rebuildValueEditor();
    bool tryParseSimple(const QString &expr);
    void setSimpleMode(bool simple);
    void onPropertyChanged();
    void onOperatorChanged();
    void onValueEdited();
    void onAnyChange();

    QVector<FilterPropertyInfo> m_props;
    QStringList m_candidates;

    QToolButton *m_modeToggle = nullptr;
    QStackedWidget *m_modeStack = nullptr;

    // Simple mode (page 0).
    QComboBox *m_propCombo = nullptr;
    QComboBox *m_opCombo = nullptr;
    QStackedWidget *m_valueStack = nullptr;
    QLineEdit *m_textValue = nullptr;
    QDateEdit *m_dateValue = nullptr;

    // Advanced mode (page 1).
    FormulaInput *m_advanced = nullptr;

    bool m_simple = true;
    bool m_lastValid = true;
    bool m_syncing = false;  // guards re-entrant onAnyChange during programmatic rebuilds
};

}  // namespace Corbomite::Bases
