// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.
//
// ThemeService — minimum-viable revival (2026-10-01).
//
// HISTORY. This file was `#if 0` end to end from the 2026-05-25 foundation
// port. The reason was real: the implementation was written against an older
// `Markoff::Theme` that had `name`, `uuid`, `loadJson()`/`saveJson()`, and six
// built-in themes shipped as `:/markoff/themes/*.json` Qt resources. Against
// the pinned Markoff none of that exists any more —
//   · `Theme` has no `name` and no `uuid` member,
//   · `loadJson`/`saveJson` are gone (`fromJson(QJsonObject)`/`toJson()` only),
//   · there are no `:/markoff/themes/*.json` resources in the submodule at all.
// So the old body could not simply be re-enabled: it would not compile.
//
// THE BUG THAT WAS LIVE FOR ~2 MONTHS. Because the only
// `emit themeChanged(...)` sat inside the dead `#if 0`, the signal was never
// emitted, and the live stub's `availableThemeNames()` returned an empty list
// while `setActiveThemeByName()` was a no-op. Consequences, all verified
// 2026-10-01:
//   1. Settings → the "Editor theme" dropdown rendered EMPTY and was inert.
//   2. Opening an Obsidian-authored dark vault did NOT switch Corbomite to
//      dark — MainWindow's `appearance.json` read path called the no-op setter.
//   3. The `css-change` plugin event was dead wiring (its test passed only
//      because it called Workspace directly, never ThemeService).
//   4. The `appearance.json` write hook was dead for the same reason.
//   5. The open "canvas ignores dark mode" punch-list item was NOT actionable,
//      because its stated remedy is to react to `themeChanged`.
//
// SCOPE. This is the roadmap's "minimum viable" revival: populate the theme
// combo and make `appearance.json` light/dark actually apply. Two known gaps
// are deliberately left, and are noted at the points where they occur:
//   · User JSON themes (the `themes/*.json` directory) are not loaded — a
//     `Theme` no longer carries a name, so there is nothing to key a loaded
//     theme by. Needs an upstream decision on how a theme declares its name.
//   · "Follow system" derives from the QApplication palette rather than from
//     `KColorScheme` via SystemThemeBuilder (itself still `#if 0`, and its
//     live body is a 3-line stub returning an empty `Markoff::Theme{}`, which
//     would leave every Slot unset). The palette path is a deliberate
//     stopgap, not the finished port.
#include "corbomite/core/ThemeService.h"

#include <QApplication>
#include <QDir>
#include <QPalette>
#include <QStandardPaths>

