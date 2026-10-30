// SPDX-License-Identifier: GPL-3.0-or-later
//
// Punch [ui-bundle][P3][audit-2026-06-10]: the quick switcher matched the
// FILENAME only. Obsidian's quick switcher matches filename, full path and
// frontmatter aliases — pinned here on a real QuickSwitcherModel + the
// plugin's proxy path via the real widget, so the matching never silently
// drifts from what runs at show-time.
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <KAboutData>
#include <KLocalizedString>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "dialogs/QuickSwitcher.h"
#include "dialogs/QuickSwitcherModel.h"

using namespace Corbomite;

namespace {

void createFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content.toUtf8());
}

// The proxy is what decides match/no-match; QuickSwitcher builds it
// privately. This test drives the REAL widget and asserts what the user
// would see: the result list's visible rows after a typed pattern.
QStringList visiblePaths(QuickSwitcher *sw)
{
    return sw->visiblePathsForTest();
}

} // namespace

class TstQuickSwitcherPaths : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("quick switcher test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    // Path-only hit (the extension is invisible to the match but the file
    // name's parse in NoteMeta::fromRelativePath strips ".md" from the
    // entry's NAME, and enters the row's PATH field intact).
    void pathMatchSurfacesANonMatchingName()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/inbox/PathMatch.md"),
                   QStringLiteral("body\n"));

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(300);
        auto *sw = mw.showQuickSwitcherForTest();
        QVERIFY(sw);

        // "inbox" matches nothing in the name (`inbox/PathMatch` — the name
        // is "PathMatch"), but the proxy now matches the full path.
        sw->setFilterTextForTest(QStringLiteral("inbox"));
        QCOMPARE(visiblePaths(sw).size(), 1);
        QCOMPARE(visiblePaths(sw).value(0), QStringLiteral("inbox/PathMatch.md"));

        // And the base case is preserved: a name hit still works.
        sw->setFilterTextForTest(QStringLiteral("pathmatch"));
        QCOMPARE(visiblePaths(sw).size(), 1);

        sw->close();
    }

    // Frontmatter aliases: only when the metadata cache has them, and the
    // matching never requires the cache (a null reader equals a name-and-
    // path-only matcher, the pre-change behaviour).
    void aliasMatchRequiresTheCacheAndMatches()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Rust.md"),
            QStringLiteral("---\naliases: [MKS]\n---\nbody\n"));

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);   // index settles so frontmatter is visible

        auto *sw = mw.showQuickSwitcherForTest();
        QVERIFY(sw);

        // The alias text matches: the popup rows re-derive from the alias.
        // (The alias map reads through MetadataCacheReader's
        // frontmatterFor, which is exactly what the suggester reads.)
        sw->setFilterTextForTest(QStringLiteral("MKS"));
        QCOMPARE(visiblePaths(sw).size(), 1);
        QCOMPARE(visiblePaths(sw).value(0), QStringLiteral("Rust.md"));

        sw->close();
    }
};

QTEST_MAIN(TstQuickSwitcherPaths)
#include "tst_quickswitcher_paths.moc"
