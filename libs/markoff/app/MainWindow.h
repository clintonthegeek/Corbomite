// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_TESTAPP_MAINWINDOW_H
#define MARKOFF_TESTAPP_MAINWINDOW_H
#include <QMainWindow>

class QLabel;
class QTreeWidget;
class QAction;

namespace Markoff { class Editor; }

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void openFile(const QString &path);

private Q_SLOTS:
    void onOpen();
    void onSave();
    void onToggleReadOnly();
    void onToggleTheme();
    void onToggleSidebar();

private:
    void updateTitle();
    void updateStatusBar();
    void updateMetadata();

    Markoff::Editor *m_editor = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTreeWidget *m_metadataTree = nullptr;
    QAction *m_readOnlyAction = nullptr;
    QAction *m_themeAction = nullptr;
    QAction *m_sidebarAction = nullptr;
    QDockWidget *m_sidebarDock = nullptr;
    QString m_filePath;
    bool m_darkTheme = false;
};
#endif
