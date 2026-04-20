// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarkModal.h"

#include "BookmarksStore.h"

namespace Corbomite::Bookmarks {

bool BookmarkModal::runFor(BookmarkItem inferred, BookmarksStore *store, QWidget *)
{
    // Stub: silently commit without showing UI. Replaced in Task 3.1.
    if (store) store->addBookmark(std::move(inferred), {});
    return store != nullptr;
}

} // namespace Corbomite::Bookmarks
