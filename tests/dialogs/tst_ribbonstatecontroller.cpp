// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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

} // namespace

class TestRibbonStateController : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void appliesHiddenItemsOnBind()
    {
        RibbonToolBar bar;
        bar.addRibbonIcon(QStringLiteral("core:a"), QIcon(),
                          QStringLiteral("A"), []() {});
        bar.addRibbonIcon(QStringLiteral("core:b"), QIcon(),
                          QStringLiteral("B"), []() {});

        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");
        QJsonObject external;
        QJsonObject hidden;
        hidden.insert(QStringLiteral("core:a"), true);
        hidden.insert(QStringLiteral("core:b"), false);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        external.insert(QStringLiteral("left-ribbon"), ribbon);
        writeJson(path, external);

        SessionManager sm;
        sm.setSessionPath(path);
        QVERIFY(sm.load());

        RibbonStateController controller(&bar, &sm);
        controller.applyFromSession();

        QVERIFY(!bar.isIconVisible(QStringLiteral("core:a")));
        QVERIFY(bar.isIconVisible(QStringLiteral("core:b")));
    }

    void appliesRetroactivelyToLateArrivingIcons()
    {
        QTemporaryDir tmp;
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");
        QJsonObject external;
        QJsonObject hidden;
        hidden.insert(QStringLiteral("plugin-x:Thing"), true);
        QJsonObject ribbon;
        ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        external.insert(QStringLiteral("left-ribbon"), ribbon);
        writeJson(path, external);

        SessionManager sm;
        sm.setSessionPath(path);
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
        const QString path = tmp.path() + QStringLiteral("/.obsidian/workspace.json");
        SessionManager sm;
        sm.setSessionPath(path);

        RibbonToolBar bar;
        RibbonStateController controller(&bar, &sm);
        controller.applyFromSession();

        bar.addRibbonIcon(QStringLiteral("core:graph"), QIcon(),
                          QStringLiteral("Graph"), []() {});
        bar.setIconVisible(QStringLiteral("core:graph"), false);

        sm.saveNow();

        const QJsonObject root = readJson(path);
        const QJsonObject hidden = root.value(QStringLiteral("left-ribbon"))
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
        sm1.setSessionPath(t1.path() + QStringLiteral("/.obsidian/workspace.json"));
        QJsonObject hidden; hidden.insert(QStringLiteral("core:g"), true);
        QJsonObject ribbon; ribbon.insert(QStringLiteral("hiddenItems"), hidden);
        sm1.setLeftRibbonState(ribbon);

        RibbonStateController controller(&bar, &sm1);
        controller.applyFromSession();
        QVERIFY(!bar.isIconVisible(QStringLiteral("core:g")));

        SessionManager sm2;
        QTemporaryDir t2;
        sm2.setSessionPath(t2.path() + QStringLiteral("/.obsidian/workspace.json"));

        controller.rebind(&sm2);
        controller.applyFromSession();
        QVERIFY(bar.isIconVisible(QStringLiteral("core:g")));
    }
};

QTEST_MAIN(TestRibbonStateController)
#include "tst_ribbonstatecontroller.moc"
