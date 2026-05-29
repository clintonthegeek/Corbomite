// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FilterSpec.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QVBoxLayout;

namespace Corbomite::Bases {

class FormulaInput;

/// Recursive editor for ONE filter group. Leaves are FormulaInputs; nested
/// groups are child FilterBuilderWidgets. Emits changed() on any edit and
/// validityChanged() when the aggregate validity flips. spec() reconstructs a
/// FilterSpec from the live widget state on demand.
class FilterBuilderWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FilterBuilderWidget(QWidget *parent = nullptr);

    /// Rebuild the widget from `group` (must be Kind::Group). Candidates are
    /// forwarded to every leaf FormulaInput (and nested group).
    void setSpec(const FilterSpec &group, const QStringList &candidates);

    /// Reconstruct the current group spec from the widgets.
    FilterSpec spec() const;

    /// True iff every descendant leaf is parse-valid (empty leaves count as
    /// valid — they are dropped by toFilter).
    bool isValid() const;

Q_SIGNALS:
    void changed();
    void validityChanged(bool valid);

private:
    struct Row { QWidget *container = nullptr;
                 FormulaInput *leaf = nullptr;
                 FilterBuilderWidget *group = nullptr; };

    void addLeafRow(const QString &expr);
    void addGroupRow(const FilterSpec &groupSpec);
    void removeRow(QWidget *container);
    void clearRows();
    void onAnyChange();          // emits changed() + recomputes validity

    QComboBox *m_conj = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    QVector<Row> m_rows;
    QStringList m_candidates;
    bool m_lastValid = true;
};

}  // namespace Corbomite::Bases
