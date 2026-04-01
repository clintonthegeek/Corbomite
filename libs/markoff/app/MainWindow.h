// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_TESTAPP_MAINWINDOW_H
#define MARKOFF_TESTAPP_MAINWINDOW_H
#include <QMainWindow>
#include <memory>
namespace Markoff { class Editor; class ReadingView; }
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void openFile(const QString &path);
private Q_SLOTS:
    void onOpen();
    void onSave();
    void onTextChanged();
private:
    void updateTitle();
    Markoff::Editor *m_editor = nullptr;
    Markoff::ReadingView *m_readingView = nullptr;
    QString m_filePath;
};
#endif
