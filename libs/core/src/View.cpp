// libs/core/src/View.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/View.h"
#include "corbomite/core/Component.h"
#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceParent.h"
#include "corbomite/core/WorkspaceTabs.h"

#include <KLocalizedString>
#include <QAction>
#include <QMenu>
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

void View::onTabMenu(QMenu *menu)
{
    // Default: the canonical Close / Close Others / Close All to the
    // Right / Close All menu (audit: views.md §1 tab context). Subclasses
    // can override to customise or suppress. The leaf must live under a
    // WorkspaceTabs for the close-siblings semantics to apply; otherwise
    // no items are added.
    if (!menu || !m_leaf) return;
    auto *tabs = qobject_cast<WorkspaceTabs *>(m_leaf->parentItem());
    if (!tabs) return;
    const int myIdx = tabs->indexOf(m_leaf);
    const int count = tabs->children().size();
    if (myIdx < 0) return;

    auto *aClose = menu->addAction(i18n("Close"));
    QObject::connect(aClose, &QAction::triggered, tabs,
                     [tabs, myIdx] { tabs->requestCloseTab(myIdx); });

    if (count > 1) {
        auto *aOthers = menu->addAction(i18n("Close Others"));
        QObject::connect(aOthers, &QAction::triggered, tabs,
                         [tabs, myIdx] { tabs->requestCloseOthers(myIdx); });

        if (myIdx < count - 1) {
            auto *aRight = menu->addAction(i18n("Close All to the Right"));
            QObject::connect(aRight, &QAction::triggered, tabs,
                             [tabs, myIdx] { tabs->requestCloseToRight(myIdx); });
        }

        auto *aAll = menu->addAction(i18n("Close All"));
        QObject::connect(aAll, &QAction::triggered, tabs,
                         [tabs] { tabs->requestCloseAll(); });
    }
}
void View::onResize() {}

void View::zoomIn() {}
void View::zoomOut() {}
void View::zoomReset() {}

QWidget *View::containerWidget() const { return m_containerWidget; }
WorkspaceLeaf *View::leaf() const { return m_leaf; }

void View::onOpen() {}
void View::onClose() {}

} // namespace Corbomite
