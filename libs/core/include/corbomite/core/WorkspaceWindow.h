// libs/core/include/corbomite/core/WorkspaceWindow.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class QWidget;

namespace Corbomite {

// Popout window stub. Phase 4b drops the WorkspaceParent inheritance now
// that the substrate types are gone. Phase 5 will replace the QWidget
// container with a KDDW FloatingWindow and wire drag-drop / geometry
// persistence; until then this serves as a bookkeeping shell so the
// existing Workspace::popoutLeaf contract (returns non-null, joins
// windows() list) keeps holding.
class WorkspaceWindow : public QObject
{
    Q_OBJECT
public:
    explicit WorkspaceWindow(QObject *parent = nullptr);
    ~WorkspaceWindow() override;

    QString id() const;
    void setId(const QString &id);

    QWidget *widget();
    QJsonObject serialize() const;

    void setWindowGeometry(int x, int y, int w, int h);
    bool maximized() const;
    void setMaximized(bool max);

    void showWindow();
    void closeWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QString m_id;
    QWidget *m_widget;
    int m_x = 0, m_y = 0, m_width = 800, m_height = 600;
    bool m_maximized = false;
};

} // namespace Corbomite
