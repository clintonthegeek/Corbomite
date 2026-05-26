// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression: a BasesView must build its model regardless of whether services
// (Vault/cache/FileManager) are injected before or after the .base content is
// loaded. In the live app `onLoadFile`->`setViewData` fires *before*
// `propagateServicesToView`->`setServices`, so a view that only builds its
// model from setViewData renders permanently empty. See debugging session
// 2026-05-26.
#include <QTest>
#include <QTreeView>
#include <QAbstractItemModel>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "corbomite/bases/BasesView.h"
#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/PropertyId.h"
#include "corbomite/bases/Values.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;
using namespace Corbomite::Bases;

namespace {
// A minimal .base with a 3-column ordered view (no filter, so all rows pass).
const char *kBase =
    "views:\n"
    "  - type: table\n"
    "    name: All\n"
    "    order:\n"
    "      - note.title\n"
    "      - note.year\n"
    "      - file.name\n";

// Build a tiny on-disk vault with two markdown notes.
void seedVault(const QString &root) {
    auto write = [](const QString &path, const QByteArray &body) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(body);
    };
    write(root + QStringLiteral("/A.md"), "---\ntitle: Alpha\nyear: 2001\n---\n# Alpha\n");
    write(root + QStringLiteral("/B.md"), "---\ntitle: Beta\nyear: 1999\n---\n# Beta\n");
    write(root + QStringLiteral("/v.base"), kBase);
}

int columnCount(BasesView &bv) {
    auto *tree = bv.findChild<QTreeView *>();
    if (!tree || !tree->model()) return -1;     // -1 == no model at all
    return tree->model()->columnCount(QModelIndex());
}
int rootRowCount(BasesView &bv) {
    auto *tree = bv.findChild<QTreeView *>();
    if (!tree || !tree->model()) return -1;
    return tree->model()->rowCount(QModelIndex());
}
}

class TestBasesViewWiring : public QObject
{
    Q_OBJECT
private:
    QTemporaryDir m_dir;

private Q_SLOTS:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        seedVault(m_dir.path());
    }

    // The live-app ordering: content loaded, THEN services injected.
    void dataFirstThenServicesBuildsModel() {
        FileSystemAdapter adapter;
        Vault vault(&adapter);
        vault.load(m_dir.path());
        QCOMPARE(vault.getMarkdownFiles().size(), 2);

        BasesView bv(nullptr);
        bv.setViewData(QString::fromUtf8(kBase), true);   // content first
        bv.setServices(&vault, nullptr, nullptr);          // services after

        QCOMPARE(columnCount(bv), 3);     // note.title, note.year, file.name
        QCOMPARE(rootRowCount(bv), 2);    // two notes, ungrouped -> flat
    }

    // The other ordering must keep working too.
    void servicesFirstThenDataBuildsModel() {
        FileSystemAdapter adapter;
        Vault vault(&adapter);
        vault.load(m_dir.path());

        BasesView bv(nullptr);
        bv.setServices(&vault, nullptr, nullptr);          // services first
        bv.setViewData(QString::fromUtf8(kBase), true);

        QCOMPARE(columnCount(bv), 3);
        QCOMPARE(rootRowCount(bv), 2);
    }

    // The REAL host path: the leaf loads a NoteDocument via FileView::loadFile,
    // which calls onLoadFile. BasesView must take the .base body from the
    // document (TextFileView's adapter loader is inert) and parse it. Before
    // the fix, onLoadFile never called setViewData, so the model never built.
    void loadFileFromDocumentBuildsModel() {
        FileSystemAdapter adapter;
        Vault vault(&adapter);
        vault.load(m_dir.path());
        Corbomite::NoteDocument *doc = vault.openDocument(QStringLiteral("v.base"));
        QVERIFY(doc);
        QVERIFY(!doc->markdown().isEmpty());   // document carries the .base YAML

        BasesView bv(nullptr);
        bv.setServices(&vault, nullptr, nullptr);
        bv.loadFile(doc);                       // -> onLoadFile -> setViewData

        QCOMPARE(columnCount(bv), 3);
        QCOMPARE(rootRowCount(bv), 2);
    }

    // Regression: BasesEntry::frontmatter() returned a reference into the
    // std::optional<CachedMetadata> temporary from getFileCache(), so reading
    // any note.* value over a cache with real frontmatter dereferenced freed
    // memory (segfault in ObjectValue::fromFrontMatter during filter eval).
    void noteValueOverPopulatedCacheDoesNotDangle() {
        FileSystemAdapter adapter;
        Vault vault(&adapter);
        vault.load(m_dir.path());

        Corbomite::LinkResolver resolver;
        resolver.setVaultPaths({QStringLiteral("A.md"), QStringLiteral("B.md")});
        Corbomite::MetadataCache cache(resolver);
        QSignalSpy spy(&cache, &Corbomite::MetadataCache::cacheChanged);
        cache.onFileChanged(QStringLiteral("A.md"),
            QByteArray("---\ntitle: Alpha\nyear: 2001\n---\n# Alpha\n"), 1);
        if (spy.isEmpty()) QVERIFY(spy.wait(2000));   // parse may drain via worker
        // Confirm the cache actually holds the frontmatter we'll read.
        const auto fc = cache.getFileCache(QStringLiteral("A.md"));
        QVERIFY(fc && fc->frontmatter && !fc->frontmatter->isEmpty());

        Corbomite::TFile *tf = vault.getFileByPath(QStringLiteral("A.md"));
        QVERIFY(tf);

        BasesQuery q;
        BasesEntry entry(&vault, &cache, tf, tf, q);
        const ValuePtr title =
            entry.getValue(PropertyId{PropertyKind::Note, QStringLiteral("title")});
        QVERIFY(title);
        QCOMPARE(title->toString(), QStringLiteral("Alpha"));
    }
};

QTEST_MAIN(TestBasesViewWiring)
#include "tst_bases_view_wiring.moc"
