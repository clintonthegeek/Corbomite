// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PropertyId.h"

#include <QFrame>
#include <QVector>
#include <functional>

class QListWidget;

namespace Corbomite::Bases {

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
};

}  // namespace Corbomite::Bases
