// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "VaultService.h"
#include "editor/EditorViewManager.h"
#include "editor/EditorViewSpace.h"
#include "editor/NoteEditorWidget.h"
#include "editor/NotePreviewWidget.h"
#include "sidebar/FileExplorerPanel.h"
#include "sidebar/SearchPanel.h"
#include "sidebar/BacklinksPanel.h"
#include "sidebar/OutlinksPanel.h"
#include "sidebar/OutlinePanel.h"
#include "graph/LocalGraphPanel.h"
#include "graph/GraphViewTab.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/core/NoteDocument.h"
#include "reactors/AutosaveReactor.h"
#include "reactors/FileWatchReactor.h"
#include "SessionManager.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/QuickSwitcher.h"

#include <KCommandBar>
#include <KLocalizedString>
#include <KStandardAction>
#include <KActionCollection>
#include <KMessageBox>
#include <KStandardGuiItem>
#include <QApplication>
#include <QFileDialog>
#include <QLabel>
#include <QStatusBar>
#include <QInputDialog>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextCursor>

namespace Corbomite {

MainWindow::MainWindow(VaultService *vaultService, QWidget *parent)
    : CorbomiteMDI::MainWindow(parent)
    , m_vaultService(vaultService)
{
#ifdef CORBOMITE_DEV_BUILD
    setObjectName(QStringLiteral("CorbomiteMainWindowDev"));
    setComponentName(QStringLiteral("corbomite-dev"), i18n("Corbomite [Dev]"));
#else
    setObjectName(QStringLiteral("CorbomiteMainWindow"));
    setComponentName(QStringLiteral("corbomite"), i18n("Corbomite"));
#endif

    setupActions();
    setupEditor();
    setupSidebars();
    setupStatusBar();

#ifdef CORBOMITE_DEV_BUILD
    setupGUI(Default, QStringLiteral("corbomite-devui.rc"));
#else
    setupGUI(Default, QStringLiteral("corbomiteui.rc"));
#endif

    connect(m_vaultService, &VaultService::vaultOpened, this, &MainWindow::onVaultOpened);
    connect(m_vaultService, &VaultService::vaultClosed, this, &MainWindow::onVaultClosed);

    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    // Clean up vault-related objects before base class destroys the widget tree.
    // Order matters: reactors first (they reference services/models), then models.
    delete m_autosave;
    m_autosave = nullptr;

    if (m_fileWatch) {
        m_fileWatch->stopWatching();
    }
    delete m_fileWatch;
    m_fileWatch = nullptr;

    delete m_sessionManager;
    m_sessionManager = nullptr;

    delete m_treeModel;
    m_treeModel = nullptr;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_sessionManager) {
        // Save window geometry
        m_sessionManager->saveWindowGeometry(saveGeometry(), saveState());

        // Save sidebar state
        m_sessionManager->saveSidebarState(
            sidebarsVisible(), 200, // leftVisible, leftWidth (default)
            false, 200);            // rightVisible, rightWidth (default)

        // Save open tabs
        auto *viewSpace = m_editorManager->activeViewSpace();
        if (viewSpace) {
            auto *tabs = viewSpace->tabModel();
            QJsonArray tabArray;
            for (int i = 0; i < tabs->rowCount(); ++i) {
                QJsonObject tab;
                tab[QStringLiteral("path")] = tabs->tabPath(i);
                tabArray.append(tab);
            }
            m_sessionManager->saveOpenTabs(tabArray, tabs->activeTabIndex());
        }

        // Write session data immediately (bypass debounce timer)
        m_sessionManager->saveNow();
    }

    CorbomiteMDI::MainWindow::closeEvent(event);
}

