// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "BasesViewConfig.h"
#include "PropertyId.h"
#include <QHeaderView>
#include <QVector>
#include <functional>

namespace Corbomite::Bases {

/// Header that paints multi-key sort indicators. The owner supplies the
/// current sort keys + a column->PropertyId map via setProviders.
class BasesHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit BasesHeaderView(QWidget *parent = nullptr);
    using SortProvider = std::function<QVector<SortKey>()>;
    using PropertyForColumn = std::function<PropertyId(int)>;
    void setProviders(SortProvider sort, PropertyForColumn prop);
protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
private:
    SortProvider m_sort;
    PropertyForColumn m_prop;
};

}  // namespace Corbomite::Bases
