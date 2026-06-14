// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

// TODO(port-foundation-exploration): same fate as SystemThemeBuilder.cpp —
// disabled pending theme port (Markoff::Theme has a different shape from the
// master-side Theme this service was built against).
#if 0

#include "corbomite/core/ThemeService.h"

#include "corbomite/core/SystemThemeBuilder.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace Corbomite::Core {

namespace {
const QString kSystemThemeName = QStringLiteral("Follow system");
}

ThemeService::ThemeService(KColorSchemeManager *kdeMgr, QObject *parent)
    : QObject(parent), m_kdeMgr(kdeMgr)
{
    loadBuiltInThemes();
    loadUserThemes();
    m_activeName = kSystemThemeName;
    rebuildAndEmit();
}

ThemeService::~ThemeService() = default;

Markoff::Theme ThemeService::currentTheme() const { return m_currentTheme; }
QString ThemeService::activeThemeName() const { return m_activeName; }

QStringList ThemeService::availableThemeNames() const {
    QStringList list;
    list << kSystemThemeName;
    auto names = m_themes.keys();
    std::sort(names.begin(), names.end());
    list += names;
    return list;
}

QString ThemeService::userThemesDirectory() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/themes");
}

void ThemeService::setActiveThemeByName(const QString &name) {
    if (name == m_activeName) return;
    m_activeName = name;
    rebuildAndEmit();
}

void ThemeService::addUserTheme(const Markoff::Theme &theme) {
    QDir().mkpath(userThemesDirectory());
    Markoff::Theme t = theme;
    if (t.uuid.isEmpty())
        t.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString path = userThemesDirectory() + QStringLiteral("/")
        + t.uuid + QStringLiteral(".json");
    if (t.saveJson(path))
        m_themes[t.name] = t;
}

void ThemeService::refreshSystemTheme() {
    if (m_activeName == kSystemThemeName)
        rebuildAndEmit();
}

void ThemeService::loadBuiltInThemes() {
    static const char *kBuiltins[] = {
        "Light", "Dark", "SolarizedLight", "SolarizedDark", "Dracula", "Monokai",
    };
    for (const char *name : kBuiltins) {
        const QString path = QStringLiteral(":/markoff/themes/%1.json").arg(QString::fromLatin1(name));
        if (auto t = Markoff::Theme::loadJson(path))
            m_themes[t->name] = *t;
    }
}

void ThemeService::loadUserThemes() {
    QDir d(userThemesDirectory());
    if (!d.exists()) return;
    const auto entries = d.entryInfoList(QStringList{QStringLiteral("*.json")}, QDir::Files);
    for (const auto &fi : entries) {
        if (auto t = Markoff::Theme::loadJson(fi.absoluteFilePath()))
            m_themes[t->name] = *t;
    }
}

void ThemeService::rebuildAndEmit() {
    if (m_activeName == kSystemThemeName)
        m_currentTheme = SystemThemeBuilder::buildFromKColorScheme(m_kdeMgr);
    else if (m_themes.contains(m_activeName))
        m_currentTheme = m_themes[m_activeName];
    else
        m_currentTheme = Markoff::Theme::defaultLight();
    emit themeChanged(m_currentTheme);
}

} // namespace Corbomite::Core

#endif // 0 — disabled pending theme port

// Minimal stubs OUTSIDE the #if 0 so downstream linkers (vault, app, etc.)
// can still link. TODO(port-foundation-exploration): replace with real impl
// when the theme port lands.
#include "corbomite/core/ThemeService.h"

#include <QApplication>
#include <QPalette>

namespace Corbomite::Core {
ThemeService::ThemeService(KColorSchemeManager *, QObject *parent)
    : QObject(parent) {}
ThemeService::~ThemeService() = default;
// Return a POPULATED default theme, not an empty Theme{}. An empty theme
// leaves every Slot unset, so Theme::color() falls back to TextDefault (a
// dark color) for the search-highlight backgrounds — and the C++
// InlineHighlighter reads those Slots directly (no QML fallback), rendering
// Live find-highlights as a black block. Pick light/dark from the app
// palette so we roughly follow the system until the full theme port lands.
// NOTE: Markoff::Theme::defaultDark() does not yet populate the search-
// highlight slots (queue #14, dark half) — dark mode highlights fall back to
// the (light) text color rather than black; tracked as a follow-up.
Markoff::Theme ThemeService::currentTheme() const {
    const QColor window = QApplication::palette().color(QPalette::Window);
    return window.lightness() < 128 ? Markoff::Theme::defaultDark()
                                    : Markoff::Theme::defaultLight();
}
QString      ThemeService::activeThemeName() const { return {}; }
QStringList  ThemeService::availableThemeNames() const { return {}; }
void         ThemeService::setActiveThemeByName(const QString &) {}
void         ThemeService::addUserTheme(const Markoff::Theme &) {}
void         ThemeService::refreshSystemTheme() {}
} // namespace Corbomite::Core
