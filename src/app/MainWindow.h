// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mdi/CorbomiteMDI.h"
#include <QLabel>

namespace Corbomite {

class VaultService;
class EditorViewManager;
class FileExplorerPanel;
class NotesTreeModel;

class MainWindow : public CorbomiteMDI::MainWindow {
    Q_OBJECT

public:
    explicit MainWindow(VaultService *vaultService, QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupActions();
    void setupSidebars();
    void setupStatusBar();
    void setupEditor();

    // Action handlers
    void openVaultDialog();
    void createNewNote();
    void saveCurrentNote();
    void onNoteActivated(const QString &relativePath);
    void onVaultOpened();
    void onVaultClosed();
    void onCursorInfoChanged(int line, int column, int wordCount);

    VaultService *m_vaultService;
    EditorViewManager *m_editorManager = nullptr;
    FileExplorerPanel *m_fileExplorer = nullptr;
    NotesTreeModel *m_treeModel = nullptr;
    QLabel *m_wordCountLabel = nullptr;
    QLabel *m_cursorPosLabel = nullptr;
};

} // namespace Corbomite
