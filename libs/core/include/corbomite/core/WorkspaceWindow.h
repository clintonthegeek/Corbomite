// libs/core/include/corbomite/core/WorkspaceWindow.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace Corbomite {

// Popout window stub. Post-Cluster-Y, geometry / show / close / serialize
// ride KDDockWidgets::Core::FloatingWindow directly via DockRegistry, so
// the standalone QWidget facade has been removed. This shell is now just
// an identity token: it satisfies the `Workspace::popoutLeaf` contract
// (returns non-null, joins `windows()` list) and carries an id() for the
// session serializer.
class WorkspaceWindow : public QObject
{
    Q_OBJECT
public:
    explicit WorkspaceWindow(QObject *parent = nullptr);
    ~WorkspaceWindow() override;

    QString id() const;
    void setId(const QString &id);

private:
    QString m_id;
};

} // namespace Corbomite
