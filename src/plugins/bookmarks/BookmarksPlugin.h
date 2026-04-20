// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/vault/Plugin.h"

#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

namespace Corbomite::Bookmarks {

class BookmarksStore;

class BookmarksPlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    BookmarksPlugin(QObject *parent, const QVariantList &args);
    ~BookmarksPlugin() override;

    QObject *createView(Corbomite::MainWindow *mainWindow) override;

    QJsonObject saveSessionState(QObject *view) const override;
    void loadSessionState(QObject *view, const QJsonObject &state) override;

    /// Host entry point for the Cluster R "Bookmark…" hamburger menu slot.
    /// Composes a file-type BookmarkItem for `relativePath` and opens the
    /// BookmarkModal parented at `parent`. Silent no-op if the plugin has
    /// no live store (e.g. called before onLoad).
    Q_INVOKABLE void openBookmarkModalForFile(const QString &relativePath,
                                              QWidget *parent = nullptr);

    // Store-mutation helpers shared by command callbacks and tests. These are
    // the testable core: callbacks assemble workspace state into these inputs,
    // the helpers build the BookmarkItem and append. Exposed for
    // tst_bookmarks_commands to exercise without a live WorkspaceController.
    static void bookmarkFile(BookmarksStore *store, const QString &path,
                             const QStringList &groupPath = {});
    static void bookmarkAllTabs(BookmarksStore *store, const QStringList &paths,
                                const QStringList &groupPath = {});
    static void bookmarkHeading(BookmarksStore *store, const QString &path,
                                const QString &heading,
                                const QStringList &groupPath = {});
    static void bookmarkBlock(BookmarksStore *store, const QString &path,
                              const QString &blockId,
                              const QStringList &groupPath = {});
    static void bookmarkSearch(BookmarksStore *store, const QString &query,
                               const QStringList &groupPath = {});
    static void bookmarkGraph(BookmarksStore *store, const QJsonObject &options,
                              const QStringList &groupPath = {});

protected:
    void onLoad(Corbomite::PluginContext *ctx) override;
    void onUnload() override;

private:
    void registerCommands(Corbomite::PluginContext *ctx);
    void scheduleSave();
    void doSave();

    QPointer<BookmarksStore> m_store;
    QTimer                   m_saveTimer;
};

} // namespace Corbomite::Bookmarks
