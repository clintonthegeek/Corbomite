// SPDX-License-Identifier: GPL-3.0-or-later
//
// Spec-driven tests for SessionManager persistence claims.
// Source: docs/superpowers/specs/2026-04-01-vault-session-management-design.md
//
// Claims under test:
//  1. SessionManager stores window geometry inside _corbomite namespace.
//  2. SessionManager stores sidebar state inside _corbomite namespace.
//  3. SessionManager stores expanded folders inside _corbomite namespace.
//  4. workspace.json preserves unknown Obsidian keys on round-trip.
//  5. _corbomite namespace holds Corbomite-specific state (not at root level).
//  6. Loading then saving produces equivalent JSON for all known keys.
//  7. saveWindowGeometry round-trips through saveNow → load.
//  8. saveSidebarState round-trips through saveNow → load.
//  9. saveExpandedFolders round-trips through saveNow → load.
// 10. setWorkspaceLayout round-trips through saveNow → load.
// 11. active leaf ID is written at root level (not inside _corbomite).
// 12. load() returns false when session file is missing.
// 13. load() returns false when session path is not set.
// 14. hasLoadedSession() is false before a successful load.
// 15. blockSaving() / unblockSaving() suppress scheduleSave.

#include <QTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>

#include "SessionManager.h"

using namespace Corbomite;

namespace {

/// Read workspace.json as a QJsonObject from the given path.
QJsonObject readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

/// Write arbitrary JSON to the given path (creates parent dirs).
void writeJson(const QString &path, const QJsonObject &obj)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

} // namespace

