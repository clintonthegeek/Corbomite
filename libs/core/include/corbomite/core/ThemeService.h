// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

#ifndef CORBOMITE_CORE_THEMESERVICE_H
#define CORBOMITE_CORE_THEMESERVICE_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <markoff/core/Theme.h>

class KColorSchemeManager;

namespace Corbomite::Core {

/// Owns the active Markoff theme + the registry of installed themes (six
/// built-ins shipped in markoff-core's Qt resources + any user themes the
/// user dropped into ${XDG_CONFIG_HOME}/corbomite[-dev]/themes/*.json).
///
/// Active theme is "Follow system" by default — built via SystemThemeBuilder
/// from the current KColorScheme. setActiveThemeByName drives the rebuild +
/// emits themeChanged whenever the active theme actually changes.
class ThemeService : public QObject {
    Q_OBJECT
public:
    explicit ThemeService(KColorSchemeManager *kdeMgr, QObject *parent = nullptr);
    ~ThemeService() override;

    Markoff::Theme currentTheme() const;
    QString activeThemeName() const;
    QStringList availableThemeNames() const;
    QString userThemesDirectory() const;

public Q_SLOTS:
    void setActiveThemeByName(const QString &name);
    void addUserTheme(const Markoff::Theme &theme);
    void refreshSystemTheme();  // Called on KDE color-scheme change.

Q_SIGNALS:
    void themeChanged(const Markoff::Theme &theme);

private:
    void loadBuiltInThemes();
    void loadUserThemes();
    void rebuildAndEmit();

    KColorSchemeManager *m_kdeMgr;
    QString m_activeName;
    Markoff::Theme m_currentTheme;
    QHash<QString, Markoff::Theme> m_themes;  // by name (excludes "Follow system")
};

} // namespace Corbomite::Core

#endif // CORBOMITE_CORE_THEMESERVICE_H
