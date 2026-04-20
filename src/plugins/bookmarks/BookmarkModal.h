// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BookmarkItem.h"

#include <QDialog>

namespace Corbomite::Bookmarks {

class BookmarksStore;

/// Stub modal — silently commits the inferred item to the store without
/// showing UI. Replaced with the full name + group-picker dialog in Task 3.1.
class BookmarkModal : public QDialog
{
    Q_OBJECT
public:
    /// Open the bookmark modal for `inferred`. On Accept, commits via
    /// `store->addBookmark`. Returns true if the bookmark was added.
    /// Stub implementation: always adds immediately without UI.
    static bool runFor(BookmarkItem inferred, BookmarksStore *store, QWidget *parent);
};

} // namespace Corbomite::Bookmarks