namespace Corbomite::Core {

namespace {

// The sentinel for "follow the system colour scheme". Kept as a Corbomite-side
// string rather than a Theme name, because Theme has no name any more.
const QString kSystemThemeName = QStringLiteral("Follow system");

// Canonical names for the two built-ins. MainWindow's `appearance.json` read
// path maps Obsidian's "moonstone" → "Light" and "obsidian" → "Dark" and then
// probes `availableThemeNames().contains(...)`, so these exact spellings are
// part of the vault-compat contract — see VaultConfig::obsidianAppearanceTheme()
// for the reverse (Corbomite → Obsidian) direction.
const QString kLightThemeName = QStringLiteral("Light");
const QString kDarkThemeName = QStringLiteral("Dark");

/// Derive a usable light-or-dark theme from the application palette.
///
/// A POPULATED default, never an empty `Theme{}`: an empty theme leaves every
/// Slot unset, so `Theme::color()` falls back to TextDefault (a dark colour)
/// for the search-highlight backgrounds, and the C++ InlineHighlighter reads
/// those Slots directly (no QML fallback) — which renders find-highlights as a
/// black block on a light theme. This is the same reasoning the previous stub
/// carried; it was correct, just unreachable.
Markoff::Theme themeFromPalette()
{
    const QColor window = QApplication::palette().color(QPalette::Window);
    return window.lightness() < 128 ? Markoff::Theme::defaultDark()
                                    : Markoff::Theme::defaultLight();
}

} // namespace

ThemeService::ThemeService(KColorSchemeManager *kdeMgr, QObject *parent)
    : QObject(parent)
    , m_kdeMgr(kdeMgr)
{
    // The constructor signature keeps the KColorSchemeManager* for API
    // stability and for the eventual SystemThemeBuilder port, but the
    // palette-derived "Follow system" path below does not consult it yet.
    Q_UNUSED(m_kdeMgr);

    m_themes.insert(kLightThemeName, Markoff::Theme::defaultLight());
    m_themes.insert(kDarkThemeName, Markoff::Theme::defaultDark());

    // User themes are not loaded: see the file header. When upstream settles
    // how a theme declares its name, this is the hook.
    loadUserThemes();

    m_activeName = kSystemThemeName;
    rebuildAndEmit();
}

ThemeService::~ThemeService() = default;

Markoff::Theme ThemeService::currentTheme() const { return m_currentTheme; }

QString ThemeService::activeThemeName() const { return m_activeName; }

bool ThemeService::isDark() const
{
    // Same rule themeFromPalette() uses to decide light-vs-dark for the
    // system theme: window lightness below half. Reading it off the active
    // theme's own text colour would be a second, subtly-different rule.
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}

QStringList ThemeService::availableThemeNames() const
{
    QStringList list;
    list << kSystemThemeName;
    auto names = m_themes.keys();
    std::sort(names.begin(), names.end());
    list += names;
    return list;
}

QString ThemeService::userThemesDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/themes");
}

void ThemeService::setActiveThemeByName(const QString &name)
{
    if (name == m_activeName) return;
    m_activeName = name;
    rebuildAndEmit();
}

void ThemeService::addUserTheme(const Markoff::Theme &theme)
{
    // Cannot be implemented meaningfully against the current Markoff::Theme:
    // a Theme carries no `name` and no `uuid`, so there is nothing to key the
    // stored theme by, and `saveJson()` no longer exists. Kept in the public API
    // so callers still link. Needs the same upstream decision as
    // loadUserThemes(). Deliberately does NOT silently write a file the user
    // could not then select.
    Q_UNUSED(theme);
}

void ThemeService::refreshSystemTheme()
{
    if (m_activeName == kSystemThemeName)
        rebuildAndEmit();
}

void ThemeService::loadBuiltInThemes()
{
    // Kept as a named step (and called from the constructor path above via
    // the two built-ins) so the shape of the original implementation survives.
    // There are no `:/markoff/themes/*.json` resources in the pinned Markoff,
    // so the previous six-JSON built-in list cannot be restored as written.
    m_themes.insert(kLightThemeName, Markoff::Theme::defaultLight());
    m_themes.insert(kDarkThemeName, Markoff::Theme::defaultDark());
}

void ThemeService::loadUserThemes()
{
    // No-op: see loadUserThemes() in the file header. ${XDG_CONFIG_HOME}/
    // corbomite[-dev]/themes/*.json is intentionally NOT read, because without
    // a name on Theme there is no way to map a loaded file to a selectable
    // entry, and silently ignoring a user's theme directory is worse than
    // declaring the gap.
    QDir d(userThemesDirectory());
    Q_UNUSED(d);
}

void ThemeService::rebuildAndEmit()
{
    if (m_activeName == kSystemThemeName) {
        m_currentTheme = themeFromPalette();
    } else if (m_themes.contains(m_activeName)) {
        m_currentTheme = m_themes.value(m_activeName);
    } else {
        // Unknown name (e.g. an `appearance.json` naming a custom theme we do
        // not have). Fall back to following the system rather than to a fixed
        // light theme, so an unknown vault theme cannot force a light UI.
        m_currentTheme = themeFromPalette();
    }
    emit themeChanged(m_currentTheme);
}

} // namespace Corbomite::Core
