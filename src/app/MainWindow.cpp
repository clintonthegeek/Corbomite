// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include <KActionCollection>
#include <KLocalizedString>
#include <KStandardAction>

#include <QApplication>
#include <QLabel>
#include <QStatusBar>
#include <QVBoxLayout>

namespace Corbomite {

MainWindow::MainWindow(QWidget *parent)
    : CorbomiteMDI::MainWindow(parent)
{
#ifdef CORBOMITE_DEV_BUILD
    setObjectName(QStringLiteral("CorbomiteMainWindowDev"));
    setComponentName(QStringLiteral("corbomite-dev"), i18n("Corbomite [Dev]"));
#else
    setObjectName(QStringLiteral("CorbomiteMainWindow"));
    setComponentName(QStringLiteral("corbomite"), i18n("Corbomite"));
#endif

    resize(1200, 800);

    setupActions();
    setupCentralWidget();
    setupSidebars();
    setupStatusBar();

    setupGUI(ToolBar | Keys | StatusBar | Save);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupActions()
{
    KStandardAction::quit(qApp, &QApplication::quit, actionCollection());
    KStandardAction::preferences(this, []() {
        // Preferences dialog will be implemented later
    }, actionCollection());

    // File > New Note (Ctrl+N)
    auto *newNoteAction = actionCollection()->addAction(QStringLiteral("file_new_note"));
    newNoteAction->setText(i18n("New Note"));
    newNoteAction->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    actionCollection()->setDefaultShortcut(newNoteAction, Qt::CTRL | Qt::Key_N);
    connect(newNoteAction, &QAction::triggered, this, &MainWindow::createNewNote);

    // File > Save (Ctrl+S)
    auto *saveAction = actionCollection()->addAction(QStringLiteral("file_save"));
    saveAction->setText(i18n("Save"));
    saveAction->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    actionCollection()->setDefaultShortcut(saveAction, Qt::CTRL | Qt::Key_S);

    // View > Toggle Left Sidebar (Ctrl+\)
    auto *toggleLeftAction = actionCollection()->addAction(QStringLiteral("view_toggle_left_sidebar"));
    toggleLeftAction->setText(i18n("Toggle Left Sidebar"));
    actionCollection()->setDefaultShortcut(toggleLeftAction, Qt::CTRL | Qt::Key_Backslash);
    connect(toggleLeftAction, &QAction::triggered, this, [this]() {
        auto *tv = activeViewToolView(KMultiTabBar::Left);
        if (tv) {
            hideToolView(tv);
        } else {
            // Show the first toolview in the left sidebar if one exists
            auto names = toolviewNames();
            for (const auto &name : names) {
                auto *view = toolView(name);
                if (view && toolViewPosition(view) == CorbomiteMDI::Left) {
                    showToolView(view);
                    break;
                }
            }
        }
    });

    // View > Zoom In (Ctrl++)
    auto *zoomInAction = actionCollection()->addAction(QStringLiteral("view_zoom_in"));
    zoomInAction->setText(i18n("Zoom In"));
    zoomInAction->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    actionCollection()->setDefaultShortcut(zoomInAction, Qt::CTRL | Qt::Key_Plus);

    // View > Zoom Out (Ctrl+-)
    auto *zoomOutAction = actionCollection()->addAction(QStringLiteral("view_zoom_out"));
    zoomOutAction->setText(i18n("Zoom Out"));
    zoomOutAction->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    actionCollection()->setDefaultShortcut(zoomOutAction, Qt::CTRL | Qt::Key_Minus);

    // View > Zoom Reset (Ctrl+0)
    auto *zoomResetAction = actionCollection()->addAction(QStringLiteral("view_zoom_reset"));
    zoomResetAction->setText(i18n("Reset Zoom"));
    zoomResetAction->setIcon(QIcon::fromTheme(QStringLiteral("zoom-original")));
    actionCollection()->setDefaultShortcut(zoomResetAction, Qt::CTRL | Qt::Key_0);
}

void MainWindow::setupCentralWidget()
{
    m_editorArea = new QWidget(centralWidget());
    auto *layout = new QVBoxLayout(m_editorArea);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *placeholder = new QLabel(i18n("Open a vault to begin"), m_editorArea);
    placeholder->setAlignment(Qt::AlignCenter);
    auto font = placeholder->font();
    font.setPointSize(16);
    placeholder->setFont(font);
    placeholder->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(placeholder);

    centralWidget()->layout()->addWidget(m_editorArea);
}

void MainWindow::setupSidebars()
{
    // Create a "Files" toolview in the left sidebar
    auto *filesToolView = createToolView(nullptr,
                                          QStringLiteral("files_panel"),
                                          KMultiTabBar::Left,
                                          QIcon::fromTheme(QStringLiteral("folder")),
                                          i18n("Files"));

    auto *filesPlaceholder = new QLabel(i18n("No vault open"), filesToolView);
    filesPlaceholder->setAlignment(Qt::AlignCenter);
    filesPlaceholder->setMargin(20);
}

void MainWindow::setupStatusBar()
{
    m_wordCountLabel = new QLabel(i18n("Words: 0"), this);
    m_wordCountLabel->setContentsMargins(8, 0, 8, 0);
    statusBar()->addWidget(m_wordCountLabel);

    m_cursorPosLabel = new QLabel(i18n("Ln 1, Col 1"), this);
    m_cursorPosLabel->setContentsMargins(8, 0, 8, 0);
    statusBar()->addPermanentWidget(m_cursorPosLabel);
}

void MainWindow::createNewNote()
{
    // Will be implemented when editor integration is added
}

} // namespace Corbomite
