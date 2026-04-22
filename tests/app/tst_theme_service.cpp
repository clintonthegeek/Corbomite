// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include "corbomite/core/ThemeService.h"

class TestThemeService : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        // Force user themes dir to a temp location.
        QStandardPaths::setTestModeEnabled(true);
    }

    void availableNamesIncludeBuiltIns() {
        Corbomite::Core::ThemeService svc(nullptr);
        const auto names = svc.availableThemeNames();
        QVERIFY(names.contains(QStringLiteral("Follow system")));
        QVERIFY(names.contains(QStringLiteral("Light")));
        QVERIFY(names.contains(QStringLiteral("Dark")));
        QVERIFY(names.contains(QStringLiteral("Solarized Light")));
        QVERIFY(names.contains(QStringLiteral("Solarized Dark")));
        QVERIFY(names.contains(QStringLiteral("Dracula")));
        QVERIFY(names.contains(QStringLiteral("Monokai")));
    }

    void setActiveEmitsThemeChanged() {
        Corbomite::Core::ThemeService svc(nullptr);
        QSignalSpy spy(&svc, &Corbomite::Core::ThemeService::themeChanged);
        svc.setActiveThemeByName(QStringLiteral("Dracula"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(svc.currentTheme().name, QStringLiteral("Dracula"));
        QCOMPARE(svc.activeThemeName(), QStringLiteral("Dracula"));
    }

    void setActiveSameNameNoEmit() {
        Corbomite::Core::ThemeService svc(nullptr);
        svc.setActiveThemeByName(QStringLiteral("Light"));
        QSignalSpy spy(&svc, &Corbomite::Core::ThemeService::themeChanged);
        svc.setActiveThemeByName(QStringLiteral("Light"));
        QCOMPARE(spy.count(), 0);
    }

    void defaultIsFollowSystem() {
        Corbomite::Core::ThemeService svc(nullptr);
        QCOMPARE(svc.activeThemeName(), QStringLiteral("Follow system"));
        // The system-derived theme should have a populated Text element.
        QVERIFY(svc.currentTheme().elements.contains(Markoff::Element::Text));
    }
};

QTEST_MAIN(TestThemeService)
#include "tst_theme_service.moc"
