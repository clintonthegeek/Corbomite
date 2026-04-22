// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KPageDialog>

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
                            QWidget *parent = nullptr);

private:
    void setupEditorPage();
    void setupFilesPage();
    void setupAppearancePage();
    void setupDailyNotesPage();
    void setupPluginsPage();
    void applySettings();

    PluginManager *m_plugins;
    Core::ThemeService *m_themeService;
};

} // namespace Corbomite
