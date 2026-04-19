// libs/core/src/View.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/View.h"
#include "corbomite/core/Component.h"
#include "corbomite/core/MenuSectionHelper.h"

#include <QVBoxLayout>

namespace Corbomite {

View::View(WorkspaceLeaf *leaf, QWidget *parent)
    : QWidget(parent)
    , m_leaf(leaf)
    , m_component(std::make_unique<Component>())
    , m_containerWidget(new QWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_containerWidget);
}

View::~View()
{
    if (m_opened)
        close();
}

Component *View::component() const { return m_component.get(); }

void View::registerQObjectConnection(const QMetaObject::Connection &conn)
{
    m_component->registerQObjectConnection(conn);
}

void View::addChild(Component *child) { m_component->addChild(child); }

int View::registerInterval(int ms, std::function<void()> fn)
{
    return m_component->registerInterval(ms, std::move(fn));
}

QString View::getIcon() const { return QStringLiteral("document"); }

void View::open(QWidget *parent)
{
    if (m_opened) return;
    if (parent && parentWidget() != parent)
        setParent(parent);
    m_component->load();
    m_opened = true;
    onOpen();
}

void View::close()
{
    if (!m_opened) return;
    onClose();
    m_component->unload();
    m_opened = false;
}

QJsonObject View::getState() const { return {}; }
void View::setState(const QJsonObject &) {}
QJsonObject View::getEphemeralState() const { return {}; }
void View::setEphemeralState(const QJsonObject &) {}

void View::onPaneMenu(QMenu *) {}

void View::onMoreOptionsMenu(MenuSectionHelper & /*helper*/) {}

void View::onPaneMenu(QMenu *menu, const QString & /*source*/)
{
    onPaneMenu(menu);  // backward-compat forwarder
}

void View::onTabMenu(QMenu *) {}
void View::onResize() {}

QWidget *View::containerWidget() const { return m_containerWidget; }
WorkspaceLeaf *View::leaf() const { return m_leaf; }

void View::onOpen() {}
void View::onClose() {}

} // namespace Corbomite
