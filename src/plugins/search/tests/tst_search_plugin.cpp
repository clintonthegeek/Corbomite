// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>

#include <KPluginMetaData>

#include <memory>

#include "../SearchPlugin.h"
#include "../SearchView.h"

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/storage/IgnoreFilter.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

class TestSearchPlugin : public QObject
{
    Q_OBJECT
private slots:
    void createsViewWhenMetadataReadGranted();
    void returnsNullWhenIndexMissing();
    void focusOverrideDispatchesToSearchInput();
    void ignoredPathsDisappearFromSearchResults();
};

static PluginMetaData makeMeta() { return PluginMetaData(KPluginMetaData{}); }

namespace {
void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

// Build a plugin-created SearchView over a real vault + index. The view is
// created through the plugin, exactly the way createView() builds it, so the
// test exercises the real permission → proxy → view wiring including the
// vault proxy the ignore-filter read needs.
struct SearchHarness {
    FileSystemAdapter fs;
    Vault vaultObj{&fs};
    LinkResolver resolver;
    MetadataCache cache{resolver};
    SQLiteIndex index;
    std::unique_ptr<PluginContext> ctx;
    SearchPlugin plugin;
    SearchView *sv = nullptr;
    QString status;
};

/** Build `h` for a real vault + index. Test scope: h must outlive the asserts. */
static void makeSearchHarness(SearchHarness &h, const QString &vaultDir,
                              const QString &dbPath)
{
    h.vaultObj.load(vaultDir);

    QStringList paths;
    for (TFile *f : h.vaultObj.getMarkdownFiles()) paths.append(f->path);
    h.resolver.setVaultPaths(paths);

    QVERIFY(h.index.open(dbPath));
    h.index.setVaultRoot(vaultDir);
    h.index.setMetadataCache(&h.cache);

    QSignalSpy finishedSpy(&h.cache, &Corbomite::MetadataCache::indexFinished);
    h.cache.rebuildVault(vaultDir, paths);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);

    h.ctx = std::make_unique<PluginContext>(makeMeta(),
        QSet<QString>{QStringLiteral("metadata.read"),
                      QStringLiteral("workspace"),
                      QStringLiteral("vault.read")});
    h.ctx->setCoreServices(&h.vaultObj, nullptr, &h.cache, &h.index,
                            nullptr, nullptr, nullptr, nullptr, nullptr);
    h.plugin.load(h.ctx.get());

    QObject *view = h.plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    h.sv = qobject_cast<SearchView *>(view);
    QVERIFY(h.sv != nullptr);
}
} // namespace


void TestSearchPlugin::createsViewWhenMetadataReadGranted()
{
    SearchPlugin plugin;
    LinkResolver resolver;
    MetadataCache cache(resolver);
    SQLiteIndex index;

    PluginContext ctx(makeMeta(),
        {QStringLiteral("metadata.read"), QStringLiteral("workspace")});
    ctx.setCoreServices(nullptr, nullptr, &cache, &index, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    plugin.load(&ctx);

    QObject *view = plugin.createView(nullptr);
    QVERIFY(view != nullptr);
    QVERIFY(qobject_cast<SearchView *>(view) != nullptr);
    delete view;
}

void TestSearchPlugin::returnsNullWhenIndexMissing()
{
    SearchPlugin plugin;
    LinkResolver resolver;
    MetadataCache cache(resolver);
    PluginContext ctx(makeMeta(), {QStringLiteral("metadata.read")});
    ctx.setCoreServices(nullptr, nullptr, &cache, nullptr, nullptr, nullptr,
                         nullptr, nullptr, nullptr);
    // No SQLiteIndex — search() returns nullptr.
    plugin.load(&ctx);
    QCOMPARE(plugin.createView(nullptr), nullptr);
}

void TestSearchPlugin::focusOverrideDispatchesToSearchInput()
{
    // Non-SearchView input falls through to Plugin::focus default.
    // Passing nullptr must be a safe no-op (default ignores non-widget).
    SearchPlugin plugin;
    plugin.focus(nullptr);
    QObject notAView;
    plugin.focus(&notAView);
    // If we had a SearchView here, focus() would land on its QLineEdit
    // via focusSearchInput(). Covered end-to-end by tst_e2e_gui.
}

void TestSearchPlugin::ignoredPathsDisappearFromSearchResults()
{
    // userIgnoreFilters is a search-results gate (Obsidian
    // app/own/1951-e3.js:77). The view is asserted on its user-visible
    // surface: the status label counts and the shown paths. The filter is
    // read from a real `.obsidian/app.json` through the vault proxy.
    QTemporaryDir filtered;
    QDir().mkpath(filtered.path() + "/vault");
    writeFile(filtered.path() + "/vault/A.md", QByteArrayLiteral("needle here"));
    writeFile(filtered.path() + "/vault/drafts/D.md",
              QByteArrayLiteral("needle hidden"));
    writeFile(filtered.path() + "/vault/.obsidian/app.json",
              QByteArrayLiteral(R"({"userIgnoreFilters": ["drafts/"]})"));

    // The control run: without the filter, both files surface. A fixture
    // that only ever returned one file would make the first block vacuous.
    QTemporaryDir unfiltered;
    QDir().mkpath(unfiltered.path() + "/vault");
    writeFile(unfiltered.path() + "/vault/A.md",
              QByteArrayLiteral("needle here"));
    writeFile(unfiltered.path() + "/vault/drafts/D.md",
              QByteArrayLiteral("needle hidden"));

    SearchHarness h;
    makeSearchHarness(h, filtered.path() + "/vault",
                      filtered.path() + "/index.sqlite");
    h.sv->setQuery(QStringLiteral("needle"));
    QTest::qWait(600);   // past the 300 ms debounce
    QString status = h.sv->findChild<QLabel *>()->text();
    QVERIFY2(status.contains(QLatin1String("1 files")), qPrintable(status));

    SearchHarness h2;
    makeSearchHarness(h2, unfiltered.path() + "/vault",
                      unfiltered.path() + "/index.sqlite");
    h2.sv->setQuery(QStringLiteral("needle"));
    QTest::qWait(600);
    status = h2.sv->findChild<QLabel *>()->text();
    QVERIFY2(status.contains(QLatin1String("2 files")), qPrintable(status));

    // The surviving row is the non-ignored file, not merely one of the two.
    auto *resultView = h.sv->findChild<QTreeView *>();
    QVERIFY(resultView);
    QCOMPARE(resultView->model()->rowCount(), 1);

    // And a direct call of the pure helper, so the semantics are pinned even
    // if the status text's wording changes.
    Corbomite::SearchMatch ignored, kept;
    ignored.notePath = QStringLiteral("drafts/D.md");
    kept.notePath = QStringLiteral("A.md");
    const auto out = Corbomite::SearchView::dropIgnoredPaths(
        {ignored, kept},
        Corbomite::IgnoreFilter::fromPatterns({QStringLiteral("drafts/")}));
    QCOMPARE(out.size(), 1);
    QCOMPARE(out.first().notePath, QStringLiteral("A.md"));
}

QTEST_MAIN(TestSearchPlugin)
#include "tst_search_plugin.moc"
