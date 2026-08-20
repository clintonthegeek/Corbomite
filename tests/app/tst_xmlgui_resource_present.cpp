// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster P Phase P1 — named regression test for P1.T2's risk: CorbomiteApp's
// compiled-in KXMLGUI .rc resource (embedded via qt_add_resources in the
// root CMakeLists.txt) must still self-register at process startup now that
// CorbomiteApp is a SHARED library rather than STATIC. Static-library Qt
// resources need an explicit Q_INIT_RESOURCE call that nothing in this tree
// makes; shared-library resources self-initialise when the .so loads. If
// that stops being true, the app's menus/toolbars silently come up empty —
// the exact failure mode the Cluster O3 stale-cache incident already taught
// this project to misdiagnose as something else.
#include <QtTest/QtTest>
#include <QFile>

class TestXmlGuiResourcePresent : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void resourceIsReadableAtRuntime()
    {
        QFile rc(CORBOMITE_XMLGUI_RESOURCE_PATH);
        QVERIFY2(rc.exists(),
            qPrintable(QStringLiteral("compiled-in KXMLGUI resource missing: %1")
                .arg(CORBOMITE_XMLGUI_RESOURCE_PATH)));
        QVERIFY(rc.open(QIODevice::ReadOnly));
        const QByteArray contents = rc.readAll();
        QVERIFY(!contents.isEmpty());
        QVERIFY2(contents.contains("gui"),
            "resource content doesn't look like a KXMLGUI .rc file");
    }
};

QTEST_GUILESS_MAIN(TestXmlGuiResourcePresent)
#include "tst_xmlgui_resource_present.moc"
