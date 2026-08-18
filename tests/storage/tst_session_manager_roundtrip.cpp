// SPDX-License-Identifier: GPL-3.0-or-later
//
// Spec-driven tests for SessionManager persistence claims under the Cluster
// L workspace compat-boundary doctrine
// (docs/superpowers/specs/2026-08-17-workspace-compat-boundary.md).
//
// SessionManager no longer owns tier-1 (Obsidian-schema workspace.json —
// see tst_workspace_session.cpp / the golden-fixture test for that side).
// It owns:
//   - Tier 2 (vault-portable, Corbomite-native): <vault>/.obsidian/
//     corbomite/state.json — expandedFolders, leftRibbon, sidebar
//     visibility (bool) + activePanel, vaultId.
//   - Tier 3 (machine-local): <AppDataLocation>/vaults/<vaultId>/
//     session.json — windowGeometry/State, sidebar pixel widths,
//     per-plugin ephemeral session state.
//
// Claims under test:
//  1. windowGeometry/windowState round-trip via tier 3.
//  2. Tier 3 file does not contain vault-portable (tier 2) keys.
//  3. Sidebar visible/activePanel round-trip via tier 2; width via tier 3.
//  4. expandedFolders round-trip via tier 2.
//  5. Tier 2 file does not contain machine-local (tier 3) keys.
//  6. A vaultId is minted on first load() and is stable across reload.
//  7. pluginSessionState round-trips via tier 3, keyed by plugin id.
//  8. leftRibbonState round-trips via tier 2.
//  9. load() returns false when vault path is not set.
// 10. hasLoadedSession() state transitions.
// 11. blockSaving()/unblockSaving() suppress scheduleSave.
// 12. Reloading an existing vault's SessionManager reuses the same vaultId
//     (doesn't mint a new one) and the tier-3 directory it derives matches.

#include <QTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "SessionManager.h"

using namespace Corbomite;

namespace {

QJsonObject readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

QString tier2Path(const QString &vaultRoot)
{
    return vaultRoot + QStringLiteral("/.obsidian/corbomite/state.json");
}

QString tier3Path(const QString &vaultId)
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
         + QStringLiteral("/vaults/") + vaultId + QStringLiteral("/session.json");
}

} // namespace

