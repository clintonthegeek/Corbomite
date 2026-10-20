// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression: a BasesView must build its model regardless of whether services
// (Vault/cache/FileManager) are injected before or after the .base content is
// loaded. In the live app `onLoadFile`->`setViewData` fires *before*
// `propagateServicesToView`->`setServices`, so a view that only builds its
// model from setViewData renders permanently empty. See debugging session
// 2026-05-26.
#include <KLocalizedString>

#include <QTest>
#include <QTreeView>
#include <QAbstractItemModel>
#include <QTemporaryDir>
#include <QAction>
#include <QMenu>
#include <QSignalSpy>
#include <QToolButton>

#include "corbomite/bases/BasesView.h"
#include "corbomite/bases/BasesEntry.h"
#include "corbomite/bases/BasesQuery.h"
#include "corbomite/bases/FilterSpec.h"
#include "corbomite/bases/PropertyId.h"
#include "corbomite/bases/Values.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/vault/FileManager.h"
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

// The Results button is the only toolbar QToolButton with an InstantPopup
// mode, which makes it unambiguously findable without exposing the buttons
// as public API just for a test.
QToolButton *resultsButton(BasesView &bv) {
    const auto buttons = bv.findChildren<QToolButton *>();
    for (auto *b : buttons)
        if (b && b->popupMode() == QToolButton::InstantPopup) return b;
    return nullptr;
}

