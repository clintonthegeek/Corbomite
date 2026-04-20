// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BookmarkItem.h"

#include <QDialog>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QStringList>

class QComboBox;
class QLineEdit;

namespace Corbomite::Bookmarks {

class BookmarksStore;

/// Modal for creating a bookmark. Pre-fills the title from the inferred item
/// (file basename, search query, or "Graph view") and offers a group picker
/// combo that walks the store's group tree. On Accept the composed
/// BookmarkItem is committed via `store->addBookmark` into the selected
/// group (or at root).
class BookmarkModal : public QDialog
{
    Q_OBJECT
public:
    BookmarkModal(BookmarkItem inferred, BookmarksStore *store,
                  QWidget *parent = nullptr);
    ~BookmarkModal() override;

    /// Blocking helper. Returns true iff the user accepted and the item was
    /// committed. Non-modal construction is also supported via the ctor for
    /// tests.
    static bool runFor(BookmarkItem inferred, BookmarksStore *store, QWidget *parent);

    // --- Test / caller accessors ------------------------------------------
    QLineEdit *nameEdit() const { return m_nameEdit; }
    QComboBox *groupCombo() const { return m_groupCombo; }

    /// Collect (display-label, itemPath) for every group in `store`, recursing
    /// through nested groups. Exposed as a static so tests (and
    /// BookmarksView's Move-to-group menu) can exercise the same walk. The
    /// first element is always the synthetic `{"(root)", {}}` entry.
    static QList<QPair<QString, QStringList>> collectGroups(const BookmarksStore *store);

    /// Compose the final BookmarkItem from the inferred values + current
    /// UI state. Called on Accept; exposed for tests to drive directly.
    BookmarkItem composedItem() const;

    /// Accept path — commits to the store at the currently-selected group.
    /// Exposed so tests don't need to simulate QDialog::accept().
    void commit();

private:
    BookmarkItem             m_inferred;
    QPointer<BookmarksStore> m_store;

    QLineEdit *m_nameEdit   = nullptr;
    QComboBox *m_groupCombo = nullptr;

    // Parallel to m_groupCombo items — index maps 1:1 to an itemPath.
    QList<QStringList> m_groupPaths;

    static QString inferDefaultTitle(const BookmarkItem &item);
};

} // namespace Corbomite::Bookmarks
