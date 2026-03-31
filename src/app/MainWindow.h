// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "CorbomiteMDI.h"

#include <QLabel>

namespace Corbomite {

class MainWindow : public CorbomiteMDI::MainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupActions();
    void setupSidebars();
    void setupStatusBar();
    void setupCentralWidget();
    void createNewNote();

    QWidget *m_editorArea = nullptr;
    QLabel *m_wordCountLabel = nullptr;
    QLabel *m_cursorPosLabel = nullptr;
};

} // namespace Corbomite
