// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarksView.h"

#include "BookmarksModel.h"
#include "BookmarksStore.h"

#include "corbomite/core/proxies/WorkspaceController.h"

#include <KLocalizedString>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace Corbomite::Bookmarks {

BookmarksView::BookmarksView(BookmarksStore *store,
                             Corbomite::WorkspaceController *workspace,
                             QWidget *parent)
    : QWidget(parent), m_store(store), m_workspace(workspace)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *headerRow = new QHBoxLayout;
    m_header = new QLabel(i18n("Bookmarks"), this);
    m_plusBtn = new QToolButton(this);
    m_plusBtn->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_plusBtn->setToolTip(i18n("New bookmark from current"));
    headerRow->addWidget(m_header);
    headerRow->addStretch();
    headerRow->addWidget(m_plusBtn);
    outer->addLayout(headerRow);

    m_tree = new QTreeView(this);
    m_tree->setHeaderHidden(true);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    outer->addWidget(m_tree);

    m_model = new BookmarksModel(m_store, this);
    m_tree->setModel(m_model);

    connect(m_tree, &QAbstractItemView::activated, this, &BookmarksView::onActivated);
    connect(m_tree, &QWidget::customContextMenuRequested, this, &BookmarksView::onContextMenu);
    connect(m_plusBtn, &QToolButton::clicked, this, &BookmarksView::requestNewBookmark);
}

BookmarksView::~BookmarksView() = default;

void BookmarksView::onActivated(const QModelIndex &index)
{
    if (!index.isValid() || !m_workspace) return;
    const QString path = index.data(BookmarksModel::BookmarksPathRole).toString();
    if (path.isEmpty()) return;
    // TODO: use WorkspaceController::openLinkText(path, subpath) once it is
    // added to the WorkspaceController proxy surface (Cluster G follow-up #3).
    // For MVP, fall back to openFile which handles vault-relative paths.
    m_workspace->openFile(path);
}

void BookmarksView::onContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_tree->indexAt(pos);
    if (!idx.isValid() || !m_store) return;
    QMenu menu(this);
    auto *del = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete"));
    connect(del, &QAction::triggered, this, [this, idx] {
        if (m_store) m_store->removeBookmark(m_model->pathOf(idx));
    });
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

} // namespace Corbomite::Bookmarks
