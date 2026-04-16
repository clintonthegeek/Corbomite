// libs/core/include/corbomite/core/WorkspaceItem.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <optional>

namespace Corbomite {

class WorkspaceParent;

class WorkspaceItem : public QObject
{
    Q_OBJECT
public:
    explicit WorkspaceItem(QObject *parent = nullptr);
    ~WorkspaceItem() override;

    QString id() const;
    void setId(const QString &id);

    std::optional<int> dimension() const;
    void setDimension(std::optional<int> dim);

    WorkspaceParent *parentItem() const;
    void setParentItem(WorkspaceParent *parent);

    virtual QWidget *widget() = 0;
    virtual QJsonObject serialize() const = 0;

    static QString generateId();

private:
    QString m_id;
    std::optional<int> m_dimension;
    WorkspaceParent *m_parentItem = nullptr;
};

} // namespace Corbomite
