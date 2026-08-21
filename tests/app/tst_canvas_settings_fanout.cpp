// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster O Phase O4 (O4.T4) — settings fan-out. Named test from the plan's
// "Tests:" line: two open canvases both follow a kcfg change, not just the
// focused one. Runs under QT_QPA_PLATFORM=offscreen.

#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTest>

#include <KAboutData>
#include <KLocalizedString>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "corbomite/core/Command.h"
#include "canvas/CanvasFileView.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomitesettings.h"

#include <canvas/CanvasAlignmentStrategy.h>
#include <canvas/CanvasScene.h>
#include <canvas/CanvasView.h>

using namespace Corbomite;

namespace {

void createFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content.toUtf8());
}

const QString kCanvasSeed = QStringLiteral(
    "{\"nodes\":[{\"id\":\"n1\",\"type\":\"text\",\"x\":0,\"y\":0,"
    "\"width\":250,\"height\":60,\"text\":\"hi\"}],\"edges\":[]}");

} // namespace

class TstCanvasSettingsFanout : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("canvas-settings-fanout test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    void cleanup()
    {
        CorbomiteSettings::self()->setSnapToGrid(true);
        CorbomiteSettings::self()->setSnapToObjects(true);
        CorbomiteSettings::self()->setShowGrid(true);
        CorbomiteSettings::self()->save();
    }

    // A settings change made while ONE canvas is focused must reach a
    // SECOND, unfocused/background canvas leaf too — not just the bound
    // one CanvasViewActions can see directly.
    void settingsChange_reachesEveryOpenCanvas_notJustFocused()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/CanvasA.canvas"), kCanvasSeed);
        createFile(tmp.path() + QStringLiteral("/CanvasB.canvas"), kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);

        // Open A, then split so B opens in a second leaf without replacing
        // A — both stay live leaves in the workspace (only one is
        // "focused").
        mw.onNoteActivated(QStringLiteral("CanvasA.canvas"));
        QTest::qWait(200);
        QVERIFY(mw.commandRegistry());
        QVERIFY(mw.commandRegistry()->executeById(QStringLiteral("split_right")));
        QTest::qWait(200);
        mw.onNoteActivated(QStringLiteral("CanvasB.canvas"));
        QTest::qWait(200);

        auto *ws = mw.findChild<Workspace *>();
        QVERIFY(ws);
        QList<CanvasFileView *> canvases;
        for (auto *leaf : ws->allLeaves()) {
            if (leaf->isDeferred()) continue;
            if (auto *cv = qobject_cast<CanvasFileView *>(leaf->view()))
                canvases << cv;
        }
        QVERIFY2(canvases.size() >= 2, "expected two live canvas leaves (A + B)");

        for (auto *cv : canvases) {
            auto *align = cv->canvasWidget()->canvasScene()->alignmentStrategy();
            QVERIFY(align);
            QVERIFY2(align->snapToGridEnabled(), "kcfg default is true, both leaves");
        }

        // Flip the setting directly through kcfg (as if from whichever
        // canvas happens to be focused) and confirm BOTH leaves follow,
        // including whichever one is NOT currently focused.
        CorbomiteSettings::self()->setSnapToGrid(false);
        CorbomiteSettings::self()->setShowGrid(false);
        CorbomiteSettings::self()->save();
        QTest::qWait(50);

        for (auto *cv : canvases) {
            auto *tab = cv->canvasWidget();
            QVERIFY2(!tab->canvasScene()->alignmentStrategy()->snapToGridEnabled(),
                     "every open canvas must follow the setting, not just the focused one");
            QVERIFY2(!tab->canvasView()->gridVisible(), "");
        }
    }
};

QTEST_MAIN(TstCanvasSettingsFanout)
#include "tst_canvas_settings_fanout.moc"
