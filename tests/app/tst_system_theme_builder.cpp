// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QObject>
#include <QTest>

#include "corbomite/core/SystemThemeBuilder.h"

class TestSystemThemeBuilder : public QObject {
    Q_OBJECT
private slots:
    void buildsPlausibleTheme() {
        const auto t = Corbomite::Core::SystemThemeBuilder::buildFromKColorScheme(nullptr);
        QVERIFY(!t.name.isEmpty());
        QVERIFY(t.elements.contains(Markoff::Element::Text));
        QVERIFY(t.elements.contains(Markoff::Element::Selection));
        QVERIFY(t.elements.contains(Markoff::Element::H1));
        QCOMPARE(t.elements[Markoff::Element::H1].bold, true);
        QCOMPARE(t.elements[Markoff::Element::H1].fontSizeAdaptionPercent, 200);
        QVERIFY(!t.paint.calloutAccents.isEmpty());
        QVERIFY(t.paint.calloutAccents.contains(QStringLiteral("note")));
    }
};

QTEST_MAIN(TestSystemThemeBuilder)
#include "tst_system_theme_builder.moc"
