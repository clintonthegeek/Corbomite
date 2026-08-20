// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KPageDialog>

#include <QList>
#include <QPair>
#include <QString>

class KActionCollection;

namespace Corbomite {

class PluginManager;

namespace Core {
class ThemeService;
}

class SettingsDialog : public KPageDialog {
    Q_OBJECT

public:
    /// Cluster O Phase O3 (O3.T2) — the Hotkeys page must show every
    /// `ViewActions` provider's shortcuts, not just the universal
    /// collection, even when no tab of that type is currently open
    /// (providers are eagerly constructed but only dynamically
    /// installed — KShortcutsEditor can't discover an uninstalled
    /// client's collection any other way). Each entry is one
    /// `KActionCollection` plus the section title `KShortcutsEditor`
    /// should show for it (empty string uses the collection's own
    /// default title).
    using ActionCollections = QList<QPair<KActionCollection *, QString>>;

    explicit SettingsDialog(PluginManager *plugins = nullptr,
                            Core::ThemeService *themeService = nullptr,
                            const ActionCollections &actionCollections = {},
                            QWidget *parent = nullptr);

private:
    void setupEditorPage();
    void setupFilesPage();
    void setupAppearancePage();
    void setupDailyNotesPage();
    void setupPluginsPage();
    void setupHotkeysPage();
    void applySettings();

    PluginManager *m_plugins;
    Core::ThemeService *m_themeService;
    ActionCollections m_actionCollections;
};

} // namespace Corbomite
