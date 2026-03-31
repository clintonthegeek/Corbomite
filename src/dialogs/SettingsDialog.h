// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KPageDialog>

namespace Corbomite {

class SettingsDialog : public KPageDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private:
    void setupEditorPage();
    void setupFilesPage();
    void setupAppearancePage();
    void applySettings();
};

} // namespace Corbomite