void MainWindow::setupActions()
{
    KActionCollection *ac = actionCollection();

    KStandardAction::quit(qApp, &QApplication::quit, ac);
    KStandardAction::preferences(this, [this]() {
        SettingsDialog dialog(this);
        dialog.exec();
    }, ac);

    // File actions
    auto *openVault = ac->addAction(QStringLiteral("file_open_vault"));
    openVault->setText(i18n("Open Vault..."));
    openVault->setIcon(QIcon::fromTheme(QStringLiteral("folder-open")));
    ac->setDefaultShortcut(openVault, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(openVault, &QAction::triggered, this, &MainWindow::openVaultDialog);

    auto *newNote = ac->addAction(QStringLiteral("file_new_note"));
    newNote->setText(i18n("New Note"));
    newNote->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    ac->setDefaultShortcut(newNote, QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(newNote, &QAction::triggered, this, &MainWindow::createNewNote);

    auto *save = ac->addAction(QStringLiteral("file_save"));
    save->setText(i18n("Save"));
    save->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    ac->setDefaultShortcut(save, QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(save, &QAction::triggered, this, &MainWindow::saveCurrentNote);

    // View actions
    auto *toggleLeft = ac->addAction(QStringLiteral("view_toggle_left_sidebar"));
    toggleLeft->setText(i18n("Toggle Left Sidebar"));
    ac->setDefaultShortcut(toggleLeft, QKeySequence(Qt::CTRL | Qt::Key_Backslash));
    connect(toggleLeft, &QAction::triggered, this, [this]() {
        setSidebarsVisible(!sidebarsVisible());
    });

    auto *zoomIn = ac->addAction(QStringLiteral("view_zoom_in"));
    zoomIn->setText(i18n("Zoom In"));
    ac->setDefaultShortcut(zoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal));

    auto *zoomOut = ac->addAction(QStringLiteral("view_zoom_out"));
    zoomOut->setText(i18n("Zoom Out"));
    ac->setDefaultShortcut(zoomOut, QKeySequence(Qt::CTRL | Qt::Key_Minus));

    auto *zoomReset = ac->addAction(QStringLiteral("view_zoom_reset"));
    zoomReset->setText(i18n("Reset Zoom"));
    ac->setDefaultShortcut(zoomReset, QKeySequence(Qt::CTRL | Qt::Key_0));

    // Navigation actions
    auto *quickSwitcher = ac->addAction(QStringLiteral("quick_switcher"));
    quickSwitcher->setText(i18n("Quick Switcher"));
    quickSwitcher->setIcon(QIcon::fromTheme(QStringLiteral("quickopen")));
    ac->setDefaultShortcut(quickSwitcher, QKeySequence(Qt::CTRL | Qt::Key_O));
    connect(quickSwitcher, &QAction::triggered, this, &MainWindow::showQuickSwitcher);

    auto *commandPalette = ac->addAction(QStringLiteral("command_palette"));
    commandPalette->setText(i18n("Command Palette"));
    commandPalette->setIcon(QIcon::fromTheme(QStringLiteral("system-run")));
    ac->setDefaultShortcut(commandPalette, QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(commandPalette, &QAction::triggered, this, &MainWindow::showCommandPalette);

    auto *searchVault = ac->addAction(QStringLiteral("search_vault"));
    searchVault->setText(i18n("Search Vault"));
    searchVault->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    ac->setDefaultShortcut(searchVault, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    connect(searchVault, &QAction::triggered, this, &MainWindow::showSearchPanel);

    auto *graphView = ac->addAction(QStringLiteral("graph_view"));
    graphView->setText(i18n("Graph View"));
    graphView->setIcon(QIcon::fromTheme(QStringLiteral("preferences-system-network")));
    ac->setDefaultShortcut(graphView, QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(graphView, &QAction::triggered, this, &MainWindow::openGraphView);

    auto *toggleMode = ac->addAction(QStringLiteral("editor_toggle_mode"));
    toggleMode->setText(i18n("Toggle Reading Mode"));
    toggleMode->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
    ac->setDefaultShortcut(toggleMode, QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(toggleMode, &QAction::triggered, this, &MainWindow::toggleEditorMode);
}

void MainWindow::setupEditor()
{
    m_editorManager = new EditorViewManager(centralWidget());
    centralWidget()->layout()->addWidget(m_editorManager);

    connect(m_editorManager, &EditorViewManager::cursorInfoChanged,
            this, &MainWindow::onCursorInfoChanged);
}

void MainWindow::setupSidebars()
{
    // File Explorer in left sidebar
    auto *toolView = createToolView(
        nullptr,
        QStringLiteral("files_panel"),
        KMultiTabBar::Left,
        QIcon::fromTheme(QStringLiteral("folder")),
        i18n("Files")
    );

    m_fileExplorer = new FileExplorerPanel(toolView);
    // ToolView already has a QVBoxLayout from CorbomiteMDI — use it
    toolView->layout()->addWidget(m_fileExplorer);

    connect(m_fileExplorer, &FileExplorerPanel::noteActivated,
            this, &MainWindow::onNoteActivated);
    connect(m_fileExplorer, &FileExplorerPanel::newNoteRequested,
            this, [this](const QString &folder) {
        bool ok;
        QString name = QInputDialog::getText(this, i18n("New Note"),
                                              i18n("Note name:"), QLineEdit::Normal,
                                              QString(), &ok);
        if (ok && !name.isEmpty()) {
            auto *doc = m_vaultService->noteService()->createNote(name, folder);
            if (doc) {
                m_editorManager->openNote(doc);
            }
        }
    });
    connect(m_fileExplorer, &FileExplorerPanel::deleteNoteRequested,
            this, [this](const QString &path) {
        auto result = KMessageBox::questionTwoActions(
            this,
            i18n("Delete \"%1\"?", path),
            i18n("Delete Note"),
            KStandardGuiItem::del(),
            KStandardGuiItem::cancel()
        );
        if (result == KMessageBox::PrimaryAction) {
            m_vaultService->noteService()->deleteNote(path);
        }
    });

    // Search panel in left sidebar
    auto *searchToolView = createToolView(
        nullptr,
        QStringLiteral("search_panel"),
        KMultiTabBar::Left,
        QIcon::fromTheme(QStringLiteral("edit-find")),
        i18n("Search")
    );

    m_searchPanel = new SearchPanel(searchToolView);
    searchToolView->layout()->addWidget(m_searchPanel);

    connect(m_searchPanel, &SearchPanel::noteActivated,
            this, &MainWindow::onNoteActivated);

    // Right sidebar: Backlinks
    auto *backlinksView = createToolView(
        nullptr,
        QStringLiteral("backlinks_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("link")),
        i18n("Backlinks")
    );
    m_backlinksPanel = new BacklinksPanel(backlinksView);
    backlinksView->layout()->addWidget(m_backlinksPanel);
    connect(m_backlinksPanel, &BacklinksPanel::noteActivated,
            this, &MainWindow::onNoteActivated);

    // Right sidebar: Outlinks
    auto *outlinksView = createToolView(
        nullptr,
        QStringLiteral("outlinks_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("go-jump")),
        i18n("Outlinks")
    );
    m_outlinksPanel = new OutlinksPanel(outlinksView);
    outlinksView->layout()->addWidget(m_outlinksPanel);
    connect(m_outlinksPanel, &OutlinksPanel::noteActivated,
            this, &MainWindow::onNoteActivated);
    connect(m_outlinksPanel, &OutlinksPanel::createNoteRequested,
            this, [this](const QString &name) {
        auto *doc = m_vaultService->noteService()->createNote(name, QString());
        if (doc) m_editorManager->openNote(doc);
    });

    // Right sidebar: Outline
    auto *outlineView = createToolView(
        nullptr,
        QStringLiteral("outline_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("view-list-tree")),
        i18n("Outline")
    );
    m_outlinePanel = new OutlinePanel(outlineView);
    outlineView->layout()->addWidget(m_outlinePanel);
    connect(m_outlinePanel, &OutlinePanel::scrollToLine,
            this, [this](int lineNumber) {
        auto *editor = m_editorManager->activeEditor();
        if (!editor) return;
        QTextCursor cursor(editor->document()->findBlockByLineNumber(lineNumber - 1));
        editor->setTextCursor(cursor);
        editor->centerCursor();
    });

    // Right sidebar: Local Graph
    auto *localGraphView = createToolView(
        nullptr,
        QStringLiteral("local_graph_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("preferences-system-network")),
        i18n("Local Graph")
    );
    m_localGraphPanel = new LocalGraphPanel(localGraphView);
    localGraphView->layout()->addWidget(m_localGraphPanel);
    connect(m_localGraphPanel, &LocalGraphPanel::noteActivated,
            this, &MainWindow::onNoteActivated);
}

void MainWindow::setupStatusBar()
{
    m_wordCountLabel = new QLabel(i18n("Words: 0"), this);
    m_cursorPosLabel = new QLabel(i18n("Ln 1, Col 1"), this);

    statusBar()->addPermanentWidget(m_wordCountLabel);
    statusBar()->addPermanentWidget(m_cursorPosLabel);
}

void MainWindow::openVaultDialog()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, i18n("Open Vault"), QDir::homePath());
    if (!dir.isEmpty()) {
        m_vaultService->openVault(dir);
    }
}

void MainWindow::createNewNote()
{
    if (!m_vaultService->isOpen()) {
        openVaultDialog();
        if (!m_vaultService->isOpen()) return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, i18n("New Note"),
                                          i18n("Note name:"), QLineEdit::Normal,
                                          QString(), &ok);
    if (ok && !name.isEmpty()) {
        auto *doc = m_vaultService->noteService()->createNote(name, QString());
        if (doc) {
            m_editorManager->openNote(doc);
        }
    }
}

void MainWindow::saveCurrentNote()
{
    auto *editor = m_editorManager->activeEditor();
    if (editor && editor->noteDocument()) {
        m_vaultService->noteService()->saveNote(editor->noteDocument());
    }
}

void MainWindow::showCommandPalette()
{
    auto *bar = new KCommandBar(this);

    QList<KCommandBar::ActionGroup> groups;

    // Collect actions from our KActionCollection, grouped by prefix
    KActionCollection *ac = actionCollection();
    QList<QAction *> fileActions, viewActions, editActions;

    for (QAction *action : ac->actions()) {
        QString name = action->objectName();
        if (name.startsWith(QStringLiteral("file_"))) {
            fileActions.append(action);
        } else if (name.startsWith(QStringLiteral("view_"))) {
            viewActions.append(action);
        } else {
            editActions.append(action);
        }
    }

    if (!fileActions.isEmpty())
        groups.append({i18n("File"), fileActions});
    if (!viewActions.isEmpty())
        groups.append({i18n("View"), viewActions});
    if (!editActions.isEmpty())
        groups.append({i18n("Other"), editActions});

    bar->setActions(groups);
    bar->show();
}

void MainWindow::showQuickSwitcher()
{
    if (!m_vaultService->isOpen()) return;

    QStringList recent;
    auto *viewSpace = m_editorManager->activeViewSpace();
    if (viewSpace) {
        recent = viewSpace->tabModel()->lruSortedPaths();
    }

    auto *switcher = new QuickSwitcher(m_vaultService->vault(), recent, this);

    // Position at top-center of window
    QPoint topCenter = mapToGlobal(QPoint(width() / 2 - 300, 80));
    switcher->move(topCenter);

    connect(switcher, &QuickSwitcher::noteSelected,
            this, &MainWindow::onNoteActivated);
    connect(switcher, &QuickSwitcher::createNoteRequested,
            this, [this](const QString &name) {
        auto *doc = m_vaultService->noteService()->createNote(name, QString());
        if (doc) m_editorManager->openNote(doc);
    });

    switcher->show();
}

void MainWindow::onNoteActivated(const QString &relativePath)
{
    auto *doc = m_vaultService->noteService()->openNote(relativePath);
    if (doc) {
        m_editorManager->openNote(doc);
    }
}

void MainWindow::onVaultOpened()
{
    auto *vault = m_vaultService->vault();
    setWindowTitle(vault->name() +
#ifdef CORBOMITE_DEV_BUILD
        QStringLiteral(" — Corbomite [Dev]"));
#else
        QStringLiteral(" — Corbomite"));
#endif

    delete m_treeModel;
    m_treeModel = new NotesTreeModel(vault, this);
    m_fileExplorer->setModel(m_treeModel);

    // Create autosave reactor
    delete m_autosave;
    m_autosave = new AutosaveReactor(m_vaultService->noteService(), this);

    // Create file watch reactor
    delete m_fileWatch;
    m_fileWatch = new FileWatchReactor(vault, this);
    m_fileWatch->startWatching(vault->path());

    // Connect autosave's noteSaved to file watcher suppression
    connect(m_autosave, &AutosaveReactor::noteSaved, this, [this](const QString &relativePath) {
        if (m_fileWatch && m_vaultService->vault()) {
            QString absPath = m_vaultService->vault()->path() + QLatin1Char('/') + relativePath;
            m_fileWatch->suppressPath(absPath);
        }
    });

    // Watch documents as they are opened in the editor
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        if (editor && editor->noteDocument() && m_autosave) {
            m_autosave->watchDocument(editor->noteDocument());
        }
    });

    // Set vault on editors and connect link navigation
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        if (!editor) return;
        if (m_vaultService->vault()) {
            editor->setVaultModel(m_vaultService->vault());
        }
        // Connect link navigation (UniqueConnection works here — pointer-to-member)
        connect(editor, &NoteEditorWidget::linkActivated,
                this, &MainWindow::onNoteActivated, Qt::UniqueConnection);
    });

    // Create search index
    delete m_searchIndex;
    m_searchIndex = new SQLiteIndex(this);
    m_searchIndex->open(vault->configPath() + QStringLiteral("/index.sqlite"));
    m_searchIndex->rebuildIndex(vault->path());
    // TODO: Move indexing to background thread for large vaults
    m_searchPanel->setIndex(m_searchIndex);
    m_vaultService->noteService()->setSearchIndex(m_searchIndex);
    m_vaultService->vault()->setSearchIndex(m_searchIndex);

    // Set index on sidebar panels
    m_backlinksPanel->setIndex(m_searchIndex);
    m_outlinksPanel->setIndex(m_searchIndex);
    m_outlinksPanel->setVaultModel(vault);
    m_localGraphPanel->setIndex(m_searchIndex);
    m_localGraphPanel->setVaultModel(vault);

    // Update sidebar panels when active note changes
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        if (editor && editor->noteDocument()) {
            m_backlinksPanel->setCurrentNote(editor->noteDocument());
            m_outlinksPanel->setCurrentNote(editor->noteDocument());
            m_outlinePanel->setCurrentNote(editor->noteDocument());
            m_localGraphPanel->setCurrentNote(editor->noteDocument());
        } else {
            m_backlinksPanel->setCurrentNote(nullptr);
            m_outlinksPanel->setCurrentNote(nullptr);
            m_outlinePanel->setCurrentNote(nullptr);
            m_localGraphPanel->setCurrentNote(nullptr);
        }
    });

    // Update index on note saves
    connect(m_autosave, &AutosaveReactor::noteSaved, this, [this](const QString &relPath) {
        if (!m_searchIndex || !m_vaultService->vault()) return;
        auto *doc = m_vaultService->vault()->cachedDocument(relPath);
        if (doc) {
            m_searchIndex->indexNote(relPath, doc->name(), doc->markdown());
        }
    });

    // Connect internal link navigation from preview widgets
    connect(m_editorManager->activeViewSpace(), &EditorViewSpace::internalLinkClicked,
            this, &MainWindow::onNoteActivated, Qt::UniqueConnection);

    // Connect graph view note navigation
    connect(m_editorManager, &EditorViewManager::graphNoteActivated,
            this, &MainWindow::onNoteActivated, Qt::UniqueConnection);

    // Create session manager and restore session
    delete m_sessionManager;
    m_sessionManager = new SessionManager(this);
    m_sessionManager->setSessionPath(vault->configPath() + QStringLiteral("/session.json"));

    auto session = m_sessionManager->load();
    if (session.contains(QStringLiteral("tabs"))) {
        auto tabs = session[QStringLiteral("tabs")].toArray();
        for (const auto &tabVal : tabs) {
            auto tab = tabVal.toObject();
            QString path = tab[QStringLiteral("path")].toString();
            if (!path.isEmpty()) {
                onNoteActivated(path);
            }
        }
    }
}

void MainWindow::onVaultClosed()
{
    delete m_autosave;
    m_autosave = nullptr;
    if (m_fileWatch) {
        m_fileWatch->stopWatching();
    }
    delete m_fileWatch;
    m_fileWatch = nullptr;
    delete m_sessionManager;
    m_sessionManager = nullptr;

    m_vaultService->noteService()->setSearchIndex(nullptr);
    m_vaultService->vault()->setSearchIndex(nullptr);

    m_backlinksPanel->setIndex(nullptr);
    m_backlinksPanel->setCurrentNote(nullptr);
    m_outlinksPanel->setIndex(nullptr);
    m_outlinksPanel->setVaultModel(nullptr);
    m_outlinksPanel->setCurrentNote(nullptr);
    m_outlinePanel->setCurrentNote(nullptr);
    m_localGraphPanel->setIndex(nullptr);
    m_localGraphPanel->setVaultModel(nullptr);
    m_localGraphPanel->setCurrentNote(nullptr);

    delete m_searchIndex;
    m_searchIndex = nullptr;
    m_searchPanel->setIndex(nullptr);

    delete m_treeModel;
    m_treeModel = nullptr;
    m_fileExplorer->setModel(nullptr);
    setWindowTitle(
#ifdef CORBOMITE_DEV_BUILD
        QStringLiteral("Corbomite [Dev]"));
#else
        QStringLiteral("Corbomite"));
#endif
}

void MainWindow::openGraphView()
{
    if (!m_vaultService->isOpen() || !m_searchIndex) return;
    m_editorManager->openGraphView(m_searchIndex, m_vaultService->vault());
}

void MainWindow::showSearchPanel()
{
    // Show the search toolview and focus its input
    auto *tv = toolView(QStringLiteral("search_panel"));
    if (tv) {
        showToolView(tv);
        m_searchPanel->focusSearchInput();
    }
}

void MainWindow::toggleEditorMode()
{
    m_editorManager->toggleEditorMode();
    // Update status bar
    if (m_editorManager->isPreviewMode()) {
        m_cursorPosLabel->setText(i18n("Reading"));
    }
}

void MainWindow::onCursorInfoChanged(int line, int column, int wordCount)
{
    m_wordCountLabel->setText(i18n("Words: %1", wordCount));
    m_cursorPosLabel->setText(i18n("Ln %1, Col %2", line, column));
}

} // namespace Corbomite
