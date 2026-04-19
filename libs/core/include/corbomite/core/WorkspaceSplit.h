// libs/core/include/corbomite/core/WorkspaceSplit.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include "corbomite/core/WorkspaceParent.h"

class QSplitter;

namespace Corbomite {

class WorkspaceSplit : public WorkspaceParent
{
    Q_OBJECT
public:
    explicit WorkspaceSplit(QObject *parent = nullptr);
    ~WorkspaceSplit() override;

    Qt::Orientation direction() const;
    void setDirection(Qt::Orientation dir);

    QWidget *widget() override;
    QJsonObject serialize() const override;

    void addChild(WorkspaceItem *child, int index = -1) override;
    void removeChild(WorkspaceItem *child, bool deleteChild = false) override;

    void syncDimensionsFromSplitter();
    void syncDimensionsToSplitter();

private:
    QPointer<QSplitter> m_splitter;
    Qt::Orientation m_direction = Qt::Horizontal;
};

} // namespace Corbomite
