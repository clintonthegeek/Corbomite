// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster O Phase O3 (O3.T4, doctrine §D4) — tri-state toolbar visibility
// policy. Named test from the plan's "Tests:" line:
// Auto/AlwaysShow/AlwaysHide x in/out of context -> visibility + enabled
// state. Part 1 is a pure logic table over
// Corbomite::toolBarShouldBeVisible() (no widgets, no MainWindow). Part 2
// drives a real MainWindow + vault to confirm the persisted policy
// actually reaches a real KToolBar's visibility and that Tier B
// (per-action enablement) still applies out-of-context under AlwaysShow.
// Runs under QT_QPA_PLATFORM=offscreen.

#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTest>

#include <KAboutData>
#include <KActionCollection>
#include <KLocalizedString>
#include <KToolBar>

#include <QAction>

#include "app/ActionContextController.h"
#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "corbomite/core/ToolBarPolicy.h"
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

class TstToolBarPolicy : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("corbomite");
        KAboutData about(QStringLiteral("corbomite-test"),
                         QStringLiteral("Corbomite Test"),
                         QStringLiteral("0.1.0"),
                         QStringLiteral("toolbar-policy test"),
                         KAboutLicense::GPL_V3);
        KAboutData::setApplicationData(about);
    }

    // -----------------------------------------------------------------
    // Pure Tier-A visibility decision, all 3x2 combinations.
    // -----------------------------------------------------------------
    void shouldBeVisible_matrix()
    {
        QVERIFY(toolBarShouldBeVisible(ToolBarPolicy::Auto, /*inContext=*/true));
        QVERIFY(!toolBarShouldBeVisible(ToolBarPolicy::Auto, /*inContext=*/false));

        QVERIFY(toolBarShouldBeVisible(ToolBarPolicy::AlwaysShow, true));
        QVERIFY(toolBarShouldBeVisible(ToolBarPolicy::AlwaysShow, false));

        QVERIFY(!toolBarShouldBeVisible(ToolBarPolicy::AlwaysHide, true));
        QVERIFY(!toolBarShouldBeVisible(ToolBarPolicy::AlwaysHide, false));
    }

    void policyStringRoundTrip()
    {
        for (auto p : {ToolBarPolicy::Auto, ToolBarPolicy::AlwaysShow, ToolBarPolicy::AlwaysHide})
            QCOMPARE(toolBarPolicyFromString(toolBarPolicyToString(p)), p);
        // Unknown strings fall back to Auto rather than crashing/asserting
        // — defensive against a hand-edited or stale kcfg file.
        QCOMPARE(toolBarPolicyFromString(QStringLiteral("bogus")), ToolBarPolicy::Auto);
    }

    // -----------------------------------------------------------------
    // Integration: the persisted policy must actually drive the real
    // markdown KToolBar's visibility, in and out of context, and
    // AlwaysShow's "pinned visible, actions disabled out of context"
    // half of §D4 must hold via the provider's own Tier-B refresh.
    // -----------------------------------------------------------------
    void realToolBar_followsPolicyInAndOutOfContext()
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

        auto *ctx = mw.actionContext();
        QVERIFY(ctx);
        auto *toolBar = mw.findChild<KToolBar *>(QStringLiteral("markdownToolBar"));
        QVERIFY2(toolBar, "MainWindow must create a persistent 'markdownToolBar'");
        auto *provider = mw.markdownViewActions();
        QVERIFY(provider);
        auto *formatBold = provider->actionCollection()->action(QStringLiteral("format_bold"));
        QVERIFY(formatBold);

        // Note: MainWindow is never show()n in this offscreen test, so
        // QWidget::isVisible() (which requires the WHOLE ancestor chain,
        // including the never-shown top-level, to be visible) is always
        // false regardless of what our code does. isHidden() reflects
        // only this widget's own explicit shown/hidden state
        // (WA_WState_Hidden) — exactly "did applyToolBarPolicies() call
        // setVisible(true) or setVisible(false) on THIS toolbar," which
        // is what this test is actually checking.

        // --- Auto: visible only while a markdown tab is focused. ---
        ctx->setToolBarPolicy(QStringLiteral("markdown"), ToolBarPolicy::Auto);
        mw.onNoteActivated(QStringLiteral("Note.md"));
        QTest::qWait(200);
        QVERIFY2(!toolBar->isHidden(), "Auto + in context -> visible");
        QVERIFY2(formatBold->isEnabled(), "in context -> action enabled");

        mw.onNoteActivated(QStringLiteral("Canvas.canvas"));
        QTest::qWait(200);
        QVERIFY2(toolBar->isHidden(), "Auto + out of context -> hidden");
        QVERIFY2(!formatBold->isEnabled(), "out of context -> action disabled (provider unbound)");

        // --- AlwaysShow: pinned visible even out of context; actions
        // stay disabled out of context (Tier B, unrelated to visibility). ---
        ctx->setToolBarPolicy(QStringLiteral("markdown"), ToolBarPolicy::AlwaysShow);
        QVERIFY2(!toolBar->isHidden(), "AlwaysShow + out of context -> still visible");
        QVERIFY2(!formatBold->isEnabled(), "AlwaysShow does not itself re-enable out-of-context actions");

        mw.onNoteActivated(QStringLiteral("Note.md"));
        QTest::qWait(200);
        QVERIFY2(!toolBar->isHidden(), "AlwaysShow + in context -> visible");
        QVERIFY2(formatBold->isEnabled(), "in context -> action enabled regardless of policy");

        // --- AlwaysHide: pinned hidden even in context. ---
        ctx->setToolBarPolicy(QStringLiteral("markdown"), ToolBarPolicy::AlwaysHide);
        QVERIFY2(toolBar->isHidden(), "AlwaysHide + in context -> still hidden");
        QVERIFY2(formatBold->isEnabled(),
                  "AlwaysHide only affects toolbar visibility, not the action's own enable state");

        // Reset to Auto so this test doesn't leak state into the process's
        // persisted kcfg for any other test run in the same binary.
        ctx->setToolBarPolicy(QStringLiteral("markdown"), ToolBarPolicy::Auto);
    }
};

QTEST_MAIN(TstToolBarPolicy)
#include "tst_toolbar_policy.moc"
