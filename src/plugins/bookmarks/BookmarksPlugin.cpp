// SPDX-License-Identifier: GPL-3.0-or-later
#include "BookmarksPlugin.h"

#include "BookmarkItem.h"
#include "BookmarkModal.h"
#include "BookmarksStore.h"
#include "BookmarksView.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/proxies/CommandRegistrar.h"
#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <QDateTime>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QTreeView>

namespace Corbomite::Bookmarks {

BookmarksPlugin::BookmarksPlugin(QObject *parent, const QVariantList &)
    : Corbomite::Plugin(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, &BookmarksPlugin::doSave);
}

BookmarksPlugin::~BookmarksPlugin() = default;

void BookmarksPlugin::onLoad(Corbomite::PluginContext *ctx)
{
    if (!ctx) return;

    m_store = new BookmarksStore(this);
    // Initial read via VaultProxy (gated by vault.read permission).
    if (auto *vp = ctx->vault()) {
        const QJsonObject obj = vp->readConfigJson(QStringLiteral("bookmarks.json")).toObject();
        m_store->loadFromJson(obj);
    }
    connect(m_store, &BookmarksStore::changed, this, &BookmarksPlugin::scheduleSave);

    registerCommands(ctx);
}

void BookmarksPlugin::onUnload()
{
    if (m_saveTimer.isActive()) {
        m_saveTimer.stop();
        doSave();
    }
}

void BookmarksPlugin::scheduleSave() { m_saveTimer.start(); }

void BookmarksPlugin::doSave()
{
    if (!m_store || !context()) return;
    auto *vp = context()->vault();
    if (!vp) return;
    vp->writeConfigJson(QStringLiteral("bookmarks.json"), m_store->toJson());
}

QObject *BookmarksPlugin::createView(Corbomite::MainWindow *mainWindow)
{
    auto *ctx = context();
    if (!ctx || !m_store) return nullptr;
    auto *view = new BookmarksView(m_store, ctx->workspace(),
                                   reinterpret_cast<QWidget *>(mainWindow));
    connect(view, &BookmarksView::requestNewBookmark, this, [this, view] {
        BookmarkItem inferred;
        inferred.type  = QStringLiteral("file");
        inferred.ctime = QDateTime::currentMSecsSinceEpoch();
        // Populate path from active file when WorkspaceController exposes it.
        if (auto *ws = context() ? context()->workspace() : nullptr) {
            const QString active = ws->activeFilePath();
            if (!active.isEmpty()) inferred.path = active;
        }
        BookmarkModal::runFor(std::move(inferred), m_store, view);
    });
    return view;
}

QJsonObject BookmarksPlugin::saveSessionState(QObject *view) const
{
    auto *bv = qobject_cast<BookmarksView *>(view);
    if (!bv) return {};
    QJsonObject obj;
    // Record expanded group indices as "/" -joined row paths ("0", "1/0", ...).
    QStringList expanded;
    auto *tree = bv->treeView();
    if (!tree || !tree->model()) return obj;
    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &parent) {
        const int rows = tree->model()->rowCount(parent);
        for (int r = 0; r < rows; ++r) {
            const QModelIndex idx = tree->model()->index(r, 0, parent);
            if (tree->isExpanded(idx)) {
                QStringList path;
                QModelIndex cur = idx;
                while (cur.isValid()) {
                    path.prepend(QString::number(cur.row()));
                    cur = cur.parent();
                }
                expanded.append(path.join(QLatin1Char('/')));
            }
            walk(idx);
        }
    };
    walk({});
    obj.insert(QStringLiteral("expanded"), QJsonArray::fromStringList(expanded));
    return obj;
}

void BookmarksPlugin::loadSessionState(QObject *view, const QJsonObject &state)
{
    auto *bv = qobject_cast<BookmarksView *>(view);
    if (!bv) return;
    auto *tree = bv->treeView();
    if (!tree || !tree->model()) return;
    const QJsonArray arr = state.value(QStringLiteral("expanded")).toArray();
    for (const auto &v : arr) {
        const QStringList parts = v.toString().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QModelIndex cur;
        for (const QString &p : parts) {
            bool ok = false;
            const int r = p.toInt(&ok);
            if (!ok) break;
            cur = tree->model()->index(r, 0, cur);
            if (!cur.isValid()) break;
        }
        if (cur.isValid()) tree->expand(cur);
    }
}

