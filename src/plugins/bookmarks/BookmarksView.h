// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include <QWidget>

class QLabel;
class QToolButton;
class QTreeView;

namespace Corbomite { class WorkspaceController; }

namespace Corbomite::Bookmarks {

class BookmarksModel;
class BookmarksStore;

class BookmarksView : public QWidget
{
    Q_OBJECT
public:
    BookmarksView(BookmarksStore *store,
                  Corbomite::WorkspaceController *workspace,
                  QWidget *parent = nullptr);
    ~BookmarksView() override;

    QTreeView *treeView() const { return m_tree; }

Q_SIGNALS:
    void requestNewBookmark();  ///< `+` header button pressed; host opens modal

private Q_SLOTS:
    void onActivated(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);

private:
    QPointer<BookmarksStore>                  m_store;
    QPointer<Corbomite::WorkspaceController>  m_workspace;
    BookmarksModel                           *m_model = nullptr;

    QLabel      *m_header  = nullptr;
    QToolButton *m_plusBtn = nullptr;
    QTreeView   *m_tree    = nullptr;
};

} // namespace Corbomite::Bookmarks
