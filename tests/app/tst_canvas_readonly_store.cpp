// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster M Phase M5.4 — where the canvas read-only lock is persisted, and
// the tier split it must respect.
//
// Obsidian keeps it in a flat localStorage map keyed `canvas-<vault-relative
// path>`, written by `saveLocalData` (app/own/2028-n9.js:913-918) and read
// back by `getLocalData` through `localDataManager` (:560-579), whose loader
// applies the `hasOwnProperty("readonly") && typeof === "boolean"` guard
// (:879).
//
// Corbomite's equivalent is SessionManager's tier 3 — machine-local app data
// at <AppDataLocation>/vaults/<vaultId>/session.json — because that is the
// only machine-local store the app has (workspace compat doctrine, L0). It
// must NEVER appear in the .canvas file or in workspace.json, which is what
// tierFileLocalDataStaysOutOfTheVault() pins.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <KAboutData>
#include <KActionCollection>
#include <KLocalizedString>

#include <QAction>

#include "SessionManager.h"

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "canvas/CanvasFileView.h"
#include "canvas/CanvasViewActions.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/core/View.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include <canvas/CanvasScene.h>

using namespace Corbomite;

namespace {

void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content);
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

const QByteArray kCanvasSeed =
    "{\"nodes\":[{\"id\":\"n1\",\"type\":\"text\",\"x\":0,\"y\":0,"
    "\"width\":250,\"height\":60,\"text\":\"hi\"}],\"edges\":[]}";

/// Every tier-3 session.json currently on disk for this test-mode app dir.
QStringList tier3SessionFiles()
{
    const QString root =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/vaults");
    QStringList out;
    const QStringList vaults =
        QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &v : vaults) {
        const QString candidate = root + QLatin1Char('/') + v
                                  + QStringLiteral("/session.json");
        if (QFile::exists(candidate))
            out << candidate;
    }
    return out;
}

} // namespace

class TstCanvasReadOnlyStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("canvas read-only store test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    // Obsidian keys on TFile.path — vault-relative, forward slashes
    // (app/own/2028-n9.js:565). The key is the whole contract between the
    // in-memory flag and the store, so pin it exactly.
    void localDataKeyIsObsidianFormat()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        CanvasViewTab tab(tmp.path() + QStringLiteral("/Canvas.canvas"), tmp.path());
        QCOMPARE(tab.localDataKey(), QStringLiteral("canvas-Canvas.canvas"));

        // Subdirectory paths stay vault-relative rather than absolute.
        QVERIFY(QDir().mkpath(tmp.path() + QStringLiteral("/sub")));
        writeFile(tmp.path() + QStringLiteral("/sub/Deep.canvas"), kCanvasSeed);
        CanvasViewTab deep(tmp.path() + QStringLiteral("/sub/Deep.canvas"), tmp.path());
        QCOMPARE(deep.localDataKey(), QStringLiteral("canvas-sub/Deep.canvas"));

        // Outside a vault there is no TFile.path, so the absolute path is the
        // stable fallback.
        CanvasViewTab adHoc(tmp.path() + QStringLiteral("/Loose.canvas"));
        QCOMPARE(adHoc.localDataKey(),
                 QStringLiteral("canvas-") + tmp.path() + QStringLiteral("/Loose.canvas"));
    }

    // Round-trip through the seam: toggling the scene writes Obsidian's
    // payload shape, unlocking removes the key entirely (Obsidian's
    // saveLocalStorage(key, null), :569/:576).
    void lockRoundTripsThroughTheStoreSeam()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        QJsonObject store;
        CanvasViewTab tab(tmp.path() + QStringLiteral("/Canvas.canvas"), tmp.path());
        tab.setLocalDataStore(
            [&store](const QString &key) { return store.value(key).toObject(); },
            [&store](const QString &key, const QJsonObject &value) {
                if (value.isEmpty())
                    store.remove(key);
                else
                    store.insert(key, value);
            });

        auto *scene = tab.canvasScene();
        QVERIFY(scene);
        QVERIFY(!scene->isReadOnly());
        QVERIFY(!store.contains(QStringLiteral("canvas-Canvas.canvas")));

        scene->setReadOnly(true);
        QCOMPARE(store.value(QStringLiteral("canvas-Canvas.canvas")).toObject(),
                 QJsonObject({{QStringLiteral("readonly"), true}}));

        scene->setReadOnly(false);
        QVERIFY(!store.contains(QStringLiteral("canvas-Canvas.canvas")));
    }

    // Restoring on open, including Obsidian's exact load guard: the key must
    // be present AND a boolean. Anything else is ignored rather than coerced.
    void lockIsRestoredOnOpenAndOnlyFromABoolean()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        const auto sceneFor = [&](const QJsonObject &stored) {
            CanvasViewTab tab(tmp.path() + QStringLiteral("/Canvas.canvas"), tmp.path());
            tab.setLocalDataStore(
                [stored](const QString &) { return stored; },
                [](const QString &, const QJsonObject &) {});
            return tab.canvasScene()->isReadOnly();
        };

        QVERIFY(sceneFor(QJsonObject{{QStringLiteral("readonly"), true}}));
        QVERIFY(!sceneFor(QJsonObject{{QStringLiteral("readonly"), false}}));

        // Guard rejections: absent key, non-boolean value, empty object.
        QVERIFY(!sceneFor(QJsonObject{}));
        QVERIFY(!sceneFor(QJsonObject{{QStringLiteral("readonly"),
                                       QStringLiteral("true")}}));
        QVERIFY(!sceneFor(QJsonObject{{QStringLiteral("other"), true}}));
    }

    // The tier split itself. Tier 3 lives under AppDataLocation; the vault's
    // .obsidian dir (tier 2) and workspace.json must be byte-identical
    // before and after a lock toggle.
    void tierFileLocalDataStaysOutOfTheVault()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        SessionManager sm;
        sm.setVaultPath(tmp.path());
        sm.load();

        const QString tier2Path = tmp.path() + QStringLiteral("/.obsidian/corbomite/state.json");
        const QByteArray tier2Before = readFile(tier2Path);
        QVERIFY(!tier2Before.isEmpty());

        sm.setFileLocalData(QStringLiteral("canvas-Canvas.canvas"),
                            QJsonObject{{QStringLiteral("readonly"), true}});
        sm.saveNow();

        // Tier 3 has it, keyed exactly as Obsidian does.
        QCOMPARE(sm.fileLocalData(QStringLiteral("canvas-Canvas.canvas")),
                 QJsonObject({{QStringLiteral("readonly"), true}}));
        const QString tier3Path =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/vaults/") + sm.vaultId() + QStringLiteral("/session.json");
        QVERIFY2(QFile::exists(tier3Path), qPrintable(tier3Path));
        QVERIFY2(tier3SessionFiles().contains(tier3Path), qPrintable(tier3Path));
        const QJsonObject tier3 =
            QJsonDocument::fromJson(readFile(tier3Path)).object();
        const QJsonObject expectedLocalData{
            {QStringLiteral("canvas-Canvas.canvas"),
             QJsonObject{{QStringLiteral("readonly"), true}}}};
        QCOMPARE(tier3.value(QStringLiteral("localData")).toObject(),
                 expectedLocalData);

        // ...and nowhere in the vault.
        QCOMPARE(readFile(tier2Path), tier2Before);
        QVERIFY(!readFile(tmp.path() + QStringLiteral("/.obsidian/workspace.json"))
                     .contains("readonly"));
        QCOMPARE(readFile(tmp.path() + QStringLiteral("/Canvas.canvas")), kCanvasSeed);

        // An empty payload removes the key, like Obsidian's null write.
        sm.setFileLocalData(QStringLiteral("canvas-Canvas.canvas"), {});
        sm.saveNow();
        QVERIFY(sm.fileLocalData(QStringLiteral("canvas-Canvas.canvas")).isEmpty());
        QVERIFY(!QJsonDocument::fromJson(readFile(tier3Path))
                     .object().value(QStringLiteral("localData"))
                     .toObject().contains(QStringLiteral("canvas-Canvas.canvas")));

        // A fresh SessionManager over the same vault reloads it (the
        // "survives restart" leg of the plan's exit criteria).
        SessionManager reloaded;
        reloaded.setVaultPath(tmp.path());
        reloaded.load();
        QCOMPARE(reloaded.vaultId(), sm.vaultId());
        QVERIFY(reloaded.fileLocalData(QStringLiteral("canvas-Canvas.canvas")).isEmpty());
    }

    // End to end through the real MainWindow: the KAction toggles the scene,
    // the scene's readOnlyChanged reaches tier 3 via CanvasFileView's store,
    // and reopening the canvas restores the lock. Also asserts the two
    // Obsidian-shared files were never written.
    void lockActionDrivesSceneAndPersistsAcrossReopen()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString canvasPath = tmp.path() + QStringLiteral("/Canvas.canvas");
        writeFile(canvasPath, kCanvasSeed);

        const QJsonObject expected{{QStringLiteral("readonly"), true}};
        const auto lockedOnDisk = [&] {
            SessionManager probe;
            probe.setVaultPath(tmp.path());
            probe.load();
            return probe.fileLocalData(QStringLiteral("canvas-Canvas.canvas"));
        };

        {
            CorbomiteApp app;
            MainWindow mw(&app);
            QVERIFY(app.openVault(tmp.path()));
            QTest::qWait(500);

            mw.onNoteActivated(QStringLiteral("Canvas.canvas"));
            QTest::qWait(300);

            auto *actions = mw.canvasViewActions();
            QVERIFY(actions);
            QAction *lock = actions->actionCollection()->action(QStringLiteral("canvas_lock"));
            QVERIFY2(lock, "O4.T6 registered canvas_lock as a placeholder; M5.4 wires it");
            QVERIFY(lock->isCheckable());
            QVERIFY(lock->isEnabled());
            QVERIFY(!lock->isChecked());

            auto *ws = mw.findChild<Workspace *>();
            QVERIFY(ws);
            auto *view = qobject_cast<CanvasFileView *>(ws->activeLeaf()->view());
            QVERIFY(view);
            QVERIFY(!view->isReadOnly());

            // Trigger the KAction exactly as the Canvas menu does. trigger()
            // on a checkable action toggles, so it is called once per state
            // change (no setChecked() first, which would undo the toggle).
            lock->trigger();
            QTest::qWait(1);

            QVERIFY(view->isReadOnly());
            QVERIFY(view->canvasWidget()->canvasScene()->isReadOnly());
            // The action's checked state tracks the scene, not just the click.
            QVERIFY(lock->isChecked());
            // The Tier-B mutation actions go dead with it.
            QVERIFY(!view->canEdit());
            QVERIFY(!view->canUndo());

            // Reached tier 3 under Obsidian's key. SessionManager's save is
            // debounced (2s), so poll rather than sleep.
            QTRY_COMPARE_WITH_TIMEOUT(lockedOnDisk(), expected, 6000);

            // Neither Obsidian-shared surface was touched.
            QCOMPARE(readFile(canvasPath), kCanvasSeed);
            QVERIFY(!readFile(tmp.path() + QStringLiteral("/.obsidian/workspace.json"))
                         .contains("readonly"));
        }

        // A brand-new window over the same vault (KDDockWidgets forbids two
        // live MainWindows, hence the scope above): the lock comes back.
        {
            CorbomiteApp app;
            MainWindow mw(&app);
            QVERIFY(app.openVault(tmp.path()));
            QTest::qWait(500);
            mw.onNoteActivated(QStringLiteral("Canvas.canvas"));
            QTest::qWait(400);

            auto *ws = mw.findChild<Workspace *>();
            QVERIFY(ws);
            auto *view = qobject_cast<CanvasFileView *>(ws->activeLeaf()->view());
            QVERIFY(view);
            QVERIFY2(view->isReadOnly(), "lock must survive closing and reopening the canvas");
            QVERIFY2(!view->canEdit(), "a restored lock re-gates canEdit");
            QVERIFY(view->canvasWidget()->canvasScene()->isReadOnly());

            auto *actions = mw.canvasViewActions();
            QVERIFY(actions);
            QAction *lock = actions->actionCollection()->action(QStringLiteral("canvas_lock"));
            QVERIFY(lock);
            QVERIFY2(lock->isChecked(), "the restored state must reach the action's checked state");
            QVERIFY(lock->isEnabled());

            // Unlocking through the same action writes the removal.
            lock->trigger();
            QTest::qWait(1);
            QVERIFY(!view->isReadOnly());
            QVERIFY(!lock->isChecked());
            QVERIFY(view->canEdit());

            QTRY_VERIFY_WITH_TIMEOUT(lockedOnDisk().isEmpty(), 6000);
        }
    }
};

QTEST_MAIN(TstCanvasReadOnlyStore)
#include "tst_canvas_readonly_store.moc"