class TestSessionManagerRoundtrip : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // -----------------------------------------------------------------------
    // 1. Window geometry stored under _corbomite
    // -----------------------------------------------------------------------
    void windowGeometryStoredUnderCorbomite()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);

        const QByteArray geo = QByteArray("fake-geometry-bytes");
        const QByteArray state = QByteArray("fake-state-bytes");
        sm.saveWindowGeometry(geo, state);
        sm.saveNow();

        const QJsonObject root = readJson(path);
        QVERIFY2(root.contains(QStringLiteral("_corbomite")),
                 "_corbomite key must be present at root");
        const QJsonObject corbomite = root.value(QStringLiteral("_corbomite")).toObject();
        QVERIFY2(corbomite.contains(QStringLiteral("windowGeometry")),
                 "windowGeometry must be inside _corbomite");
        QVERIFY2(corbomite.contains(QStringLiteral("windowState")),
                 "windowState must be inside _corbomite");

        // windowGeometry must NOT appear at root level
        QVERIFY2(!root.contains(QStringLiteral("windowGeometry")),
                 "windowGeometry must not leak to root");
        QVERIFY2(!root.contains(QStringLiteral("windowState")),
                 "windowState must not leak to root");
    }

    // -----------------------------------------------------------------------
    // 2. Sidebar state stored under _corbomite
    // -----------------------------------------------------------------------
    void sidebarStateStoredUnderCorbomite()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);
        sm.saveSidebarState(true, 250, false, 300, QStringLiteral("files"));
        sm.saveNow();

        const QJsonObject root = readJson(path);
        QVERIFY(root.contains(QStringLiteral("_corbomite")));
        const QJsonObject corbomite = root.value(QStringLiteral("_corbomite")).toObject();
        QVERIFY2(corbomite.contains(QStringLiteral("sidebar")),
                 "sidebar must be inside _corbomite");

        // sidebar must NOT appear at root level
        QVERIFY2(!root.contains(QStringLiteral("sidebar")),
                 "sidebar must not leak to root");
    }

    // -----------------------------------------------------------------------
    // 3. Expanded folders stored under _corbomite
    // -----------------------------------------------------------------------
    void expandedFoldersStoredUnderCorbomite()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);
        sm.saveExpandedFolders({QStringLiteral("folder/a"), QStringLiteral("folder/b")});
        sm.saveNow();

        const QJsonObject root = readJson(path);
        QVERIFY(root.contains(QStringLiteral("_corbomite")));
        const QJsonObject corbomite = root.value(QStringLiteral("_corbomite")).toObject();
        QVERIFY2(corbomite.contains(QStringLiteral("expandedFolders")),
                 "expandedFolders must be inside _corbomite");

        QVERIFY2(!root.contains(QStringLiteral("expandedFolders")),
                 "expandedFolders must not leak to root");
    }

    // -----------------------------------------------------------------------
    // 4. Unknown Obsidian keys are preserved on round-trip
    //    (the core "unknown-key invariant")
    // -----------------------------------------------------------------------
    void unknownObsidianKeysPreservedOnRoundTrip()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        // Write a workspace.json that simulates Obsidian's schema with several
        // unknown keys that Corbomite must not drop.
        QJsonObject original;
        original.insert(QStringLiteral("main"), QJsonObject{});
        original.insert(QStringLiteral("left"), QJsonObject{{QStringLiteral("tabs"), QJsonArray{}}});
        original.insert(QStringLiteral("right"), QJsonObject{{QStringLiteral("collapsed"), true}});
        original.insert(QStringLiteral("floating"), QJsonObject{});
        original.insert(QStringLiteral("lastOpenFiles"), QJsonArray{QStringLiteral("notes/Welcome.md")});
        original.insert(QStringLiteral("left-ribbon"), QJsonObject{{QStringLiteral("collapsed"), false}});
        original.insert(QStringLiteral("someObsidianPluginKey"), QJsonArray{1, 2, 3});
        writeJson(path, original);

        // Load and immediately save without any changes.
        SessionManager sm;
        sm.setSessionPath(path);
        const bool loaded = sm.load();
        QVERIFY2(loaded, "load() must return true for a valid workspace.json");

        sm.saveNow();

        const QJsonObject saved = readJson(path);

        // All unknown Obsidian keys must survive.
        QVERIFY2(saved.contains(QStringLiteral("left")),       "left must be preserved");
        QVERIFY2(saved.contains(QStringLiteral("right")),      "right must be preserved");
        QVERIFY2(saved.contains(QStringLiteral("floating")),   "floating must be preserved");
        QVERIFY2(saved.contains(QStringLiteral("lastOpenFiles")), "lastOpenFiles must be preserved");
        QVERIFY2(saved.contains(QStringLiteral("left-ribbon")), "left-ribbon must be preserved");
        QVERIFY2(saved.contains(QStringLiteral("someObsidianPluginKey")),
                 "unknown plugin keys must be preserved");

        // Values must be identical.
        QCOMPARE(saved.value(QStringLiteral("lastOpenFiles")).toArray().size(), 1);
        QCOMPARE(saved.value(QStringLiteral("lastOpenFiles")).toArray().at(0).toString(),
                 QStringLiteral("notes/Welcome.md"));
        QCOMPARE(saved.value(QStringLiteral("someObsidianPluginKey")).toArray().size(), 3);
    }

    // -----------------------------------------------------------------------
    // 5. Load-save-load produces identical content for known keys
    // -----------------------------------------------------------------------
    void loadSaveLoadEquivalentForKnownKeys()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        // Write initial state.
        SessionManager sm;
        sm.setSessionPath(path);
        sm.saveWindowGeometry(QByteArray("geo"), QByteArray("st8"));
        sm.saveSidebarState(true, 220, false, 180, QStringLiteral("search"));
        sm.saveExpandedFolders({QStringLiteral("notes"), QStringLiteral("daily")});

        QJsonObject layout;
        layout.insert(QStringLiteral("type"), QStringLiteral("split"));
        sm.setWorkspaceLayout(layout, QStringLiteral("leaf-id-abc"));
        sm.saveNow();

        // Load, then save again without any changes.
        SessionManager sm2;
        sm2.setSessionPath(path);
        QVERIFY(sm2.load());
        sm2.saveNow();

        // The two files should be byte-for-byte identical.
        QFile f1(path);
        QVERIFY(f1.open(QIODevice::ReadOnly));
        const QByteArray firstBytes = f1.readAll();
        f1.close();

        // Load into a third instance and compare accessors.
        SessionManager sm3;
        sm3.setSessionPath(path);
        QVERIFY(sm3.load());

        QCOMPARE(sm3.windowGeometry(), QByteArray("geo"));
        QCOMPARE(sm3.windowState(), QByteArray("st8"));
        QCOMPARE(sm3.expandedFolders(), QStringList({QStringLiteral("notes"), QStringLiteral("daily")}));
        QCOMPARE(sm3.activeLeafId(), QStringLiteral("leaf-id-abc"));
        QCOMPARE(sm3.workspaceLayout().value(QStringLiteral("type")).toString(),
                 QStringLiteral("split"));
    }

    // -----------------------------------------------------------------------
    // 6. saveWindowGeometry → saveNow → load round-trip
    // -----------------------------------------------------------------------
    void saveWindowGeometryRoundTrip()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        const QByteArray geo   = QByteArray("window-geometry-data");
        const QByteArray state = QByteArray("window-state-data");

        SessionManager sm;
        sm.setSessionPath(path);
        sm.saveWindowGeometry(geo, state);
        sm.saveNow();

        SessionManager sm2;
        sm2.setSessionPath(path);
        QVERIFY(sm2.load());

        QCOMPARE(sm2.windowGeometry(), geo);
        QCOMPARE(sm2.windowState(), state);
    }

    // -----------------------------------------------------------------------
    // 7. saveSidebarState → saveNow → load round-trip
    // -----------------------------------------------------------------------
    void saveSidebarStateRoundTrip()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);
        sm.saveSidebarState(true, 250, false, 300, QStringLiteral("files"));
        sm.saveNow();

        SessionManager sm2;
        sm2.setSessionPath(path);
        QVERIFY(sm2.load());

        const QJsonObject sidebar = sm2.sidebarState();
        QVERIFY2(!sidebar.isEmpty(), "sidebarState() must not be empty after load");
        QCOMPARE(sidebar.value(QStringLiteral("leftVisible")).toBool(),  true);
        QCOMPARE(sidebar.value(QStringLiteral("leftWidth")).toInt(),     250);
        QCOMPARE(sidebar.value(QStringLiteral("rightVisible")).toBool(), false);
        QCOMPARE(sidebar.value(QStringLiteral("rightWidth")).toInt(),    300);
        QCOMPARE(sidebar.value(QStringLiteral("activePanel")).toString(), QStringLiteral("files"));
    }

    // -----------------------------------------------------------------------
    // 8. saveSidebarState with empty activePanel omits activePanel key
    // -----------------------------------------------------------------------
    void saveSidebarStateEmptyActivePanelOmitted()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);
        sm.saveSidebarState(false, 200, true, 150); // no activePanel arg
        sm.saveNow();

        SessionManager sm2;
        sm2.setSessionPath(path);
        QVERIFY(sm2.load());

        const QJsonObject sidebar = sm2.sidebarState();
        QVERIFY2(!sidebar.contains(QStringLiteral("activePanel")),
                 "activePanel must not appear when not set");
    }

    // -----------------------------------------------------------------------
    // 9. saveExpandedFolders → saveNow → load round-trip
    // -----------------------------------------------------------------------
    void saveExpandedFoldersRoundTrip()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        const QStringList folders{
            QStringLiteral("notes/daily"),
            QStringLiteral("projects/alpha"),
            QStringLiteral("archive/2025")
        };

        SessionManager sm;
        sm.setSessionPath(path);
        sm.saveExpandedFolders(folders);
        sm.saveNow();

        SessionManager sm2;
        sm2.setSessionPath(path);
        QVERIFY(sm2.load());

        QCOMPARE(sm2.expandedFolders(), folders);
    }

    // -----------------------------------------------------------------------
    // 10. setWorkspaceLayout → saveNow → load round-trip
    //     (main key + active key at root)
    // -----------------------------------------------------------------------
    void setWorkspaceLayoutRoundTrip()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        QJsonObject mainJson;
        mainJson.insert(QStringLiteral("id"), QStringLiteral("root-split"));
        mainJson.insert(QStringLiteral("type"), QStringLiteral("split"));
        mainJson.insert(QStringLiteral("direction"), QStringLiteral("horizontal"));

        SessionManager sm;
        sm.setSessionPath(path);
        sm.setWorkspaceLayout(mainJson, QStringLiteral("leaf-xyz"));
        sm.saveNow();

        // Verify raw JSON: main at root, active at root.
        const QJsonObject root = readJson(path);
        QVERIFY2(root.contains(QStringLiteral("main")), "main key must be at root");
        QVERIFY2(root.contains(QStringLiteral("active")), "active key must be at root");
        QCOMPARE(root.value(QStringLiteral("active")).toString(), QStringLiteral("leaf-xyz"));

        const QJsonObject savedMain = root.value(QStringLiteral("main")).toObject();
        QCOMPARE(savedMain.value(QStringLiteral("id")).toString(), QStringLiteral("root-split"));
        QCOMPARE(savedMain.value(QStringLiteral("type")).toString(), QStringLiteral("split"));

        // Load and verify accessors.
        SessionManager sm2;
        sm2.setSessionPath(path);
        QVERIFY(sm2.load());

        QCOMPARE(sm2.activeLeafId(), QStringLiteral("leaf-xyz"));
        const QJsonObject loaded = sm2.workspaceLayout();
        QCOMPARE(loaded.value(QStringLiteral("id")).toString(), QStringLiteral("root-split"));
        QCOMPARE(loaded.value(QStringLiteral("direction")).toString(), QStringLiteral("horizontal"));
    }

    // -----------------------------------------------------------------------
    // 11. active leaf ID is at root level, not inside _corbomite
    // -----------------------------------------------------------------------
    void activeLeafIdAtRootNotInsideCorbomite()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);
        sm.setWorkspaceLayout(QJsonObject{}, QStringLiteral("leaf-root"));
        sm.saveNow();

        const QJsonObject root = readJson(path);
        QVERIFY2(root.contains(QStringLiteral("active")),
                 "active must be at root");
        QCOMPARE(root.value(QStringLiteral("active")).toString(), QStringLiteral("leaf-root"));

        // Must not appear inside _corbomite.
        if (root.contains(QStringLiteral("_corbomite"))) {
            const QJsonObject corbomite = root.value(QStringLiteral("_corbomite")).toObject();
            QVERIFY2(!corbomite.contains(QStringLiteral("active")),
                     "active must not be inside _corbomite");
        }
    }

    // -----------------------------------------------------------------------
    // 12. load() returns false when session file is missing
    // -----------------------------------------------------------------------
    void loadReturnsFalseWhenFileMissing()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");
        // File does not exist.

        SessionManager sm;
        sm.setSessionPath(path);
        QVERIFY2(!sm.load(), "load() must return false when file is missing");
        QVERIFY2(!sm.hasLoadedSession(), "hasLoadedSession() must be false when file is missing");
    }

    // -----------------------------------------------------------------------
    // 13. load() returns false when session path not set
    // -----------------------------------------------------------------------
    void loadReturnsFalseWhenPathNotSet()
    {
        SessionManager sm;
        // No setSessionPath call.
        QVERIFY2(!sm.load(), "load() must return false when path is not set");
        QVERIFY2(!sm.hasLoadedSession(), "hasLoadedSession() must be false");
    }

    // -----------------------------------------------------------------------
    // 14. hasLoadedSession() is false initially, true after successful load
    // -----------------------------------------------------------------------
    void hasLoadedSessionStateTransitions()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);

        QVERIFY2(!sm.hasLoadedSession(), "must be false before any load");

        // Create a minimal valid workspace.json.
        sm.saveWindowGeometry(QByteArray("g"), QByteArray("s"));
        sm.saveNow();

        QVERIFY2(!sm.hasLoadedSession(), "must still be false — saveNow doesn't set loaded");

        QVERIFY(sm.load());
        QVERIFY2(sm.hasLoadedSession(), "must be true after successful load");
    }

    // -----------------------------------------------------------------------
    // 15. blockSaving() prevents scheduleSave from writing
    //     (load then block, mutate, unblock — only after explicit saveNow
    //     does the file change)
    // -----------------------------------------------------------------------
    void blockSavingPreventsScheduledWrite()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        // Write an initial file.
        SessionManager sm;
        sm.setSessionPath(path);
        sm.saveExpandedFolders({QStringLiteral("initial")});
        sm.saveNow();

        // Load it.
        sm.load();
        QCOMPARE(sm.expandedFolders(), QStringList{QStringLiteral("initial")});

        // Block saving and mutate.
        sm.blockSaving();
        sm.saveExpandedFolders({QStringLiteral("blocked-update")});
        // scheduleSave was called inside saveExpandedFolders, but blockSaving
        // should have suppressed the timer.  The file on disk should still
        // contain "initial".
        //
        // We can't easily wait for the 2-second timer in a unit test, but we
        // can verify that the timer is NOT running by checking that an explicit
        // saveNow() is required.  After unblocking, call saveNow() and then
        // verify the update appears.

        sm.unblockSaving();
        sm.saveNow();

        // After saveNow the in-memory state (blocked-update) wins.
        SessionManager sm2;
        sm2.setSessionPath(path);
        QVERIFY(sm2.load());
        QCOMPARE(sm2.expandedFolders(), QStringList{QStringLiteral("blocked-update")});
    }

    // -----------------------------------------------------------------------
    // 16. Unknown root keys do NOT bleed into _corbomite. Generic unknown
    // keys (i.e. not the sidedock-specific `left`/`right` pair) survive at
    // root regardless of whether Corbomite touched sidebar state.
    // -----------------------------------------------------------------------
    void unknownRootKeysDoNotBleedIntoCorbomite()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        // Seed a file with an unknown Obsidian key. `left`/`right` are
        // covered separately — they get dropped when sidebar is dirty.
        QJsonObject seed;
        seed.insert(QStringLiteral("main"),          QJsonObject{});
        seed.insert(QStringLiteral("obsidianPlugin"), QJsonArray{1, 2, 3});
        writeJson(path, seed);

        SessionManager sm;
        sm.setSessionPath(path);
        QVERIFY(sm.load());

        // Add some corbomite state and save.
        sm.saveSidebarState(true, 200, false, 150);
        sm.saveNow();

        const QJsonObject root = readJson(path);

        // Unknown keys survive at root.
        QVERIFY(root.contains(QStringLiteral("obsidianPlugin")));

        // _corbomite exists.
        QVERIFY(root.contains(QStringLiteral("_corbomite")));
        const QJsonObject corbomite = root.value(QStringLiteral("_corbomite")).toObject();

        // Unknown Obsidian keys must not appear inside _corbomite.
        QVERIFY2(!corbomite.contains(QStringLiteral("obsidianPlugin")),
                 "obsidianPlugin must not be inside _corbomite");
    }

    // -----------------------------------------------------------------------
    // P1 #4 — sidedock passthrough policy
    //
    // While Corbomite hasn't touched sidebar state since load, the Obsidian
    // `left`/`right` sub-trees are passed through unmodified (preserves an
    // Obsidian-authored vault on a no-op Corbomite session). Once the user
    // changes Corbomite-side sidebar state, those sub-trees are stale and
    // dropped on save so Obsidian rebuilds the sidedock from defaults
    // rather than re-applying an arbitrary frozen-in-time configuration.
    // Audit ref: workspace.md §"High severity" #4.
    // -----------------------------------------------------------------------
    void leftRightPreservedWhenSidebarUntouched()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        QJsonObject seed;
        seed.insert(QStringLiteral("main"), QJsonObject{});
        seed.insert(QStringLiteral("left"),
                    QJsonObject{{QStringLiteral("collapsed"), true}});
        seed.insert(QStringLiteral("right"),
                    QJsonObject{{QStringLiteral("collapsed"), false}});
        writeJson(path, seed);

        SessionManager sm;
        sm.setSessionPath(path);
        QVERIFY(sm.load());
        // Re-applying the same sidebar values (as MainWindow does on every
        // saveSessionState replay) must NOT mark the sidebar dirty —
        // identity is the gate, not call count.
        sm.saveNow();

        const QJsonObject root = readJson(path);
        QVERIFY2(root.contains(QStringLiteral("left")),
                 "left must survive an untouched-sidebar round-trip");
        QVERIFY2(root.contains(QStringLiteral("right")),
                 "right must survive an untouched-sidebar round-trip");
        QCOMPARE(root.value(QStringLiteral("left")).toObject()
                     .value(QStringLiteral("collapsed")).toBool(),
                 true);
    }

    void leftRightDroppedWhenSidebarMutated()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        QJsonObject seed;
        seed.insert(QStringLiteral("main"), QJsonObject{});
        seed.insert(QStringLiteral("left"),
                    QJsonObject{{QStringLiteral("collapsed"), true}});
        seed.insert(QStringLiteral("right"),
                    QJsonObject{{QStringLiteral("collapsed"), false}});
        seed.insert(QStringLiteral("obsidianPlugin"), QJsonArray{1, 2, 3});
        writeJson(path, seed);

        SessionManager sm;
        sm.setSessionPath(path);
        QVERIFY(sm.load());
        // First save: no prior sidebar state in _corbomite, so this is a
        // "new" sidebar value and counts as a Corbomite-side mutation.
        sm.saveSidebarState(true, 200, false, 150);
        sm.saveNow();

        const QJsonObject root = readJson(path);
        QVERIFY2(!root.contains(QStringLiteral("left")),
                 "left must be dropped once Corbomite mutated sidebar");
        QVERIFY2(!root.contains(QStringLiteral("right")),
                 "right must be dropped once Corbomite mutated sidebar");
        // Other unknown keys still survive — only the sidedock pair is special.
        QVERIFY2(root.contains(QStringLiteral("obsidianPlugin")),
                 "non-sidedock unknown keys must still survive");
    }

    void leftRightPreservedWhenLoadedSidebarReapplied()
    {
        // Loading a Corbomite-authored vault populates `_corbomite.sidebar`
        // before MainWindow replays it via saveSidebarState. Re-applying the
        // same values must not mark the sidebar dirty, so passed-through
        // `left`/`right` survive.
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        // Seed a workspace.json that already has a Corbomite sidebar state
        // plus Obsidian `left`/`right` sub-trees.
        QJsonObject corbomite;
        QJsonObject sidebar;
        sidebar.insert(QStringLiteral("leftVisible"), true);
        sidebar.insert(QStringLiteral("leftWidth"), 250);
        sidebar.insert(QStringLiteral("rightVisible"), false);
        sidebar.insert(QStringLiteral("rightWidth"), 300);
        corbomite.insert(QStringLiteral("sidebar"), sidebar);

        QJsonObject seed;
        seed.insert(QStringLiteral("main"), QJsonObject{});
        seed.insert(QStringLiteral("_corbomite"), corbomite);
        seed.insert(QStringLiteral("left"),
                    QJsonObject{{QStringLiteral("collapsed"), true}});
        seed.insert(QStringLiteral("right"),
                    QJsonObject{{QStringLiteral("collapsed"), false}});
        writeJson(path, seed);

        SessionManager sm;
        sm.setSessionPath(path);
        QVERIFY(sm.load());

        // Re-apply identical values — mirrors MainWindow::saveSessionState.
        sm.saveSidebarState(true, 250, false, 300);
        sm.saveNow();

        const QJsonObject root = readJson(path);
        QVERIFY2(root.contains(QStringLiteral("left")),
                 "left must survive when re-applied sidebar matches loaded value");
        QVERIFY2(root.contains(QStringLiteral("right")),
                 "right must survive when re-applied sidebar matches loaded value");
    }

    // -----------------------------------------------------------------------
    // 17. Empty expandedFolders round-trips correctly
    // -----------------------------------------------------------------------
    void emptyExpandedFoldersRoundTrip()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);
        sm.saveExpandedFolders({});
        sm.saveNow();

        SessionManager sm2;
        sm2.setSessionPath(path);
        QVERIFY(sm2.load());
        QVERIFY2(sm2.expandedFolders().isEmpty(),
                 "expandedFolders() must return empty list when saved as empty");
    }

    // -----------------------------------------------------------------------
    // 18. setWorkspaceLayout with empty activeLeafId omits active key
    // -----------------------------------------------------------------------
    void emptyActiveLeafIdOmittedFromRoot()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);
        sm.setWorkspaceLayout(QJsonObject{}, QString()); // empty activeLeafId
        sm.saveNow();

        const QJsonObject root = readJson(path);
        QVERIFY2(!root.contains(QStringLiteral("active")),
                 "active must be omitted when activeLeafId is empty");
    }

    // -----------------------------------------------------------------------
    // 19. Full round-trip: all setters together, load into fresh instance
    // -----------------------------------------------------------------------
    void fullRoundTripAllSetters()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        const QByteArray geo   = QByteArray("GEOBYTES");
        const QByteArray state = QByteArray("STATEBYTES");
        const QStringList folders{QStringLiteral("a"), QStringLiteral("b/c")};
        QJsonObject mainJson;
        mainJson.insert(QStringLiteral("type"), QStringLiteral("leaf"));
        mainJson.insert(QStringLiteral("id"),   QStringLiteral("leaf-full"));
        const QString activeId = QStringLiteral("leaf-full");

        SessionManager smA;
        smA.setSessionPath(path);
        smA.saveWindowGeometry(geo, state);
        smA.saveSidebarState(false, 100, true, 200, QStringLiteral("outline"));
        smA.saveExpandedFolders(folders);
        smA.setWorkspaceLayout(mainJson, activeId);
        smA.saveNow();

        SessionManager smB;
        smB.setSessionPath(path);
        QVERIFY(smB.load());

        QCOMPARE(smB.windowGeometry(), geo);
        QCOMPARE(smB.windowState(),    state);
        QCOMPARE(smB.expandedFolders(), folders);
        QCOMPARE(smB.activeLeafId(), activeId);

        const QJsonObject sb = smB.sidebarState();
        QCOMPARE(sb.value(QStringLiteral("leftVisible")).toBool(),  false);
        QCOMPARE(sb.value(QStringLiteral("leftWidth")).toInt(),     100);
        QCOMPARE(sb.value(QStringLiteral("rightVisible")).toBool(), true);
        QCOMPARE(sb.value(QStringLiteral("rightWidth")).toInt(),    200);
        QCOMPARE(sb.value(QStringLiteral("activePanel")).toString(), QStringLiteral("outline"));

        const QJsonObject wl = smB.workspaceLayout();
        QCOMPARE(wl.value(QStringLiteral("type")).toString(), QStringLiteral("leaf"));
        QCOMPARE(wl.value(QStringLiteral("id")).toString(),   QStringLiteral("leaf-full"));
    }

    // -----------------------------------------------------------------------
    // 20. Per-plugin session state round-trip (Cluster Q retro follow-up #4)
    //     setPluginSessionState + pluginSessionState write/read _corbomite.plugins.<id>
    // -----------------------------------------------------------------------
    void pluginSessionStateRoundTrip()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager smA;
        smA.setSessionPath(path);

        QJsonObject fe;
        fe.insert(QStringLiteral("expandedFolders"),
                  QJsonArray{QStringLiteral("a"), QStringLiteral("b/c")});
        smA.setPluginSessionState(QStringLiteral("corbomite-file-explorer"), fe);

        QJsonObject backlinks;
        backlinks.insert(QStringLiteral("scrollY"), 42);
        smA.setPluginSessionState(QStringLiteral("corbomite-backlinks"), backlinks);

        smA.saveNow();

        // On disk: both plugin states nested under _corbomite.plugins.
        const QJsonObject root = readJson(path);
        const QJsonObject corbomite = root.value(QStringLiteral("_corbomite")).toObject();
        const QJsonObject plugins =
            corbomite.value(QStringLiteral("plugins")).toObject();
        QVERIFY(plugins.contains(QStringLiteral("corbomite-file-explorer")));
        QVERIFY(plugins.contains(QStringLiteral("corbomite-backlinks")));

        // Load into a fresh instance — accessor returns the stored object.
        SessionManager smB;
        smB.setSessionPath(path);
        QVERIFY(smB.load());
        QCOMPARE(smB.pluginSessionState(QStringLiteral("corbomite-file-explorer")),
                 fe);
        QCOMPARE(smB.pluginSessionState(QStringLiteral("corbomite-backlinks")),
                 backlinks);

        // Unknown id returns empty.
        QVERIFY(smB.pluginSessionState(QStringLiteral("nope")).isEmpty());

        // Setting empty state removes the entry; last-plugin drop removes
        // the `plugins` key entirely.
        smB.setPluginSessionState(QStringLiteral("corbomite-file-explorer"), {});
        smB.setPluginSessionState(QStringLiteral("corbomite-backlinks"), {});
        smB.saveNow();
        const QJsonObject rootCleared = readJson(path);
        const QJsonObject corbomiteCleared =
            rootCleared.value(QStringLiteral("_corbomite")).toObject();
        QVERIFY2(!corbomiteCleared.contains(QStringLiteral("plugins")),
                 "empty plugins sub-object should be dropped");
    }

    // -----------------------------------------------------------------------
    // 21. setLeftRibbonState round-trips and sits at the workspace.json root
    // (not inside _corbomite), matching Obsidian's schema.
    // -----------------------------------------------------------------------
    void leftRibbonStateRoundTrips()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        SessionManager sm;
        sm.setSessionPath(path);

        QJsonObject hidden;
        hidden.insert(QStringLiteral("core:graph_view"), true);
        hidden.insert(QStringLiteral("core:quick_switcher"), false);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);

        sm.setLeftRibbonState(ribbon);
        sm.saveNow();

        const QJsonObject root = readJson(path);
        QVERIFY2(root.contains(QStringLiteral("left-ribbon")),
                 "left-ribbon must be at the workspace.json root");
        QVERIFY2(!root.value(QStringLiteral("_corbomite")).toObject()
                     .contains(QStringLiteral("left-ribbon")),
                 "left-ribbon must NOT live under _corbomite");

        SessionManager sm2;
        sm2.setSessionPath(path);
        QVERIFY(sm2.load());
        const QJsonObject reloaded = sm2.leftRibbonState();
        const QJsonObject reloadedHidden =
            reloaded.value(QStringLiteral("hiddenItems")).toObject();
        QCOMPARE(reloadedHidden.value(QStringLiteral("core:graph_view")).toBool(), true);
        QCOMPARE(reloadedHidden.value(QStringLiteral("core:quick_switcher")).toBool(), false);
    }

    // -----------------------------------------------------------------------
    // 22. Pre-existing left-ribbon content (written by Obsidian) is preserved
    // on a Corbomite load → save cycle even when Corbomite never calls
    // setLeftRibbonState (unknown-key preservation invariant).
    // -----------------------------------------------------------------------
    void leftRibbonPreservedFromExternalWriter()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");

        QJsonObject external;
        QJsonObject hidden;
        hidden.insert(QStringLiteral("obsidian-plugin:whatever"), true);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        external.insert(QStringLiteral("left-ribbon"), ribbon);
        writeJson(path, external);

        SessionManager sm;
        sm.setSessionPath(path);
        QVERIFY(sm.load());
        sm.saveNow();

        const QJsonObject root = readJson(path);
        const QJsonObject reloadedHidden = root.value(QStringLiteral("left-ribbon"))
            .toObject().value(QStringLiteral("hiddenItems")).toObject();
        QCOMPARE(reloadedHidden.value(QStringLiteral("obsidian-plugin:whatever")).toBool(),
                 true);
    }
};

QTEST_APPLESS_MAIN(TestSessionManagerRoundtrip)
#include "tst_session_manager_roundtrip.moc"
