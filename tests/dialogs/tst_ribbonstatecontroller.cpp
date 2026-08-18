// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "SessionManager.h"
#include "app/RibbonStateController.h"
#include "app/RibbonToolBar.h"

using namespace Corbomite;

namespace {

void writeJson(const QString &path, const QJsonObject &obj)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

QJsonObject readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

// Left-ribbon state is tier 2 (vault-portable, Corbomite-native) under the
// compat-boundary doctrine — it lives at
// `<vault>/.obsidian/corbomite/state.json` (key `leftRibbon`), never in
// Obsidian's own `.obsidian/workspace.json`.
QString tier2Path(const QString &vaultRoot)
{
    return vaultRoot + QStringLiteral("/.obsidian/corbomite/state.json");
}

} // namespace

class TestRibbonStateController : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // SessionManager's tier 3 (machine-local) writer targets
    // QStandardPaths::AppDataLocation — sandbox it so tests never touch the
    // developer's real ~/.local/share.
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void appliesHiddenItemsOnBind()
    {
        RibbonToolBar bar;
        bar.addRibbonIcon(QStringLiteral("core:a"), QIcon(),
                          QStringLiteral("A"), []() {});
        bar.addRibbonIcon(QStringLiteral("core:b"), QIcon(),
                          QStringLiteral("B"), []() {});

        QTemporaryDir tmp;
        QJsonObject hidden;
        hidden.insert(QStringLiteral("core:a"), true);
        hidden.insert(QStringLiteral("core:b"), false);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        QJsonObject tier2;
        tier2.insert(QStringLiteral("leftRibbon"), ribbon);
        writeJson(tier2Path(tmp.path()), tier2);

        SessionManager sm;
        sm.setVaultPath(tmp.path());
        QVERIFY(sm.load());

        RibbonStateController controller(&bar, &sm);
        controller.applyFromSession();

        QVERIFY(!bar.isIconVisible(QStringLiteral("core:a")));
        QVERIFY(bar.isIconVisible(QStringLiteral("core:b")));
    }

    void appliesRetroactivelyToLateArrivingIcons()
    {
        QTemporaryDir tmp;
        QJsonObject hidden;
        hidden.insert(QStringLiteral("plugin-x:Thing"), true);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        QJsonObject tier2;
        tier2.insert(QStringLiteral("leftRibbon"), ribbon);
        writeJson(tier2Path(tmp.path()), tier2);

        SessionManager sm;
        sm.setVaultPath(tmp.path());
        QVERIFY(sm.load());

        RibbonToolBar bar;
        RibbonStateController controller(&bar, &sm);
        controller.applyFromSession();

        bar.addRibbonIcon(QStringLiteral("plugin-x:Thing"), QIcon(),
                          QStringLiteral("Thing"), []() {});

        QVERIFY(!bar.isIconVisible(QStringLiteral("plugin-x:Thing")));
    }

    void visibilityChangeWritesThroughToSession()
    {
        QTemporaryDir tmp;
        SessionManager sm;
        sm.setVaultPath(tmp.path());

        RibbonToolBar bar;
        RibbonStateController controller(&bar, &sm);
        controller.applyFromSession();

        bar.addRibbonIcon(QStringLiteral("core:graph"), QIcon(),
                          QStringLiteral("Graph"), []() {});
        bar.setIconVisible(QStringLiteral("core:graph"), false);

        sm.saveNow();

        const QJsonObject root = readJson(tier2Path(tmp.path()));
        const QJsonObject hidden = root.value(QStringLiteral("leftRibbon"))
            .toObject().value(QStringLiteral("hiddenItems")).toObject();
        QCOMPARE(hidden.value(QStringLiteral("core:graph")).toBool(), true);
    }

    void rebindClearsVisibilityFromStaleVault()
    {
        RibbonToolBar bar;
        bar.addRibbonIcon(QStringLiteral("core:g"), QIcon(),
                          QStringLiteral("G"), []() {});

        SessionManager sm1;
        QTemporaryDir t1;
        sm1.setVaultPath(t1.path());
        QJsonObject hidden; hidden.insert(QStringLiteral("core:g"), true);
        QJsonObject ribbon; ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        sm1.setLeftRibbonState(ribbon);

        RibbonStateController controller(&bar, &sm1);
        controller.applyFromSession();
        QVERIFY(!bar.isIconVisible(QStringLiteral("core:g")));

        SessionManager sm2;
        QTemporaryDir t2;
        sm2.setVaultPath(t2.path());

        controller.rebind(&sm2);
        controller.applyFromSession();
        QVERIFY(bar.isIconVisible(QStringLiteral("core:g")));
    }
};

QTEST_MAIN(TestRibbonStateController)
#include "tst_ribbonstatecontroller.moc"
