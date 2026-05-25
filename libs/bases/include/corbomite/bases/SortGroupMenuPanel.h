// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BasesViewConfig.h"
#include "PropertyId.h"

#include <QFrame>
#include <QVector>
#include <functional>

class QVBoxLayout;
class QComboBox;

namespace Corbomite::Bases {

/// Popup with a stack of sort-key rows (property + ASC/DESC + remove), an "Add
/// sort" button, and a group-by row (property incl. "(none)" + direction).
/// Mutates the BasesViewConfig the owner passes by pointer, then calls onChanged.
class SortGroupMenuPanel : public QFrame
{
    Q_OBJECT
public:
    explicit SortGroupMenuPanel(QWidget *parent = nullptr);

    void setState(BasesViewConfig *cfg, const QVector<PropertyId> &allProps,
                  std::function<QString(const PropertyId &)> displayName);
    void setOnChanged(std::function<void()> cb) { m_onChanged = std::move(cb); }

private:
    void rebuild();
    void changed();
    QComboBox *makePropertyCombo(const PropertyId &selected, bool withNone);

    BasesViewConfig *m_cfg = nullptr;
    QVector<PropertyId> m_allProps;
    std::function<QString(const PropertyId &)> m_displayName;
    std::function<void()> m_onChanged;

    QVBoxLayout *m_sortRows = nullptr;
    QComboBox *m_groupCombo = nullptr;
    QComboBox *m_groupDir = nullptr;
    bool m_updating = false;
};

}  // namespace Corbomite::Bases
