// SPDX-License-Identifier: GPL-3.0-or-later
//
// ThemeService minimum-viable revival — 2026-10-01.
//
// This test exists because ThemeService was `#if 0` end to end for ~2 months
// and the failure was completely silent: every accessor had a linkable stub,
// so the app built, launched, and ran. The only `emit themeChanged(...)` was
// inside the dead block, so the signal never fired, and the stub's
// `availableThemeNames()` returned an empty list. Downstream that meant the
// Settings "Editor theme" dropdown rendered empty and inert, and opening an
// Obsidian-authored dark vault did not switch Corbomite to dark.
//
// Note the sibling `tst_system_theme_builder` is still `if(FALSE)`-disabled —
// correctly: it asserts against the pre-port `Markoff::Theme` shape
// (`t.name`, `t.elements`, `Markoff::Element`), which no longer exists.

#include "corbomite/core/ThemeService.h"

#include <QSignalSpy>
#include <QTest>

using Corbomite::Core::ThemeService;

class TestThemeService : public QObject
{
    Q_OBJECT

private slots:
    void offersFollowSystemLightAndDark();
    void themeIsPopulatedNotEmpty();
    void setActiveThemeByName_emitsThemeChanged();
    void setActiveThemeByName_sameName_doesNotReEmit();
    void setActiveThemeByName_unknownName_isSafe();
    void darkAndLightActuallyDiffer();
};

// The Settings combo is populated from this list, and MainWindow's
// appearance.json read path probes it for custom-theme names. An empty list is
// what produced the inert dropdown.
void TestThemeService::offersFollowSystemLightAndDark()
{
    ThemeService svc(nullptr);

    const QStringList names = svc.availableThemeNames();
    QVERIFY2(!names.isEmpty(),
             "availableThemeNames() empty — the Settings theme dropdown would "
             "render blank and inert (the 2026-10-01 regression).");
    QVERIFY2(names.contains(QStringLiteral("Follow system")),
             qPrintable(QStringLiteral("missing 'Follow system': %1")
                            .arg(names.join(QLatin1Char(',')))));
    QVERIFY2(names.contains(QStringLiteral("Light")),
             qPrintable(QStringLiteral("missing 'Light' — MainWindow maps "
                                       "Obsidian 'moonstone' to exactly this "
                                       "spelling: %1")
                            .arg(names.join(QLatin1Char(',')))));
    QVERIFY2(names.contains(QStringLiteral("Dark")),
             qPrintable(QStringLiteral("missing 'Dark' — MainWindow maps "
                                       "Obsidian 'obsidian' to exactly this "
                                       "spelling: %1")
                            .arg(names.join(QLatin1Char(',')))));
}

// A default is not enough — the theme must be POPULATED. An empty Markoff::Theme
// leaves every Slot unset, so color() falls back to TextDefault (a dark colour)
// for the search-highlight backgrounds, and the C++ InlineHighlighter reads
// those Slots directly (no QML fallback) — rendering find-highlights as a black
// block. This is the specific failure the previous stub's comment warned about.
void TestThemeService::themeIsPopulatedNotEmpty()
{
    ThemeService svc(nullptr);

    const auto t = svc.currentTheme();
    const QColor bg = t.color(Markoff::Theme::Slot::EditorBackground);
    const QColor fg = t.color(Markoff::Theme::Slot::TextDefault);
    QVERIFY2(bg.isValid(), "EditorBackground unset — theme is empty");
    QVERIFY2(fg.isValid(), "TextDefault unset — theme is empty");
    QVERIFY2(bg != fg, "background and foreground identical — theme not populated");
}

// THE core regression: the signal must actually be emitted. Nothing in the app
// depends on ThemeService's internals; they all depend on this signal.
void TestThemeService::setActiveThemeByName_emitsThemeChanged()
{
    ThemeService svc(nullptr);
    QSignalSpy spy(&svc, &ThemeService::themeChanged);

    svc.setActiveThemeByName(QStringLiteral("Dark"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(svc.activeThemeName(), QStringLiteral("Dark"));

    svc.setActiveThemeByName(QStringLiteral("Light"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(svc.activeThemeName(), QStringLiteral("Light"));
}

// Idempotence: re-selecting the active theme must not churn, because
// MainWindow's appearance.json write hook is wired to themeChanged and would
// otherwise rewrite the file on every vault open.
void TestThemeService::setActiveThemeByName_sameName_doesNotReEmit()
{
    ThemeService svc(nullptr);
    svc.setActiveThemeByName(QStringLiteral("Dark"));
    QSignalSpy spy(&svc, &ThemeService::themeChanged);

    svc.setActiveThemeByName(QStringLiteral("Dark"));
    QCOMPARE(spy.count(), 0);
}

// An appearance.json can name a custom theme Corbomite does not have. That must
// not crash and must not wedge the service on a fixed light theme.
void TestThemeService::setActiveThemeByName_unknownName_isSafe()
{
    ThemeService svc(nullptr);
    QSignalSpy spy(&svc, &ThemeService::themeChanged);

    svc.setActiveThemeByName(QStringLiteral("SomeThemeWeDoNotHave"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(svc.activeThemeName(), QStringLiteral("SomeThemeWeDoNotHave"));
    // Falls back to following the system rather than forcing light.
    QVERIFY(svc.currentTheme().color(Markoff::Theme::Slot::EditorBackground).isValid());
}

// Guards against both built-ins silently collapsing to the same thing, which
// would make the combo a lie.
void TestThemeService::darkAndLightActuallyDiffer()
{
    ThemeService svc(nullptr);

    svc.setActiveThemeByName(QStringLiteral("Light"));
    const QColor lightBg =
        svc.currentTheme().color(Markoff::Theme::Slot::EditorBackground);
    svc.setActiveThemeByName(QStringLiteral("Dark"));
    const QColor darkBg =
        svc.currentTheme().color(Markoff::Theme::Slot::EditorBackground);

    QVERIFY(lightBg.isValid());
    QVERIFY(darkBg.isValid());
    QVERIFY2(lightBg != darkBg,
             "Light and Dark resolve to the same background — selecting Dark "
             "would not make the editor dark.");
    QVERIFY2(darkBg.lightness() < lightBg.lightness(),
             "the Dark theme's background is not darker than the Light theme's");
}

QTEST_MAIN(TestThemeService)
#include "tst_themeservice.moc"
