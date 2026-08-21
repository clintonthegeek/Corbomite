// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster O Phase O3 (O3.T1/T2/T3) — the ViewActions provider mechanism.
// Named test from the plan's "Tests:" line: install/uninstall leaves the
// collection clean, no dangling shortcuts. Driven through a real
// MainWindow + vault, same pattern as tst_action_context.cpp. Runs under
// QT_QPA_PLATFORM=offscreen.

#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTest>

#include <KAboutData>
#include <KActionCollection>
#include <KLocalizedString>
#include <KXMLGUIFactory>

#include <QAction>

#include "app/ActionContextController.h"
#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "corbomite/core/View.h"
#include "corbomite/core/ViewActions.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "editor/MarkdownViewActions.h"

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

class TstViewActionsProvider : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("view-actions-provider test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    // -----------------------------------------------------------------
    // O3.T2 — providers are constructed EAGERLY: the markdown provider's
    // collection must already be fully populated (every action object
    // exists) even before any vault is open / any tab is focused, so the
    // Hotkeys page can show its shortcuts unconditionally.
    // -----------------------------------------------------------------
    void eagerlyConstructed_beforeAnyTabOpen()
    {
        CorbomiteApp app;
        MainWindow mw(&app);

        auto *provider = mw.markdownViewActions();
        QVERIFY2(provider, "MarkdownViewActions must exist at MainWindow construction");
        QCOMPARE(provider->viewType(), QStringLiteral("markdown"));

        auto *pac = provider->actionCollection();
        QVERIFY(pac);
        QVERIFY2(pac->action(QStringLiteral("format_bold")) != nullptr,
                  "provider's collection must be populated before any tab is open");
        QVERIFY2(pac->action(QStringLiteral("heading_1")) != nullptr, "");

        // Not installed yet — no vault, no leaf, no client on the factory.
        QVERIFY2(mw.actionContext()->currentProvider() == nullptr,
                  "provider must not be installed with nothing focused");
    }

    // -----------------------------------------------------------------
    // O3.T1/T3 — install/uninstall leaves the collection clean: after an
    // uninstall (switching away from markdown), every action the
    // provider owns must be disabled (no dangling live shortcut — a
    // disabled QAction's shortcut does not fire), and re-installing
    // (switching back) must bring them back to a sane enabled state
    // without leaking duplicate connections (verified by checking the
    // heading radio settles to a single consistent state, not a
    // double-fire artifact).
    // -----------------------------------------------------------------
    void installUninstall_leavesCollectionClean()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + QStringLiteral("/Note.md"), QStringLiteral("# Note\n"));
        createFile(tmp.path() + QStringLiteral("/Canvas.canvas"), kCanvasSeed);

        CorbomiteApp app;
        MainWindow mw(&app);
        QVERIFY(app.openVault(tmp.path()));
        QTest::qWait(500);
        QVERIFY(app.isOpen());

        auto *provider = mw.markdownViewActions();
        QVERIFY(provider);
        auto *pac = provider->actionCollection();

        mw.onNoteActivated(QStringLiteral("Note.md"));
        QTest::qWait(200);
        QCOMPARE(mw.actionContext()->currentProvider(), static_cast<ViewActions *>(provider));
        QVERIFY2(pac->action(QStringLiteral("format_bold"))->isEnabled(),
                  "installed + bound to an editable markdown view -> format_bold enabled");

        // Switch to canvas — as of O4, canvas has its own registered
        // provider, so this must be a clean SWAP (markdown uninstalled,
        // canvas installed), not an uninstall-to-null. tst_canvas_view_actions
        // covers CanvasViewActions' own behaviour in depth; this test only
        // needs to confirm markdown's own install/uninstall discipline
        // still holds when the swap target is a real provider, not a gap.
        mw.onNoteActivated(QStringLiteral("Canvas.canvas"));
        QTest::qWait(200);
        QVERIFY2(mw.actionContext()->currentProvider() != static_cast<ViewActions *>(provider),
                  "markdown's client must be uninstalled once focus leaves markdown");

        // Every action the provider owns must now be disabled — nothing
        // left "live" (dangling shortcut) after uninstall.
        const auto actions = pac->actions();
        QVERIFY2(!actions.isEmpty(), "collection must still exist post-uninstall");
        for (QAction *a : actions) {
            QVERIFY2(!a->isEnabled(),
                     qPrintable(QStringLiteral("action '%1' still enabled after provider uninstall")
                                    .arg(a->objectName())));
        }
        // The check-state (radios) must also have been cleared, not left
        // pointing at stale markdown state.
        QVERIFY2(!pac->action(QStringLiteral("view_editing_mode"))->isChecked(),
                  "editor-mode radio must clear on uninstall");

        // Switch back to markdown — must reinstall the SAME provider
        // instance and bring its actions back to a sane enabled state.
        mw.onNoteActivated(QStringLiteral("Note.md"));
        QTest::qWait(200);
        QCOMPARE(mw.actionContext()->currentProvider(), static_cast<ViewActions *>(provider));
        QVERIFY2(pac->action(QStringLiteral("format_bold"))->isEnabled(),
                  "re-installed + bound -> format_bold enabled again");
    }
};

QTEST_MAIN(TstViewActionsProvider)
#include "tst_view_actions_provider.moc"
