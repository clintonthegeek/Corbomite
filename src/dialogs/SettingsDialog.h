// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KPageDialog>

class KActionCollection;

namespace Corbomite {

class PluginManager;

namespace Core {
class ThemeService;
}

class SettingsDialog : public KPageDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(PluginManager *plugins = nullptr,
                            Core::ThemeService *themeService = nullptr,
                            KActionCollection *actions = nullptr,
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
    KActionCollection *m_actions;
};

} // namespace Corbomite