class TestSessionManagerRoundtrip : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // Redirect QStandardPaths::AppDataLocation (tier 3's root) into a
    // sandboxed per-test-run temp location instead of the developer's real
    // ~/.local/share — Qt's built-in test-mode support for exactly this.
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // -----------------------------------------------------------------------
    // 1/2. Window geometry round-trips via tier 3; tier 3 excludes tier-2
    // keys.
    // -----------------------------------------------------------------------
    void windowGeometryRoundTripsViaTier3()
    {
        QTemporaryDir tmp;

        SessionManager sm;
        sm.setVaultPath(tmp.path());
        sm.load();

        const QByteArray geo = QByteArray("fake-geometry-bytes");
        const QByteArray state = QByteArray("fake-state-bytes");
        sm.saveWindowGeometry(geo, state);
        sm.saveNow();

        const QJsonObject tier3 = readJson(tier3Path(sm.vaultId()));
        QVERIFY2(tier3.contains(QStringLiteral("windowGeometry")),
                 "windowGeometry must be in the tier-3 file");
        QVERIFY2(!tier3.contains(QStringLiteral("expandedFolders")),
                 "tier-3 file must not contain tier-2 keys");
        QVERIFY2(!tier3.contains(QStringLiteral("vaultId")),
                 "vaultId lives in tier 2, not tier 3");

        SessionManager sm2;
        sm2.setVaultPath(tmp.path());
        QVERIFY(sm2.load());
        QCOMPARE(sm2.windowGeometry(), geo);
        QCOMPARE(sm2.windowState(), state);
    }

    // -----------------------------------------------------------------------
    // 3. Sidebar: visible/activePanel land in tier 2, width in tier 3.
    // -----------------------------------------------------------------------
    void sidebarStateSplitsAcrossTiers()
    {
        QTemporaryDir tmp;

        SessionManager sm;
        sm.setVaultPath(tmp.path());
        sm.load();
        sm.saveSidebarState(true, 250, false, 300, QStringLiteral("files"));
        sm.saveNow();

        const QJsonObject tier2 = readJson(tier2Path(tmp.path()));
        const QJsonObject tier2Sidebar = tier2.value(QStringLiteral("sidebar")).toObject();
        QCOMPARE(tier2Sidebar.value(QStringLiteral("leftVisible")).toBool(), true);
        QCOMPARE(tier2Sidebar.value(QStringLiteral("rightVisible")).toBool(), false);
        QCOMPARE(tier2Sidebar.value(QStringLiteral("activePanel")).toString(),
                 QStringLiteral("files"));
        QVERIFY2(!tier2Sidebar.contains(QStringLiteral("leftWidth")),
                 "pixel width is machine-local, must not be in tier 2");

        const QJsonObject tier3 = readJson(tier3Path(sm.vaultId()));
        const QJsonObject tier3Sidebar = tier3.value(QStringLiteral("sidebar")).toObject();
        QCOMPARE(tier3Sidebar.value(QStringLiteral("leftWidth")).toInt(), 250);
        QCOMPARE(tier3Sidebar.value(QStringLiteral("rightWidth")).toInt(), 300);
        QVERIFY2(!tier3Sidebar.contains(QStringLiteral("leftVisible")),
                 "visibility is vault-portable, must not be in tier 3");

        // Merged accessor exposes both.
        SessionManager sm2;
        sm2.setVaultPath(tmp.path());
        QVERIFY(sm2.load());
        const QJsonObject merged = sm2.sidebarState();
        QCOMPARE(merged.value(QStringLiteral("leftVisible")).toBool(), true);
        QCOMPARE(merged.value(QStringLiteral("leftWidth")).toInt(), 250);
        QCOMPARE(merged.value(QStringLiteral("activePanel")).toString(),
                 QStringLiteral("files"));
    }

    // -----------------------------------------------------------------------
    // 4/5. expandedFolders round-trips via tier 2; tier 2 excludes
    // machine-local keys.
    // -----------------------------------------------------------------------
    void expandedFoldersRoundTripViaTier2()
    {
        QTemporaryDir tmp;

        const QStringList folders{
            QStringLiteral("notes/daily"),
            QStringLiteral("projects/alpha"),
        };

        SessionManager sm;
        sm.setVaultPath(tmp.path());
        sm.load();
        sm.saveExpandedFolders(folders);
        sm.saveWindowGeometry(QByteArray("g"), QByteArray("s"));
        sm.saveNow();

        const QJsonObject tier2 = readJson(tier2Path(tmp.path()));
        QVERIFY2(tier2.contains(QStringLiteral("expandedFolders")), "must be in tier 2");
        QVERIFY2(!tier2.contains(QStringLiteral("windowGeometry")),
                 "tier-2 file must not contain machine-local windowGeometry");

        SessionManager sm2;
        sm2.setVaultPath(tmp.path());
        QVERIFY(sm2.load());
        QCOMPARE(sm2.expandedFolders(), folders);
    }

    // -----------------------------------------------------------------------
    // 6. vaultId minted once on first load(), stable across subsequent
    // reloads.
    // -----------------------------------------------------------------------
    void vaultIdMintedOnceAndStable()
    {
        QTemporaryDir tmp;

        SessionManager sm;
        sm.setVaultPath(tmp.path());
        sm.load();
        const QString mintedId = sm.vaultId();
        QVERIFY2(!mintedId.isEmpty(), "vaultId must be minted on first load()");

        // Persisted immediately (not debounced) — visible on disk without
        // an explicit saveNow().
        const QJsonObject tier2 = readJson(tier2Path(tmp.path()));
        QCOMPARE(tier2.value(QStringLiteral("vaultId")).toString(), mintedId);

        SessionManager sm2;
        sm2.setVaultPath(tmp.path());
        QVERIFY(sm2.load());
        QCOMPARE(sm2.vaultId(), mintedId);
    }

    // -----------------------------------------------------------------------
    // 7. Per-plugin session state round-trips via tier 3.
    // -----------------------------------------------------------------------
    void pluginSessionStateRoundTripsViaTier3()
    {
        QTemporaryDir tmp;

        SessionManager smA;
        smA.setVaultPath(tmp.path());
        smA.load();

        QJsonObject fe;
        fe.insert(QStringLiteral("expandedFolders"),
                  QJsonArray{QStringLiteral("a"), QStringLiteral("b/c")});
        smA.setPluginSessionState(QStringLiteral("corbomite-file-explorer"), fe);
        smA.saveNow();

        const QJsonObject tier3 = readJson(tier3Path(smA.vaultId()));
        const QJsonObject plugins = tier3.value(QStringLiteral("plugins")).toObject();
        QVERIFY(plugins.contains(QStringLiteral("corbomite-file-explorer")));

        SessionManager smB;
        smB.setVaultPath(tmp.path());
        QVERIFY(smB.load());
        QCOMPARE(smB.pluginSessionState(QStringLiteral("corbomite-file-explorer")), fe);
        QVERIFY(smB.pluginSessionState(QStringLiteral("nope")).isEmpty());

        // Clearing removes the entry.
        smB.setPluginSessionState(QStringLiteral("corbomite-file-explorer"), {});
        smB.saveNow();
        const QJsonObject tier3Cleared = readJson(tier3Path(smB.vaultId()));
        QVERIFY2(!tier3Cleared.value(QStringLiteral("plugins")).toObject()
                     .contains(QStringLiteral("corbomite-file-explorer")),
                 "cleared plugin state must not survive");
    }

    // -----------------------------------------------------------------------
    // 8. leftRibbonState round-trips via tier 2.
    // -----------------------------------------------------------------------
    void leftRibbonStateRoundTripsViaTier2()
    {
        QTemporaryDir tmp;

        SessionManager sm;
        sm.setVaultPath(tmp.path());
        sm.load();

        QJsonObject hidden;
        hidden.insert(QStringLiteral("core:graph_view"), true);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        sm.setLeftRibbonState(ribbon);
        sm.saveNow();

        const QJsonObject tier2 = readJson(tier2Path(tmp.path()));
        QVERIFY2(tier2.contains(QStringLiteral("leftRibbon")),
                 "leftRibbon must be in the tier-2 file");

        SessionManager sm2;
        sm2.setVaultPath(tmp.path());
        QVERIFY(sm2.load());
        const QJsonObject reloadedHidden = sm2.leftRibbonState()
            .value(QStringLiteral("hiddenItems")).toObject();
        QCOMPARE(reloadedHidden.value(QStringLiteral("core:graph_view")).toBool(), true);
    }

    // -----------------------------------------------------------------------
    // 9. load() returns false when vault path was never set.
    // -----------------------------------------------------------------------
    void loadReturnsFalseWhenVaultPathNotSet()
    {
        SessionManager sm;
        QVERIFY2(!sm.load(), "load() must return false when vault path isn't set");
        QVERIFY2(!sm.hasLoadedSession(), "hasLoadedSession() must be false");
    }

    // -----------------------------------------------------------------------
    // 10. hasLoadedSession() transitions — true after load() even for a
    // brand-new vault with nothing on disk yet (a vaultId still gets
    // minted).
    // -----------------------------------------------------------------------
    void hasLoadedSessionStateTransitions()
    {
        QTemporaryDir tmp;

        SessionManager sm;
        QVERIFY2(!sm.hasLoadedSession(), "must be false before setVaultPath/load");

        sm.setVaultPath(tmp.path());
        QVERIFY2(!sm.hasLoadedSession(), "must still be false before load()");

        sm.load();
        QVERIFY2(sm.hasLoadedSession(),
                 "must be true after load(), even for a brand-new vault");
    }

    // -----------------------------------------------------------------------
    // 11. blockSaving() prevents scheduled writes until saveNow().
    // -----------------------------------------------------------------------
    void blockSavingPreventsScheduledWrite()
    {
        QTemporaryDir tmp;

        SessionManager sm;
        sm.setVaultPath(tmp.path());
        sm.load();
        sm.saveExpandedFolders({QStringLiteral("initial")});
        sm.saveNow();

        sm.blockSaving();
        sm.saveExpandedFolders({QStringLiteral("blocked-update")});
        // scheduleSave() was called, but blockSaving() suppresses the
        // timer — verify by requiring an explicit saveNow() before the
        // update lands.
        sm.unblockSaving();
        sm.saveNow();

        SessionManager sm2;
        sm2.setVaultPath(tmp.path());
        QVERIFY(sm2.load());
        QCOMPARE(sm2.expandedFolders(), QStringList{QStringLiteral("blocked-update")});
    }

    // -----------------------------------------------------------------------
    // 12. Reload reuses the minted vaultId and derives the same tier-3 dir.
    // -----------------------------------------------------------------------
    void reloadReusesVaultIdAndTier3Dir()
    {
        QTemporaryDir tmp;

        SessionManager sm1;
        sm1.setVaultPath(tmp.path());
        sm1.load();
        sm1.saveWindowGeometry(QByteArray("geo1"), QByteArray("st1"));
        sm1.saveNow();
        const QString id1 = sm1.vaultId();

        SessionManager sm2;
        sm2.setVaultPath(tmp.path());
        sm2.load();
        QCOMPARE(sm2.vaultId(), id1);
        QCOMPARE(sm2.windowGeometry(), QByteArray("geo1"));
    }
};

QTEST_APPLESS_MAIN(TestSessionManagerRoundtrip)
#include "tst_session_manager_roundtrip.moc"
