// libs/core/include/corbomite/core/WorkspaceWindow.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/WorkspaceParent.h"

namespace Corbomite {

// Popout window container. Currently a QWidget + Qt::Window flag.
// NOTE: promote to QMainWindow when plugin menus need per-window menu bars.
class WorkspaceWindow : public WorkspaceParent
{
    Q_OBJECT
public:
    explicit WorkspaceWindow(QObject *parent = nullptr);
    ~WorkspaceWindow() override;

    QWidget *widget() override;
    QJsonObject serialize() const override;

    void setWindowGeometry(int x, int y, int w, int h);
    bool maximized() const;
    void setMaximized(bool max);

    void showWindow();
    void closeWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QWidget *m_widget;
    int m_x = 0, m_y = 0, m_width = 800, m_height = 600;
    bool m_maximized = false;
};

} // namespace Corbomite