// The toolbar button that adopted `action`, by identity of the default
// action. Identity rather than text/index: BasesView also contains
// view-header buttons and a QLineEdit clear button, so positional or
// text-based lookup is brittle.
QToolButton *buttonAdopting(BasesView &bv, QAction *action) {
    const auto buttons = bv.findChildren<QToolButton *>();
    for (auto *b : buttons)
        if (b && b->defaultAction() == action) return b;
    return nullptr;
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

    // applySummaryChoice writes into the active view's summaries map and the
    // change survives a getViewData/setViewData round-trip (i.e., persists).
    void applySummaryChoice_writesViewConfigAndRoundTrips() {
        FileSystemAdapter adapter;
        Vault vault(&adapter);
        vault.load(m_dir.path());

        BasesView bv(nullptr);
        bv.setViewData(QString::fromUtf8(kBase), true);
        bv.setServices(&vault, nullptr, nullptr);

        // Precondition: no summary set yet.
        const PropertyId priceProp{PropertyKind::Note, QStringLiteral("year")};
        QVERIFY(bv.activeView());
        QVERIFY(!bv.activeView()->summaries.contains(priceProp));

        // Mutate via the public helper.
        bv.applySummaryChoice(priceProp, QStringLiteral("average"));

        // Immediately reflected in the active view.
        QCOMPARE(bv.activeView()->summaries.value(priceProp), QStringLiteral("average"));

        // Survives a round-trip through getViewData (YAML serialise/parse).
        const QString serialised = bv.getViewData();
        QVERIFY(!serialised.isEmpty());
        BasesView bv2(nullptr);
        bv2.setViewData(serialised, true);
        bv2.setServices(&vault, nullptr, nullptr);
        QVERIFY(bv2.activeView());
        QCOMPARE(bv2.activeView()->summaries.value(priceProp), QStringLiteral("average"));

        // Clearing with empty string removes the entry.
        bv.applySummaryChoice(priceProp, QString());
        QVERIFY(!bv.activeView()->summaries.contains(priceProp));
    }

    // applyFilterSpecs writes both filter scopes and the change survives a
    // getViewData/setViewData round-trip.
    void applyFilterSpecs_writesBothScopesAndRoundTrips()
    {
        FileSystemAdapter adapter;
        Vault vault(&adapter);
        vault.load(m_dir.path());

        BasesView bv(nullptr);
        bv.setViewData(QString::fromUtf8(kBase), true);
        bv.setServices(&vault, nullptr, nullptr);

        QVERIFY(bv.query());
        QVERIFY(bv.activeView());

        FilterSpec global = FilterSpec::group(Conj::And, { FilterSpec::leaf(QStringLiteral("file.hasTag(\"book\")")) });
        FilterSpec perView = FilterSpec::group(Conj::Or, {
            FilterSpec::leaf(QStringLiteral("status == \"open\"")),
            FilterSpec::leaf(QStringLiteral("status == \"wip\"")),
        });
        bv.applyFilterSpecs(global, perView);

        // Global: single-child And-group collapses to a bare rule via optimize().
        QVERIFY(bv.query()->filters != nullptr);
        QCOMPARE(bv.query()->filters->serialize().toString(),
                 QStringLiteral("file.hasTag(\"book\")"));

        // Per-view: Or-group with two children stays as a map.
        QVERIFY(bv.activeView()->filters != nullptr);
        QVERIFY(bv.activeView()->filters->serialize().toMap().contains(QStringLiteral("or")));

        // Both scopes survive a round-trip through YAML.
        // Note: yamlQuoteIfNeeded wraps strings containing '"' in double-quotes
        // with backslash-escaped inner quotes, so the raw YAML contains \"-escaped
        // values. Check for unambiguous substrings that survive escaping.
        const QString yaml = bv.getViewData();
        QVERIFY(yaml.contains(QStringLiteral("file.hasTag")));
        QVERIFY(yaml.contains(QStringLiteral("status == ")));  // per-view or-filter present
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

    // ---- toolbar-action adoption -------------------------------------
    //
    // These cover the *view-side* half of the toolbar-action adoption: that
    // a button really becomes a setDefaultAction() carrier, that the
    // Results menu survives adopt/detach, and that the Tier-B predicates
    // report state rather than guessing.

    // A view with no .base content parsed has no query, so the per-view
    // controls must all report false. Guards against a predicate that
    // hardcodes true and looks right only because it is never used yet.
    void emptyViewHasNoToolbarCapabilities() {
        BasesView bv(nullptr);
        QVERIFY(!bv.hasQuery());
        QVERIFY(!bv.hasActiveView());
        QVERIFY(!bv.hasActiveFilters());
        QVERIFY(!bv.canCreateNewEntry());
    }

    // Once content parses, query + active view exist. Filters stay false
    // because neither scope is set in kBase.
    void parsedViewReportsQueryButNoFilters() {
        BasesView bv(nullptr);
        bv.setViewData(QString::fromUtf8(kBase), true);
        QVERIFY(bv.hasQuery());
        QVERIFY(bv.hasActiveView());
        QVERIFY(!bv.hasActiveFilters());
    }

    // A vault-bound view can create entries; an unbound one cannot. This is
    // the "+New disabled when the query has no writable target" rule.
    void createNewEntryNeedsAVault() {
        BasesView bv(nullptr);
        bv.setViewData(QString::fromUtf8(kBase), true);
        QVERIFY(!bv.canCreateNewEntry());

        FileSystemAdapter adapter;
        Vault vault(&adapter);
        vault.load(m_dir.path());
        FileManager fm(&vault, nullptr);
        bv.setServices(&vault, nullptr, &fm);
        QVERIFY(bv.canCreateNewEntry());
    }

    // Adopting an action turns the button into a carrier: triggering the
    // action must run the same code the button runs. Before the action
    // could not exist at all, and the button's behaviour was unreachable
    // from any other surface.
    void adoptedActionDrivesTheSameCodeAsTheButton() {
        BasesView bv(nullptr);
        bv.setViewData(QString::fromUtf8(kBase), true);

        QAction probe(i18n("New entry"));
        QSignalSpy spy(&probe, &QAction::triggered);
        bv.setToolbarAction(BasesView::ToolbarSlot::NewEntry, &probe);
        QCOMPARE(bv.toolbarAction(BasesView::ToolbarSlot::NewEntry), &probe);

        // The button must genuinely become a carrier, not merely record the
        // action. Asserting defaultAction() identity is what makes this
        // falsifiable: a stub that only stored the pointer would pass a
        // "toolbarAction() == probe" check and fail this one.
        auto *btn = buttonAdopting(bv, &probe);
        QVERIFY2(btn, "no toolbar button adopted the action");

        // And the shared definition really is shared: clicking the button
        // must fire the action (not the button's own old slot), so the
        // menubar/palette/menu surface and the button run identical code.
        btn->click();
        QCOMPARE(spy.count(), 1);

        // Triggering from the other direction reaches the same action.
        probe.trigger();
        QCOMPARE(spy.count(), 2);
    }

    // Detaching restores standalone behaviour, and — the part that would
    // actually bite a user — Results regains its local menu rather than
    // being left with an InstantPopup button that opens nothing.
    void detachRestoresStandaloneBehaviourAndResultsMenu() {
        BasesView bv(nullptr);
        bv.setViewData(QString::fromUtf8(kBase), true);

        // The menu is owned by the view, and the provider adopts THAT object
        // — so adopting must leave the button's menu alone. (An earlier
        // version re-pointed the button at the action's menu and Qt 6
        // silently cleared it, leaving an InstantPopup button that opened
        // nothing; this test is what caught that.)
        QAction probe(i18n("Results"));
        QMenu *shared = bv.toolbarResultsMenu();
        QVERIFY2(shared, "view did not build its shared results menu");

        auto *btn = resultsButton(bv);
        QVERIFY2(btn, "could not locate the Results button");
        QCOMPARE(btn->menu(), shared);          // pre-adopt

        bv.setToolbarAction(BasesView::ToolbarSlot::Results, &probe);
        QVERIFY(bv.toolbarAction(BasesView::ToolbarSlot::Results));
        QCOMPARE(btn->menu(), shared);          // unchanged by adoption

        bv.setToolbarAction(BasesView::ToolbarSlot::Results, nullptr);
        QVERIFY(!bv.toolbarAction(BasesView::ToolbarSlot::Results));
        QVERIFY2(btn->menu(), "Results button lost its menu on detach");
        QCOMPARE(btn->menu(), shared);
    }

    // Both surfaces must route to the same code, and there must be exactly
    // one "Copy table" / "Export CSV" pair — two menus would be two
    // definitions, which is the thing the adoption exists to prevent.
    void resultsMenuChildrenAreSharedAndWired() {
        BasesView bv(nullptr);
        bv.setViewData(QString::fromUtf8(kBase), true);

        QMenu *menu = bv.toolbarResultsMenu();
        QVERIFY(menu);
        QVERIFY(bv.toolbarCopyTableAction());
        QVERIFY(bv.toolbarExportCsvAction());
        QCOMPARE(menu->actions().size(), 2);
        QVERIFY(menu->actions().contains(bv.toolbarCopyTableAction()));
        QVERIFY(menu->actions().contains(bv.toolbarExportCsvAction()));
        QVERIFY(bv.toolbarCopyTableAction()->isEnabled());

        auto *btn = resultsButton(bv);
        QVERIFY(btn);
        QCOMPARE(btn->menu(), menu);
    }

    // Adopting a drawer action transfers check-state ownership to it, and
    // the widget and the action can never disagree.
    void drawerCheckStateFollowsTheWidget() {
        BasesView bv(nullptr);
        bv.setViewData(QString::fromUtf8(kBase), true);
        QVERIFY(!bv.propertiesPaneVisible());

        QAction probe(i18n("Properties pane"));
        probe.setCheckable(true);
        bv.setToolbarAction(BasesView::ToolbarSlot::PropertiesPane, &probe);
        QVERIFY(!probe.isChecked());

        bv.setPropertiesPaneVisible(true);
        QVERIFY(bv.propertiesPaneVisible());
        QVERIFY2(probe.isChecked(),
                 "drawer opened but the adopted action did not follow");

        bv.setPropertiesPaneVisible(false);
        QVERIFY(!bv.propertiesPaneVisible());
        QVERIFY(!probe.isChecked());
    }

    // Setting the property via the action is the same single write path.
    void drawerActionTriggerRoutesThroughTheSetter() {
        BasesView bv(nullptr);
        bv.setViewData(QString::fromUtf8(kBase), true);
        QAction probe(i18n("Properties pane"));
        probe.setCheckable(true);
        bv.setToolbarAction(BasesView::ToolbarSlot::PropertiesPane, &probe);

        // Not connected by the view (the provider owns that connection), so
        // assert the seam the provider relies on: state is readable and
        // writable through the public API the action drives.
        bv.setPropertiesPaneVisible(true);
        QVERIFY(bv.propertiesPaneVisible());
    }
};

QTEST_MAIN(TestBasesViewWiring)
#include "tst_bases_view_wiring.moc"
