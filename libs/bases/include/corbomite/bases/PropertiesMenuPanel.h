// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PropertyId.h"

#include <QFrame>
#include <QStringList>
#include <QVector>
#include <functional>

class QListWidget;

namespace Corbomite::Bases {

/// Sentinel value for the summary picker: user chose "Custom…" (owner should
/// open a summary FormulaEditDialog).
inline constexpr char kCustomSummarySentinel[] = "__custom__";

/// Popup panel listing every available property with a visible-checkbox and a
/// drag handle for reorder. Mutates the QVector<PropertyId> the owner passes by
/// pointer (the active view's `order`), then invokes `onChanged` so the owner
/// can recompute + persist.
class PropertiesMenuPanel : public QFrame
{
    Q_OBJECT
public:
    explicit PropertiesMenuPanel(QWidget *parent = nullptr);

    /// (Re)build the row list. `order` is the live order vector (mutated in
    /// place); `allProps` is every property the panel may show/hide.
    void setState(QVector<PropertyId> *order, const QVector<PropertyId> &allProps,
                  std::function<QString(const PropertyId &)> displayName);
    void setOnChanged(std::function<void()> cb) { m_onChanged = std::move(cb); }

    /// Configure the summary picker: built-in + custom names to list, and the
    /// current per-property selection. Call before/with setState.
    void setSummaryState(const QStringList &availableSummaryNames,
                         std::function<QString(const PropertyId &)> currentSummary);

Q_SIGNALS:
    void addFormulaRequested();
    void editFormulaRequested(const QString &name);
    void deleteFormulaRequested(const QString &name);
    /// `summaryFnName` empty == None. The sentinel kCustomSummarySentinel means
    /// the user chose "Custom…" (owner opens a summary FormulaEditDialog).
    void summaryChanged(const Corbomite::Bases::PropertyId &prop, const QString &summaryFnName);

private:
    void rebuild();
    void onItemChanged();   // checkbox toggled
    void onRowsMoved();     // drag reorder finished

    QListWidget *m_list = nullptr;
    QVector<PropertyId> *m_order = nullptr;
    QVector<PropertyId> m_allProps;
    std::function<QString(const PropertyId &)> m_displayName;
    std::function<void()> m_onChanged;
    bool m_updating = false;
    QStringList m_summaryNames;
    std::function<QString(const PropertyId &)> m_currentSummary;
};

}  // namespace Corbomite::Bases
