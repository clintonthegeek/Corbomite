// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarksView.h"

#include "BookmarksModel.h"
#include "BookmarksStore.h"

#include "corbomite/core/proxies/WorkspaceController.h"

#include "BookmarkItem.h"

#include <KLocalizedString>
#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
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
    const QStringList itemPath = m_model->pathOf(idx);

    QMenu menu(this);

    // Rename — inline QInputDialog seeded with the current title (falling back
    // to the display-role inference when the user hasn't overridden it).
    auto *rename = menu.addAction(
        QIcon::fromTheme(QStringLiteral("edit-rename")), i18n("Rename…"));
    connect(rename, &QAction::triggered, this, [this, itemPath, idx] {
        if (!m_store) return;
        BookmarkItem *item = m_store->find(itemPath);
        if (!item) return;
        const QString seed = item->title.isEmpty()
                                 ? idx.data(Qt::DisplayRole).toString()
                                 : item->title;
        bool ok = false;
        const QString next = QInputDialog::getText(
            this, i18n("Rename bookmark"), i18n("Title:"),
            QLineEdit::Normal, seed, &ok);
        if (ok) m_store->setTitle(itemPath, next);
    });

    // Move to group — populated recursively from the store's group items.
    auto *moveMenu = menu.addMenu(
        QIcon::fromTheme(QStringLiteral("folder-move")), i18n("Move to group"));
    QList<QPair<QString, QStringList>> groups;  // (display label, path-parts)
    std::function<void(const QList<BookmarkItem> &, const QStringList &, const QString &)>
        walk = [&](const QList<BookmarkItem> &items, const QStringList &basePath,
                   const QString &prefix) {
        for (int i = 0; i < items.size(); ++i) {
            if (items.at(i).type != QLatin1String("group")) continue;
            QStringList path = basePath;
            path.append(QString::number(i));
            const QString label = prefix.isEmpty()
                                      ? items.at(i).title
                                      : prefix + QLatin1String(" / ") + items.at(i).title;
            groups.append({label, path});
            walk(items.at(i).children, path, label);
        }
    };
    walk(m_store->rootItems(), {}, QString());

    auto *moveRoot = moveMenu->addAction(i18n("(root)"));
    connect(moveRoot, &QAction::triggered, this, [this, itemPath] {
        if (m_store) m_store->moveBookmark(itemPath, {}, m_store->rootItems().size());
    });
    if (!groups.isEmpty()) moveMenu->addSeparator();
    for (const auto &g : groups) {
        // Don't list a group as a destination if the bookmark is already that
        // group or a descendant of it (would move a node into itself).
        if (itemPath == g.second) continue;
        bool isAncestor = itemPath.size() <= g.second.size();
        for (int i = 0; isAncestor && i < itemPath.size(); ++i)
            if (itemPath.at(i) != g.second.at(i)) isAncestor = false;
        if (isAncestor && itemPath.size() < g.second.size()) continue;

        auto *act = moveMenu->addAction(g.first);
        const QStringList destParent = g.second;
        connect(act, &QAction::triggered, this, [this, itemPath, destParent] {
            if (!m_store) return;
            BookmarkItem *parent = m_store->find(destParent);
            const int insertAt = parent ? parent->children.size() : 0;
            m_store->moveBookmark(itemPath, destParent, insertAt);
        });
    }
    if (groups.isEmpty()) moveMenu->setEnabled(false);

    menu.addSeparator();
    auto *del = menu.addAction(
        QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete"));
    connect(del, &QAction::triggered, this, [this, itemPath] {
        if (m_store) m_store->removeBookmark(itemPath);
    });

    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

} // namespace Corbomite::Bookmarks