void BookmarksPlugin::registerCommands(Corbomite::PluginContext *ctx)
{
    auto *commands = ctx->commands();
    if (!commands) return;
    auto *workspace = ctx->workspace();

    auto makeCmd = [](const QString &id, const QString &name, const QString &icon,
                      Corbomite::Command::SimpleCallback cb) -> Corbomite::Command {
        Corbomite::Command c;
        c.id       = id;
        c.name     = name;
        c.icon     = icon;
        c.callback = std::move(cb);
        return c;
    };

    // `open` — reveals the panel via the slug dispatcher in MainWindow.
    // Uses normal addCommand so the id becomes `corbomite-bookmarks:open`.
    {
        Corbomite::Command c = makeCmd(
            QStringLiteral("open"),
            i18n("Open bookmarks"),
            QStringLiteral("bookmark-new"),
            [workspace] {
                if (workspace) workspace->revealDockView(QStringLiteral("bookmarks"));
            });
        commands->addCommand(c);
    }

    // Obsidian-id commands — addCommandRaw preserves the `bookmarks:*` prefix
    // for .obsidian/hotkeys.json round-trip (addendum §4). The raw variant
    // does NOT mutate cmd.id with the pluginId prefix.

    // bookmarks:bookmark-current-file — fully wired against WorkspaceController.
    {
        Corbomite::Command c = makeCmd(
            QStringLiteral("bookmarks:bookmark-current-file"),
            i18n("Bookmark current file"),
            QStringLiteral("bookmark-new"),
            [this, workspace] {
                if (!m_store || !workspace) return;
                const QString path = workspace->activeFilePath();
                if (path.isEmpty()) return;
                bookmarkFile(m_store, path);
            });
        commands->addCommandRaw(c);
    }

    // The remaining 5 commands depend on WorkspaceController accessors that
    // do not yet exist on the plugin proxy surface (openTabPaths,
    // activeHeading, activeBlockId, activeSearchQuery, activeGraphOptions).
    // Register each with a checkCallback returning false so the palette
    // greys them out and hotkey dispatch is a no-op, while keeping the
    // Obsidian-compatible ids reserved for hotkeys.json round-trip.
    // Tracked as a Cluster S follow-up in backlog.md.
    auto stubRaw = [commands, &makeCmd](const QString &id, const QString &label,
                                         const QString &icon) {
        Corbomite::Command c = makeCmd(id, label, icon, {});
        c.checkCallback = [](bool) { return false; };
        commands->addCommandRaw(c);
    };

    stubRaw(QStringLiteral("bookmarks:bookmark-all-tabs"),
            i18n("Bookmark all open tabs"),
            QStringLiteral("bookmark-new"));
    stubRaw(QStringLiteral("bookmarks:bookmark-current-heading"),
            i18n("Bookmark current heading"),
            QStringLiteral("bookmark-new"));
    stubRaw(QStringLiteral("bookmarks:bookmark-current-block"),
            i18n("Bookmark current block"),
            QStringLiteral("bookmark-new"));
    stubRaw(QStringLiteral("bookmarks:bookmark-current-search"),
            i18n("Bookmark current search"),
            QStringLiteral("bookmark-new"));
    stubRaw(QStringLiteral("bookmarks:bookmark-current-graph"),
            i18n("Bookmark current graph view"),
            QStringLiteral("bookmark-new"));
}

// Store-mutation helpers are defined in BookmarksCommands.cpp so that
// tests can link them without pulling in KPluginFactory / BookmarksView
// / BookmarkModal.

} // namespace Corbomite::Bookmarks

K_PLUGIN_FACTORY_WITH_JSON(BookmarksPluginFactory, "metadata.json",
    registerPlugin<Corbomite::Bookmarks::BookmarksPlugin>();)

#include "BookmarksPlugin.moc"
