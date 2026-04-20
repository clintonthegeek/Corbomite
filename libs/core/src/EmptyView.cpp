// libs/core/src/EmptyView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/EmptyView.h"

#include <KLocalizedString>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Corbomite {

EmptyView::EmptyView(WorkspaceLeaf *leaf, ActionHandler handler, QWidget *parent)
    : View(leaf, parent)
    , m_handler(std::move(handler))
{
    auto *outer = new QVBoxLayout(containerWidget());
    outer->addStretch();

    auto *title = new QLabel(i18n("No file is open"), containerWidget());
    title->setAlignment(Qt::AlignCenter);
    QFont tf = title->font();
    tf.setPointSize(tf.pointSize() + 2);
    title->setFont(tf);
    outer->addWidget(title);

    auto addAction = [&](const QString &iconName, const QString &label,
                         const QString &actionId) {
        auto *btn = new QPushButton(QIcon::fromTheme(iconName), label,
                                    containerWidget());
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, actionId] {
            if (m_handler) m_handler(actionId);
        });
        auto *row = new QHBoxLayout;
        row->addStretch();
        row->addWidget(btn);
        row->addStretch();
        outer->addLayout(row);
    };

    addAction(QStringLiteral("document-new"), i18n("Create new file"),
              QStringLiteral("new-file"));
    addAction(QStringLiteral("quickopen"), i18n("Go to file"),
              QStringLiteral("go-to-file"));
    addAction(QStringLiteral("window-close"), i18n("Close"),
              QStringLiteral("close"));

    outer->addStretch();
}

QString EmptyView::getViewType() const { return QStringLiteral("empty"); }

QString EmptyView::getDisplayText() const { return i18n("New tab"); }

QString EmptyView::getIcon() const { return QStringLiteral("document-new"); }

} // namespace Corbomite
