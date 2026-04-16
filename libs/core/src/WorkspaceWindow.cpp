// libs/core/src/WorkspaceWindow.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/WorkspaceWindow.h"

#include <QCloseEvent>
#include <QJsonArray>
#include <QVBoxLayout>
#include <QWidget>

namespace Corbomite {

WorkspaceWindow::WorkspaceWindow(QObject *parent)
    : WorkspaceParent(parent)
    , m_widget(new QWidget(nullptr, Qt::Window))
{
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(0, 0, 0, 0);
    m_widget->installEventFilter(this);
}

WorkspaceWindow::~WorkspaceWindow()
{
    delete m_widget;
}

QWidget *WorkspaceWindow::widget() { return m_widget; }

void WorkspaceWindow::setWindowGeometry(int x, int y, int w, int h)
{
    m_x = x;
    m_y = y;
    m_width = w;
    m_height = h;
    m_widget->setGeometry(x, y, w, h);
}

bool WorkspaceWindow::maximized() const { return m_maximized; }

void WorkspaceWindow::setMaximized(bool max)
{
    m_maximized = max;
}

void WorkspaceWindow::showWindow()
{
    m_widget->setGeometry(m_x, m_y, m_width, m_height);
    if (m_maximized)
        m_widget->showMaximized();
    else
        m_widget->show();

    for (auto *child : m_children) {
        if (auto *w = child->widget())
            m_widget->layout()->addWidget(w);
    }
}

void WorkspaceWindow::closeWindow()
{
    m_widget->hide();
}

bool WorkspaceWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_widget && event->type() == QEvent::Close) {
        auto *closeEvent = static_cast<QCloseEvent *>(event);
        closeEvent->ignore();
        closeWindow();
        return true;
    }
    return WorkspaceParent::eventFilter(obj, event);
}

QJsonObject WorkspaceWindow::serialize() const
{
    QJsonObject json;
    json[QStringLiteral("id")] = id();
    json[QStringLiteral("type")] = QStringLiteral("window");
    json[QStringLiteral("x")] = m_x;
    json[QStringLiteral("y")] = m_y;
    json[QStringLiteral("width")] = m_width;
    json[QStringLiteral("height")] = m_height;
    if (m_maximized)
        json[QStringLiteral("maximize")] = true;

    QJsonArray children;
    for (const auto *child : m_children)
        children.append(child->serialize());
    json[QStringLiteral("children")] = children;

    return json;
}

} // namespace Corbomite
