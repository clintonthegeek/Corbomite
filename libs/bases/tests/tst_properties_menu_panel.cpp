// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/bases/PropertiesMenuPanel.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

using namespace Corbomite::Bases;

class TestPropertiesMenuPanel : public QObject
{
    Q_OBJECT
private slots:
    void addFormulaButton_emitsSignal()
    {
        PropertiesMenuPanel panel;
        QVector<PropertyId> order;
        panel.setState(&order, {}, [](const PropertyId &p) { return p.name; });
        QSignalSpy spy(&panel, &PropertiesMenuPanel::addFormulaRequested);

        auto *btn = panel.findChild<QPushButton *>(QStringLiteral("addFormulaButton"));
        QVERIFY(btn);
        btn->click();
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestPropertiesMenuPanel)
#include "tst_properties_menu_panel.moc"
