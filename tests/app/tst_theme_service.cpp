// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QTest>

#include "corbomite/core/ThemeService.h"
#include <markoff/core/Theme.h>

using Markoff::Theme;

// Regression guard for the "black Live find-highlight" bug (2026-06-14):
// the ThemeService stub returned an empty Markoff::Theme{}, so every unset
// Slot — including the search-highlight backgrounds — fell back through
// Theme::color() to TextDefault, a dark color. The C++ InlineHighlighter
// reads these Slots directly (no QML "#222222"/palette fallback), so Live
// find-highlights rendered as a black block. currentTheme() must hand the
// leaves a populated theme whose search-highlight slots are valid and
// visibly distinct from the text color.
class TestThemeService : public QObject {
    Q_OBJECT
private slots:
    void currentTheme_hasVisibleSearchHighlightSlots() {
        Corbomite::Core::ThemeService svc(nullptr);
        const Theme t = svc.currentTheme();

        const QColor textDefault = t.color(Theme::Slot::TextDefault);
        const QColor match  = t.color(Theme::Slot::SearchMatchBackground);
        const QColor active = t.color(Theme::Slot::SearchActiveMatchBackground);

        QVERIFY(match.isValid());
        QVERIFY(active.isValid());
        // Bug signature: an unset search slot falls back to the text color.
        QVERIFY2(match != textDefault,
                 "SearchMatchBackground must not fall back to the text color");
        QVERIFY2(active != textDefault,
                 "SearchActiveMatchBackground must not fall back to the text color");
    }
};

QTEST_MAIN(TestThemeService)
#include "tst_theme_service.moc"
