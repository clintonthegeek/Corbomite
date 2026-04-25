// libs/core/include/corbomite/core/WorkspaceContainer.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace Corbomite {

/// Obsidian-shape base class for `WorkspaceRoot` + `WorkspaceFloating`'s
/// per-window root + `WorkspaceSidedock`. Holds structural-only properties
/// (id, direction); does not own widget hierarchy — the KDDW substrate
/// keeps that. Cluster Y Phase 7.5 introduces this so plugin code that
/// expects to walk `workspace.rootSplit() / leftSplit() / rightSplit()`
/// from the Obsidian plugin API at least compiles + returns sensible
/// shells. Real direction-tracking and child-iteration land later.
class WorkspaceContainer : public QObject
{
    Q_OBJECT
public:
    explicit WorkspaceContainer(QString id,
                                 QString direction,
                                 QObject *parent = nullptr);

    QString id() const { return m_id; }

    /// "horizontal" / "vertical" — the orientation in which children
    /// are laid out. Default depends on subclass.
    QString direction() const { return m_direction; }
    void setDirection(const QString &direction);

Q_SIGNALS:
    void directionChanged(const QString &direction);

private:
    QString m_id;
    QString m_direction;
};

} // namespace Corbomite
