// libs/core/src/ItemView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/ItemView.h"
#include "corbomite/core/MenuEventEmitter.h"
#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QIcon>
#include <QCursor>
#include <KLocalizedString>

namespace Corbomite {

ItemView::ItemView(WorkspaceLeaf *leaf, QWidget *parent)
    : View(leaf, parent)
{
    buildHeader();
}

void ItemView::buildHeader()
{
    auto *outerLayout = qobject_cast<QVBoxLayout *>(layout());
    if (!outerLayout) return;

    outerLayout->removeWidget(containerWidget());

    m_headerWidget = new QWidget(this);
    m_headerWidget->setObjectName(QStringLiteral("view-header"));
    auto *headerLayout = new QHBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(4);

    m_backButton = new QToolButton(m_headerWidget);
    m_backButton->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    m_backButton->setToolTip(i18n("Navigate Back"));
    m_backButton->setAutoRaise(true);
    m_backButton->setEnabled(false);

    m_forwardButton = new QToolButton(m_headerWidget);
    m_forwardButton->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    m_forwardButton->setToolTip(i18n("Navigate Forward"));
    m_forwardButton->setAutoRaise(true);
    m_forwardButton->setEnabled(false);

    m_iconLabel = new QLabel(m_headerWidget);
    m_titleLabel = new QLabel(m_headerWidget);
    m_titleLabel->setObjectName(QStringLiteral("view-header-title"));

    m_actionsLayout = new QHBoxLayout;
    m_actionsLayout->setSpacing(2);

    auto *moreBtn = new QToolButton(m_headerWidget);
    moreBtn->setIcon(QIcon::fromTheme(QStringLiteral("overflow-menu")));
    moreBtn->setToolTip(i18n("More options"));
    moreBtn->setAutoRaise(true);
    connect(moreBtn, &QToolButton::clicked, this, &ItemView::showMoreOptionsMenu);

    headerLayout->addWidget(m_backButton);
    headerLayout->addWidget(m_forwardButton);
    headerLayout->addWidget(m_iconLabel);
    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addLayout(m_actionsLayout);
    headerLayout->addWidget(moreBtn);

    m_contentWidget = new QWidget(this);

    outerLayout->addWidget(m_headerWidget);
    outerLayout->addWidget(m_contentWidget, 1);
}

QWidget *ItemView::contentWidget() const { return m_contentWidget; }
QWidget *ItemView::headerWidget() const { return m_headerWidget; }

void ItemView::addAction(const QString &icon, const QString &title,
                         std::function<void()> callback)
{
    auto *btn = new QToolButton(m_headerWidget);
    btn->setIcon(QIcon::fromTheme(icon));
    btn->setToolTip(title);
    btn->setAutoRaise(true);
    connect(btn, &QToolButton::clicked, this, [cb = std::move(callback)] { cb(); });
    m_actionsLayout->addWidget(btn);
}

void ItemView::onOpen()
{
    View::onOpen();
    m_titleLabel->setText(getDisplayText());
    m_iconLabel->setPixmap(QIcon::fromTheme(getIcon()).pixmap(16, 16));

    if (m_leaf) {
        connect(m_backButton, &QToolButton::clicked,
                m_leaf, &WorkspaceLeaf::goBack);
        connect(m_forwardButton, &QToolButton::clicked,
                m_leaf, &WorkspaceLeaf::goForward);
        // Refresh button state whenever the leaf switches to a new view
        connect(m_leaf, &WorkspaceLeaf::viewChanged,
                this, &ItemView::updateNavigationButtons);
    }

    updateNavigationButtons();
}

void ItemView::updateNavigationButtons()
{
    if (!m_leaf) {
        m_backButton->setEnabled(false);
        m_forwardButton->setEnabled(false);
        return;
    }
    m_backButton->setEnabled(m_leaf->history().canGoBack());
    m_forwardButton->setEnabled(m_leaf->history().canGoForward());
}

void ItemView::buildMoreOptionsMenu(QMenu *menu)
{
    if (!menu) return;
    Corbomite::MenuSectionHelper helper(menu);

    // 1. Primary subclass hook (View::onMoreOptionsMenu(MenuSectionHelper&))
    onMoreOptionsMenu(helper);

    // 2. Back-compat: onPaneMenu with source="more-options"
    onPaneMenu(menu, QStringLiteral("more-options"));

    // 3. Plugin hook: leaf-menu emission via MenuEventEmitter
    if (m_leaf) {
        if (auto *emitter = m_leaf->menuEventEmitter())
            emitter->emitLeafMenu(menu, m_leaf);
    }

    helper.finalize();
}

void ItemView::showMoreOptionsMenu()
{
    QMenu menu(this);
    buildMoreOptionsMenu(&menu);
    if (!menu.isEmpty())
        menu.exec(QCursor::pos());
}

} // namespace Corbomite
