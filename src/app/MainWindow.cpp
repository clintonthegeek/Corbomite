// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "WelcomeScreen.h"
#include "VaultService.h"
#include "editor/EditorViewManager.h"
#include "editor/EditorViewSpace.h"
#include "editor/NoteEditorWidget.h"
#include <markoff/Editor.h>
#include "sidebar/FileExplorerPanel.h"
#include "sidebar/SearchPanel.h"
#include "sidebar/BacklinksPanel.h"
#include "sidebar/PropertiesPanel.h"
#include "sidebar/OutlinksPanel.h"
#include "sidebar/OutlinePanel.h"
#include "graph/LocalGraphPanel.h"
#include "graph/GraphControlsPanel.h"
#include "graph/GraphViewTab.h"
#include "canvas/CanvasViewTab.h"
#include <canvas/CanvasDocument.h>
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/VaultConfig.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/core/NoteDocument.h"
#include "reactors/AutosaveReactor.h"
#include "reactors/FileWatchReactor.h"
#include "SessionManager.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/EmbedRegistry.h"
#include "corbomite/core/HoverLinkSourceRegistry.h"
#include "corbomite/core/MenuEventEmitter.h"
#include "corbomite/core/VaultResourceProvider.h"
#include "corbomite/readingview/EmbedRenderer.h"
#include "editor/HoverPopover.h"
#include "editor/TagSuggest.h"
#include "editor/WikiLinkSuggest.h"
#include "dialogs/CreateVaultDialog.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/QuickSwitcher.h"
#include "dialogs/TemplatePicker.h"
#include "RibbonSlot.h"
#include "corbomite/models/TemplateService.h"
#include "corbomite/models/DailyNoteService.h"
#include "corbomitesettings.h"

#include <KCommandBar>
#include <KLocalizedString>
#include <KStandardAction>
#include <KActionCollection>
#include <KMessageBox>
#include <KStandardGuiItem>
#include <KRecentFilesAction>
#include <KSharedConfig>
#include <KConfigGroup>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QStatusBar>
#include <QInputDialog>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextCursor>
#include <QTabBar>

namespace Corbomite {

namespace {

/// Cluster J Phase 6 — vault-scoped `Core::VaultResourceProvider` adapter.
/// HoverPopover doesn't have an "active note" context, so this adapter
/// resolves wikilinks against the vault root only (no per-note relative
/// directory). Image bytes are loaded lazily from disk under the vault path.
class VaultScopedResources : public Corbomite::Core::VaultResourceProvider
{
public:
    explicit VaultScopedResources(VaultModel *vault) : m_vault(vault) {}

    QUrl resolveImage(const QString &name) const override
    {
        if (!m_vault) return {};
        const QString path = m_vault->path() + QLatin1Char('/') + name;
        if (QFileInfo::exists(path)) return QUrl::fromLocalFile(path);
        return {};
    }

    QByteArray loadImageBytes(const QString &name) const override
    {
        if (!m_vault) return {};
        const QString path = m_vault->path() + QLatin1Char('/') + name;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        return f.readAll();
    }

    std::optional<QString> resolveEmbed(const QString &name) const override
    {
        if (!m_vault) return std::nullopt;
        QString rel = name;
        if (!rel.endsWith(QStringLiteral(".md"))
            && !rel.endsWith(QStringLiteral(".canvas"))
            && !rel.contains(QLatin1Char('.'))) {
            rel += QStringLiteral(".md");
        }
        // Prefer cached document text (picks up unsaved edits in the editor).
        if (auto *doc = m_vault->cachedDocument(rel)) {
            return doc->markdown();
        }
        // Fall back to disk read against vault root.
        const QString abs = m_vault->path() + QLatin1Char('/') + rel;
        QFile f(abs);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString::fromUtf8(f.readAll());
        }
        // Shortest-path resolution when the bare name doesn't match the
        // root layout — mirrors `VaultResourceProvider::resolveTarget`.
        const QString filename =
            rel.mid(rel.lastIndexOf(QLatin1Char('/')) + 1);
        const auto allNotes = m_vault->allNotes();
        for (const auto &meta : allNotes) {
            if (meta.relativePath == filename
                || meta.relativePath.endsWith(QLatin1Char('/') + filename)) {
                if (auto *doc = m_vault->cachedDocument(meta.relativePath)) {
                    return doc->markdown();
                }
                QFile alt(m_vault->path() + QLatin1Char('/')
                          + meta.relativePath);
                if (alt.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    return QString::fromUtf8(alt.readAll());
                }
            }
        }
        return std::nullopt;
    }

    QUrl resolveWikiLink(const QString &target) const override
    {
        if (!m_vault) return {};
        return QUrl::fromLocalFile(m_vault->path() + QLatin1Char('/') + target);
    }

    bool wikiLinkExists(const QString &target) const override
    {
        return m_vault ? m_vault->noteExists(target) : false;
    }

private:
    VaultModel *m_vault;
};

} // namespace

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
    setupRibbon();

#ifdef CORBOMITE_DEV_BUILD
    setupGUI(Default, QStringLiteral("corbomite-devui.rc"));
#else
    setupGUI(Default, QStringLiteral("corbomiteui.rc"));
#endif

    connect(m_vaultService, &VaultService::vaultOpened, this, &MainWindow::onVaultOpened);
    connect(m_vaultService, &VaultService::vaultClosed, this, &MainWindow::onVaultClosed);

    m_commandRegistry = new CommandRegistry();
    m_menuEvents = new MenuEventEmitter(this);
    m_hoverSources = new HoverLinkSourceRegistry(this);
    m_hoverSources->registerBuiltins();
    m_hoverPopover = new HoverPopover(this);
    m_hoverPopover->setNoteService(m_vaultService->noteService());

    // Cluster J Phase 6 — construct the embed registry + renderer eagerly
    // so HoverPopover can swap to the rich-render path. Resources are
    // null until a vault opens; built-in factories register up-front
    // because the registry's lifetime spans every vault open/close cycle.
    m_embedRegistry = std::make_unique<Corbomite::Core::EmbedRegistry>();
    m_embedRenderer = std::make_unique<Corbomite::ReadingView::EmbedRenderer>(
        m_embedRegistry.get(), /*cache=*/nullptr, /*resources=*/nullptr);
    Corbomite::ReadingView::registerBuiltinEmbedFactories(*m_embedRegistry,
                                                          *m_embedRenderer);
    m_hoverPopover->setEmbedRenderer(m_embedRenderer.get());

    // Cluster H Phase 3 — built-in EditorSuggesters. Insertion order matters
    // (first-non-null-onTrigger-wins per audit); built-ins go first so they
    // shadow any future plugin overrides of `[[` / `#`.
    m_suggestManager = new EditorSuggestManager(this);
    m_wikiSuggest = new WikiLinkSuggest(m_vaultService->vault());
    m_tagSuggest = new TagSuggest(m_vaultService->vault());
    m_suggestManager->registerSuggest(m_wikiSuggest);
    m_suggestManager->registerSuggest(m_tagSuggest);

    updateVaultActions();
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

    // Cluster J Phase 6 — clear renderer back-pointers before tearing down
    // the cache + popover resources. EmbedRenderer holds raw pointers to
    // both; HoverPopover (owned by `this`) holds a raw pointer to the
    // renderer, so the renderer must outlive the popover. unique_ptr
    // destruction order in the member list runs in reverse declaration
    // order, but tear them down explicitly here to be safe.
    if (m_embedRenderer) {
        m_embedRenderer->setMetadataCache(nullptr);
        m_embedRenderer->setResources(nullptr);
    }
    if (m_hoverPopover) m_hoverPopover->setEmbedRenderer(nullptr);
    m_popoverResources.reset();

    // Close MetadataCache cleanly (flushes persistence) before destroying
    // dependent objects. SQLiteIndex subscribes to its signals, so close
    // the cache first.
    if (m_metadataCache) {
        m_metadataCache->close();
    }
    delete m_searchIndex;
    m_searchIndex = nullptr;
    delete m_metadataCache;
    m_metadataCache = nullptr;
    delete m_linkResolver;
    m_linkResolver = nullptr;

    delete m_treeModel;
    m_treeModel = nullptr;

    delete m_commandRegistry;
    m_commandRegistry = nullptr;

    // Cluster J Phase 6 — embed renderer + registry destroyed last so the
    // earlier popover/registry-cleared-pointer dance can't dangle.
    m_embedRenderer.reset();
    m_embedRegistry.reset();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_vaultService->isOpen()) {
        if (!m_editorManager->queryClose()) {
            event->ignore();
            return;
        }
        if (m_sessionManager) {
            m_sessionManager->saveWindowGeometry(saveGeometry(), saveState());
            m_sessionManager->saveSidebarState(sidebarsVisible(), 200, false, 200);
            m_sessionManager->setPaneLayout(m_editorManager->buildPaneLayout());
            if (m_fileExplorer) {
                m_sessionManager->saveExpandedFolders(m_fileExplorer->expandedFolders());
            }
            m_sessionManager->saveNow();
        }
    }

    CorbomiteMDI::MainWindow::closeEvent(event);
}

void MainWindow::setupActions()
{
    KActionCollection *ac = actionCollection();

    KStandardAction::quit(qApp, &QApplication::quit, ac);
    auto *prefsAction = KStandardAction::preferences(this, [this]() {
        SettingsDialog dialog(this);
        dialog.exec();
    }, ac);
    ac->setDefaultShortcut(prefsAction, QKeySequence(Qt::CTRL | Qt::Key_Comma));

    // File actions
    auto *openVault = ac->addAction(QStringLiteral("file_open_vault"));
    openVault->setText(i18n("Open Vault..."));
    openVault->setIcon(QIcon::fromTheme(QStringLiteral("folder-open")));
    ac->setDefaultShortcut(openVault, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(openVault, &QAction::triggered, this, &MainWindow::openVaultDialog);

    // Recent Vaults
    m_recentVaults = KStandardAction::openRecent(this, [this](const QUrl &url) {
        if (m_vaultService->isOpen()) {
            if (!m_editorManager->queryClose()) return;
        }
        m_vaultService->openVault(url.toLocalFile());
    }, ac);
    m_recentVaults->setObjectName(QStringLiteral("file_open_recent"));

    auto config = KSharedConfig::openConfig();
    KConfigGroup recentGroup = config->group(QStringLiteral("RecentVaults"));
    m_recentVaults->loadEntries(recentGroup);

    auto *closeVaultAction = ac->addAction(QStringLiteral("file_close_vault"));
    closeVaultAction->setText(i18n("Close Vault"));
    closeVaultAction->setIcon(QIcon::fromTheme(QStringLiteral("window-close")));
    connect(closeVaultAction, &QAction::triggered, this, &MainWindow::closeVault);

    auto *newNote = ac->addAction(QStringLiteral("file_new_note"));
    newNote->setText(i18n("New Note"));
    newNote->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    ac->setDefaultShortcut(newNote, QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(newNote, &QAction::triggered, this, &MainWindow::createNewNote);

    auto *newCanvas = ac->addAction(QStringLiteral("file_new_canvas"));
    newCanvas->setText(i18n("New Canvas"));
    newCanvas->setIcon(QIcon::fromTheme(QStringLiteral("draw-rectangle")));
    connect(newCanvas, &QAction::triggered, this, [this]() {
        if (!m_vaultService->isOpen()) return;
        bool ok;
        QString name = QInputDialog::getText(this, i18n("New Canvas"),
                                              i18n("Canvas name:"), QLineEdit::Normal,
                                              QString(), &ok);
        if (ok && !name.isEmpty()) {
            QString relPath = name + QStringLiteral(".canvas");
            QString absPath = m_vaultService->vault()->path() + QLatin1Char('/') + relPath;
            // Create empty canvas file
            Canvas::CanvasDocument emptyDoc;
            emptyDoc.saveToFile(absPath);
            // Open it
            m_editorManager->openCanvas(absPath);
        }
    });

    auto *save = ac->addAction(QStringLiteral("file_save"));
    save->setText(i18n("Save"));
    save->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    ac->setDefaultShortcut(save, QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(save, &QAction::triggered, this, &MainWindow::saveCurrentNote);

    // Edit menu — standard actions (editor handles them, but they need menu visibility)
    KStandardAction::undo(this, [this]() {
        auto *editor = m_editorManager->activeEditor();
        if (editor) editor->editor()->undo();
    }, ac);

    KStandardAction::redo(this, [this]() {
        auto *editor = m_editorManager->activeEditor();
        if (editor) editor->editor()->redo();
    }, ac);

    KStandardAction::find(this, [this]() {
        // TODO: wire up find bar when implemented
        Q_UNUSED(this)
    }, ac);

    // Help menu
    KStandardAction::aboutApp(qApp, []() {}, ac);
    KStandardAction::aboutKDE(qApp, []() {}, ac);

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

    auto *insertTpl = ac->addAction(QStringLiteral("insert_template"));
    insertTpl->setText(i18n("Insert Template"));
    insertTpl->setIcon(QIcon::fromTheme(QStringLiteral("document-new-from-template")));
    ac->setDefaultShortcut(insertTpl, QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(insertTpl, &QAction::triggered, this, &MainWindow::insertTemplate);

    auto *dailyNote = ac->addAction(QStringLiteral("open_daily_note"));
    dailyNote->setText(i18n("Open Daily Note"));
    dailyNote->setIcon(QIcon::fromTheme(QStringLiteral("view-calendar-day")));
    connect(dailyNote, &QAction::triggered, this, &MainWindow::openDailyNote);

    // Cluster E Phase 7 — three-mode selector. "Editing" is Obsidian's live-
    // preview (what most users think of as the default Obsidian editor);
    // "Source" is plain-text markdown; "Reading" is rendered-only.
    auto *editingMode = ac->addAction(QStringLiteral("view_editing_mode"));
    editingMode->setText(i18n("Live Preview"));
    editingMode->setIcon(QIcon::fromTheme(QStringLiteral("text-x-markdown")));
    connect(editingMode, &QAction::triggered, this, [this]() {
        if (auto *editor = m_editorManager->activeEditor())
            editor->setViewMode(NoteEditorWidget::ViewMode::LivePreview);
    });

    auto *sourceMode = ac->addAction(QStringLiteral("view_source_mode"));
    sourceMode->setText(i18n("Source"));
    sourceMode->setIcon(QIcon::fromTheme(QStringLiteral("text-plain")));
    connect(sourceMode, &QAction::triggered, this, [this]() {
        if (auto *editor = m_editorManager->activeEditor())
            editor->setViewMode(NoteEditorWidget::ViewMode::Source);
    });

    auto *readingMode = ac->addAction(QStringLiteral("view_reading_mode"));
    readingMode->setText(i18n("Reading"));
    readingMode->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
    ac->setDefaultShortcut(readingMode, QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(readingMode, &QAction::triggered, this, [this]() {
        if (auto *editor = m_editorManager->activeEditor())
            editor->setViewMode(NoteEditorWidget::ViewMode::Reading);
    });

    // Tab shortcuts
    auto *closeTab = ac->addAction(QStringLiteral("tab_close"));
    closeTab->setText(i18n("Close Tab"));
    closeTab->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    ac->setDefaultShortcut(closeTab, QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(closeTab, &QAction::triggered, this, [this]() {
        auto *viewSpace = m_editorManager->activeViewSpace();
        if (viewSpace) {
            auto *tabBar = viewSpace->findChild<QTabBar *>();
            if (tabBar && tabBar->count() > 0) {
                Q_EMIT tabBar->tabCloseRequested(tabBar->currentIndex());
            }
        }
    });

    auto *nextTab = ac->addAction(QStringLiteral("tab_next"));
    nextTab->setText(i18n("Next Tab"));
    ac->setDefaultShortcut(nextTab, QKeySequence(Qt::CTRL | Qt::Key_Tab));
    connect(nextTab, &QAction::triggered, this, [this]() {
        auto *viewSpace = m_editorManager->activeViewSpace();
        if (viewSpace) {
            auto *tabBar = viewSpace->findChild<QTabBar *>();
            if (tabBar && tabBar->count() > 1) {
                int next = (tabBar->currentIndex() + 1) % tabBar->count();
                tabBar->setCurrentIndex(next);
            }
        }
    });

    auto *prevTab = ac->addAction(QStringLiteral("tab_prev"));
    prevTab->setText(i18n("Previous Tab"));
    ac->setDefaultShortcut(prevTab, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    connect(prevTab, &QAction::triggered, this, [this]() {
        auto *viewSpace = m_editorManager->activeViewSpace();
        if (viewSpace) {
            auto *tabBar = viewSpace->findChild<QTabBar *>();
            if (tabBar && tabBar->count() > 1) {
                int prev = tabBar->currentIndex() - 1;
                if (prev < 0) prev = tabBar->count() - 1;
                tabBar->setCurrentIndex(prev);
            }
        }
    });

    // Split pane shortcuts
    auto *splitRight = ac->addAction(QStringLiteral("split_right"));
    splitRight->setText(i18n("Split Right"));
    ac->setDefaultShortcut(splitRight, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right));
    connect(splitRight, &QAction::triggered, this, [this]() {
        if (m_editorManager) m_editorManager->splitActiveHorizontal();
    });

    auto *splitDown = ac->addAction(QStringLiteral("split_down"));
    splitDown->setText(i18n("Split Down"));
    ac->setDefaultShortcut(splitDown, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down));
    connect(splitDown, &QAction::triggered, this, [this]() {
        if (m_editorManager) m_editorManager->splitActiveVertical();
    });
}

void MainWindow::setupEditor()
{
    // Create the stacked widget inside the MDI central area
    m_centralStack = new QStackedWidget(centralWidget());
    centralWidget()->layout()->addWidget(m_centralStack);

    // Index 0: Welcome screen
    m_welcomeScreen = new WelcomeScreen(m_vaultService, m_centralStack);
    m_centralStack->addWidget(m_welcomeScreen);

    connect(m_welcomeScreen, &WelcomeScreen::vaultRequested, this, [this](const QString &path) {
        if (m_vaultService->isOpen()) {
            if (!m_editorManager->queryClose()) return;
        }
        m_vaultService->openVault(path);
    });
    connect(m_welcomeScreen, &WelcomeScreen::openFolderRequested, this, &MainWindow::openVaultDialog);
    connect(m_welcomeScreen, &WelcomeScreen::createVaultRequested, this, [this]() {
        CreateVaultDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) return;

        QString vaultPath = dialog.vaultPath();
        QDir vaultDir(vaultPath);
        if (!vaultDir.mkpath(QStringLiteral("."))) return;
        vaultDir.mkpath(QStringLiteral(".corbomite"));

        // Create a welcome note
        QFile welcome(vaultPath + QStringLiteral("/Welcome.md"));
        if (welcome.open(QIODevice::WriteOnly | QIODevice::Text)) {
            welcome.write("# Welcome to your vault\n\n"
                          "This is your new knowledge base. Start writing!\n\n"
                          "- Create new notes with **Ctrl+N**\n"
                          "- Link notes together with `[[double brackets]]`\n"
                          "- Search your vault with **Ctrl+Shift+F**\n"
                          "- Toggle reading mode with **Ctrl+E**\n");
        }

        m_vaultService->openVault(vaultPath);
    });

    // Index 1: Editor view manager
    m_editorManager = new EditorViewManager(m_centralStack);
    m_editorManager->setHoverPopover(m_hoverPopover);
    m_editorManager->setEditorSuggestManager(m_suggestManager);
    m_centralStack->addWidget(m_editorManager);

    connect(m_editorManager, &EditorViewManager::cursorInfoChanged,
            this, &MainWindow::onCursorInfoChanged);

    // Start on welcome screen
    m_centralStack->setCurrentIndex(0);
    setSidebarsVisibleInternal(false, true);
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
    m_fileExplorer->setMenuEventEmitter(m_menuEvents);
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
    connect(m_fileExplorer, &FileExplorerPanel::renameNoteRequested,
            this, [this](const QString &path) {
        QString oldName = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
        if (oldName.endsWith(QStringLiteral(".md"))) oldName.chop(3);

        bool ok;
        QString newName = QInputDialog::getText(this, i18n("Rename Note"),
                                                 i18n("New name:"), QLineEdit::Normal,
                                                 oldName, &ok);
        if (ok && !newName.isEmpty() && newName != oldName) {
            QString folder;
            int lastSlash = path.lastIndexOf(QLatin1Char('/'));
            if (lastSlash > 0) folder = path.left(lastSlash);

            QString newPath = folder.isEmpty()
                ? newName + QStringLiteral(".md")
                : folder + QLatin1Char('/') + newName + QStringLiteral(".md");
            m_vaultService->noteService()->renameNote(path, newPath);
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

    // Right sidebar: Properties
    auto *propertiesView = createToolView(
        nullptr,
        QStringLiteral("properties_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("document-properties")),
        i18n("Properties")
    );
    m_propertiesPanel = new PropertiesPanel(propertiesView);
    propertiesView->layout()->addWidget(m_propertiesPanel);

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
        editor->editor()->goToLine(lineNumber);
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

    // Right sidebar: Graph Controls
    auto *graphControlsView = createToolView(
        nullptr,
        QStringLiteral("graph_controls_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("configure")),
        i18n("Graph Controls")
    );
    m_graphControlsPanel = new GraphControlsPanel(graphControlsView);
    graphControlsView->layout()->addWidget(m_graphControlsPanel);
}

void MainWindow::setupStatusBar()
{
    m_wordCountLabel = new QLabel(i18n("Words: 0"), this);
    m_cursorPosLabel = new QLabel(i18n("Ln 1, Col 1"), this);

    statusBar()->addPermanentWidget(m_wordCountLabel);
    statusBar()->addPermanentWidget(m_cursorPosLabel);
}

void MainWindow::setupRibbon()
{
    m_ribbon = new RibbonSlot(this);
    prependToMainHLayout(m_ribbon);

    m_ribbon->addRibbonIcon(QIcon::fromTheme(QStringLiteral("document-new")),
                            i18n("New note"),
                            [this] { createNewNote(); });
    m_ribbon->addRibbonIcon(QIcon::fromTheme(QStringLiteral("quickopen")),
                            i18n("Open quick switcher"),
                            [this] { showQuickSwitcher(); });
    m_ribbon->addRibbonIcon(QIcon::fromTheme(QStringLiteral("preferences-system-network")),
                            i18n("Open graph view"),
                            [this] { openGraphView(); });
}

void MainWindow::openVaultDialog()
{
    if (m_vaultService->isOpen()) {
        if (!m_editorManager->queryClose()) return;
    }

    QString dir = QFileDialog::getExistingDirectory(
        this, i18n("Open Vault"), QDir::homePath());
    if (!dir.isEmpty()) {
        if (!m_vaultService->openVault(dir)) {
            KMessageBox::error(this,
                i18n("Could not open vault at:\n%1\n\nThe directory may not exist or is not readable.", dir),
                i18n("Open Vault Failed"));
        }
    }
}

void MainWindow::closeVault()
{
    if (!m_vaultService->isOpen()) return;

    if (!m_editorManager->queryClose()) return;

    m_vaultService->closeVault();
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
    // Check for canvas tab
    auto *viewSpace = m_editorManager->activeViewSpace();
    if (viewSpace) {
        auto *currentWidget = viewSpace->findChild<QStackedWidget *>()->currentWidget();
        if (auto *canvasTab = qobject_cast<CanvasViewTab *>(currentWidget)) {
            canvasTab->save();
            return;
        }
    }

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

    // Merge CommandRegistry entries into the palette. Each Command is
    // wrapped in a throwaway QAction parented to the KCommandBar; the
    // QAction's triggered signal dispatches back through the registry.
    if (m_commandRegistry) {
        QList<QAction *> commandActions;
        for (auto *cmd : m_commandRegistry->listAvailable()) {
            auto *action = new QAction(cmd->name.isEmpty() ? cmd->id : cmd->name, bar);
            if (!cmd->icon.isEmpty()) action->setIcon(QIcon::fromTheme(cmd->icon));
            const QString id = cmd->id;
            connect(action, &QAction::triggered, this, [this, id]() {
                if (m_commandRegistry) m_commandRegistry->executeById(id);
            });
            commandActions.append(action);
        }
        if (!commandActions.isEmpty())
            groups.append({i18n("Commands"), commandActions});
    }

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
    if (relativePath.endsWith(QStringLiteral(".canvas"))) {
        QString absPath = m_vaultService->vault()->path() + QLatin1Char('/') + relativePath;
        m_editorManager->openCanvas(absPath);
        return;
    }

    auto *doc = m_vaultService->noteService()->openNote(relativePath);
    if (doc) {
        m_editorManager->openNote(doc);
    } else {
        // Note doesn't exist -- create it (Obsidian behavior: clicking unresolved link creates the note)
        QString name = relativePath;
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        // Extract folder if path has one
        QString folder;
        int lastSlash = name.lastIndexOf(QLatin1Char('/'));
        if (lastSlash >= 0) {
            folder = name.left(lastSlash);
            name = name.mid(lastSlash + 1);
        }
        doc = m_vaultService->noteService()->createNote(name, folder);
        if (doc) {
            m_editorManager->openNote(doc);
        }
    }
}

void MainWindow::onVaultOpened()
{
    auto *vault = m_vaultService->vault();

    // Switch to editor view
    m_centralStack->setCurrentIndex(1);
    setSidebarsVisible(true);

    updateWindowTitle();

    // Track in recent vaults
    m_recentVaults->addUrl(QUrl::fromLocalFile(vault->path()));
    auto config = KSharedConfig::openConfig();
    KConfigGroup recentGroup = config->group(QStringLiteral("RecentVaults"));
    m_recentVaults->saveEntries(recentGroup);
    config->sync();

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
        // Update status bar when view mode changes
        connect(editor, &NoteEditorWidget::viewModeChanged,
                this, [this](NoteEditorWidget::ViewMode mode) {
            if (mode == NoteEditorWidget::ViewMode::Reading)
                m_cursorPosLabel->setText(i18n("Reading"));
            // For Source/LivePreview, next cursorInfoChanged restores cursor pos
        }, Qt::UniqueConnection);
        // Apply immediately if already in reading mode
        if (editor->viewMode() == NoteEditorWidget::ViewMode::Reading)
            m_cursorPosLabel->setText(i18n("Reading"));
    });

    // Create LinkResolver — shared by MetadataCache (for link resolution in
    // parsed CachedMetadata) and seeded with the vault's note paths.
    delete m_linkResolver;
    m_linkResolver = new LinkResolver();
    {
        QStringList allPaths;
        const auto allNotes = vault->allNotes();
        allPaths.reserve(allNotes.size());
        for (const auto &meta : allNotes) {
            allPaths.append(meta.relativePath);
        }
        m_linkResolver->setVaultPaths(allPaths);
    }

    // Create MetadataCache (owns the five-signal API + worker thread) and
    // SQLiteIndex (derives FTS / links / tags from cache events).
    delete m_metadataCache;
    m_metadataCache = new MetadataCache(*m_linkResolver, this);

    // Cluster J Phase 6 — point the embed renderer at the now-open vault.
    // The HoverPopover already holds the renderer pointer; updating cache
    // + resources here means subsequent hover-link previews resolve
    // against this vault.
    m_popoverResources = std::make_unique<VaultScopedResources>(vault);
    if (m_embedRenderer) {
        m_embedRenderer->setMetadataCache(m_metadataCache);
        m_embedRenderer->setResources(m_popoverResources.get());
    }

    delete m_searchIndex;
    m_searchIndex = new SQLiteIndex(this);
    m_searchIndex->open(vault->configPath() + QStringLiteral("/index.sqlite"));
    m_searchIndex->setVaultRoot(vault->path());
    m_searchIndex->setMetadataCache(m_metadataCache);

    // Ensure .corbomite/ exists, then open the MetadataCache's persistent store.
    QDir().mkpath(vault->configPath());
    m_metadataCache->open(vault->configPath() + QStringLiteral("/metadata-cache.db"));

    // MetadataCache::open() loads persisted state silently (no cacheChanged
    // emissions), so SQLiteIndex's link/FTS/tag rows would otherwise stay
    // empty for any path whose stat matches disk after rebuildVault's
    // short-circuit. Reconcile explicitly against the freshly-loaded cache.
    m_searchIndex->reconcileWithCache();

    // Kick off the initial vault scan via MetadataCache.
    statusBar()->showMessage(i18n("Indexing vault..."));
    connect(m_metadataCache, &MetadataCache::indexFinished, this, [this]() {
        statusBar()->showMessage(i18n("Indexing complete"), 3000);
    });

    {
        QStringList notePaths;
        const auto allNotes = vault->allNotes();
        notePaths.reserve(allNotes.size());
        for (const auto &meta : allNotes) {
            notePaths.append(meta.relativePath);
        }
        m_metadataCache->rebuildVault(vault->path(), notePaths);
    }

    m_searchPanel->setIndex(m_searchIndex);
    m_searchPanel->setMetadataCache(m_metadataCache);
    m_vaultService->noteService()->setSearchIndex(m_searchIndex);
    m_vaultService->vault()->setSearchIndex(m_searchIndex);

    // Set index + metadata cache on sidebar panels
    m_backlinksPanel->setIndex(m_searchIndex);
    m_backlinksPanel->setMetadataCache(m_metadataCache);
    m_outlinksPanel->setIndex(m_searchIndex);
    m_outlinksPanel->setVaultModel(vault);
    m_outlinksPanel->setMetadataCache(m_metadataCache);
    m_localGraphPanel->setIndex(m_searchIndex);
    m_localGraphPanel->setVaultModel(vault);
    m_localGraphPanel->setMetadataCache(m_metadataCache);
    m_propertiesPanel->setMetadataCache(m_metadataCache);

    // Update sidebar panels when active note changes
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        if (editor && editor->noteDocument()) {
            m_backlinksPanel->setCurrentNote(editor->noteDocument());
            m_outlinksPanel->setCurrentNote(editor->noteDocument());
            m_outlinePanel->setCurrentNote(editor->noteDocument());
            m_localGraphPanel->setCurrentNote(editor->noteDocument());
            m_propertiesPanel->setCurrentNote(editor->noteDocument());
        } else {
            m_backlinksPanel->setCurrentNote(nullptr);
            m_outlinksPanel->setCurrentNote(nullptr);
            m_outlinePanel->setCurrentNote(nullptr);
            m_localGraphPanel->setCurrentNote(nullptr);
            m_propertiesPanel->setCurrentNote(nullptr);
        }
    });

    // Update window title when active note changes
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        updateWindowTitle(editor);
        // Track modification state for title indicator — use singleShot-style
        // disconnect/reconnect to avoid duplicate connections from repeated tab switches
        if (editor && editor->noteDocument()) {
            disconnect(editor->noteDocument(), &NoteDocument::modificationChanged,
                       this, nullptr);
            connect(editor->noteDocument(), &NoteDocument::modificationChanged,
                    this, [this]() {
                updateWindowTitle(m_editorManager->activeEditor());
            });
        }
    });

    // Update MetadataCache on note saves — feeds cacheChanged → SQLiteIndex.
    connect(m_autosave, &AutosaveReactor::noteSaved, this, [this](const QString &relPath) {
        if (!m_metadataCache || !m_vaultService->vault()) return;
        auto *doc = m_vaultService->vault()->cachedDocument(relPath);
        if (!doc) return;
        const QByteArray bytes = doc->markdown().toUtf8();
        const QString absPath =
            m_vaultService->vault()->path() + QLatin1Char('/') + relPath;
        const qint64 mtimeMs = QFileInfo(absPath).lastModified().toMSecsSinceEpoch();
        m_metadataCache->onFileChanged(relPath, bytes, mtimeMs);
    });

    // Propagate external file-watcher deletions to MetadataCache.
    if (m_fileWatch) {
        connect(m_fileWatch, &FileWatchReactor::fileDeletedExternally,
                this, [this](const QString &relPath) {
            if (m_metadataCache) m_metadataCache->onFileDeleted(relPath);
        });
        connect(m_fileWatch, &FileWatchReactor::fileCreatedExternally,
                this, [this](const QString &relPath) {
            if (!m_metadataCache || !m_vaultService->vault()) return;
            const QString absPath =
                m_vaultService->vault()->path() + QLatin1Char('/') + relPath;
            QFile f(absPath);
            if (!f.open(QIODevice::ReadOnly)) return;
            const QByteArray bytes = f.readAll();
            const qint64 mtimeMs =
                QFileInfo(absPath).lastModified().toMSecsSinceEpoch();
            m_metadataCache->onFileChanged(relPath, bytes, mtimeMs);
        });
    }

    // Connect internal link navigation from preview widgets
    connect(m_editorManager->activeViewSpace(), &EditorViewSpace::internalLinkClicked,
            this, &MainWindow::onNoteActivated, Qt::UniqueConnection);

    // Connect graph view note navigation
    connect(m_editorManager, &EditorViewManager::graphNoteActivated,
            this, &MainWindow::onNoteActivated, Qt::UniqueConnection);

    // Create session manager and restore session
    delete m_sessionManager;
    m_sessionManager = new SessionManager(this);
    m_sessionManager->setSessionPath(vault->path() + QStringLiteral("/.obsidian/workspace.json"));
    m_sessionManager->load();

    // Restore window geometry if available
    const auto geometry = m_sessionManager->windowGeometry();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
    const auto windowStateBytes = m_sessionManager->windowState();
    if (!windowStateBytes.isEmpty()) restoreState(windowStateBytes);

    // Restore sidebar state
    const auto sidebar = m_sessionManager->sidebarState();
    if (!sidebar.isEmpty()) {
        const bool leftVisible = sidebar.value(QStringLiteral("leftVisible")).toBool(true);
        setSidebarsVisibleInternal(leftVisible, true);
    }

    // Block saving during restore to avoid partial session writes
    m_sessionManager->blockSaving();

    if (m_sessionManager->hasLoadedSession()) {
        m_editorManager->applyPaneLayout(
            m_sessionManager->paneLayout(),
            [this](EditorViewSpace *space, const PaneLeaf &leaf) {
                const QString path = leaf.filePath;
                if (path.isEmpty()) return;
                if (leaf.viewType == QStringLiteral("canvas")
                        || path.endsWith(QStringLiteral(".canvas"))) {
                    const QString absPath =
                        m_vaultService->vault()->path()
                        + QLatin1Char('/') + path;
                    space->openCanvas(absPath);
                } else {
                    auto *doc = m_vaultService->noteService()->openNote(path);
                    if (doc) space->openNote(doc);
                }
            });
    }

    // Restore expanded folders
    const auto folders = m_sessionManager->expandedFolders();
    if (!folders.isEmpty() && m_fileExplorer) {
        m_fileExplorer->restoreExpandedFolders(folders);
    }

    m_sessionManager->unblockSaving();

    // Template and Daily Note services
    auto *settings = CorbomiteSettings::self();

    delete m_templateService;
    m_templateService = new TemplateService(vault, this);
    m_templateService->setTemplateFolder(settings->templateFolder());
    m_templateService->setDefaultDateFormat(settings->defaultDateFormat());
    m_templateService->setDefaultTimeFormat(settings->defaultTimeFormat());

    // Vault-local override: `.obsidian/templates.json` wins over KConfig
    // defaults for any keys it specifies. Missing file / missing keys
    // leave the KConfig-seeded defaults intact.
    {
        FileSystemAdapter fs;
        VaultConfig vaultConfig(&fs, vault->path());
        m_templateService->initFromVaultConfig(vaultConfig);
    }

    delete m_dailyNoteService;
    m_dailyNoteService = new DailyNoteService(vault, m_vaultService->noteService(),
                                                m_templateService, this);
    m_dailyNoteService->setDateFormat(settings->dailyNoteDateFormat());
    m_dailyNoteService->setFolder(settings->dailyNoteFolder());
    m_dailyNoteService->setTemplateName(settings->dailyNoteTemplate());

    // Vault-local override: `.obsidian/daily-notes.json` wins over KConfig
    // defaults. Missing file / missing keys leave the KConfig-seeded
    // values intact.
    {
        FileSystemAdapter fs;
        VaultConfig vaultConfig(&fs, vault->path());
        m_dailyNoteService->initFromVaultConfig(vaultConfig);
    }

    updateVaultActions();
}

void MainWindow::onVaultClosed()
{
    // Save session before cleanup
    if (m_sessionManager) {
        m_sessionManager->saveWindowGeometry(saveGeometry(), saveState());
        m_sessionManager->saveSidebarState(sidebarsVisible(), 200, false, 200);
        m_sessionManager->setPaneLayout(m_editorManager->buildPaneLayout());
        if (m_fileExplorer) {
            m_sessionManager->saveExpandedFolders(m_fileExplorer->expandedFolders());
        }
        m_sessionManager->saveNow();
    }

    // Disconnect vault-specific signals — prevents stale callbacks during cleanup
    disconnect(m_editorManager, &EditorViewManager::activeEditorChanged, this, nullptr);
    disconnect(m_editorManager, &EditorViewManager::graphNoteActivated, this, nullptr);

    // Reconnect non-vault signals that setupEditor originally set up
    connect(m_editorManager, &EditorViewManager::cursorInfoChanged,
            this, &MainWindow::onCursorInfoChanged);

    // Close all editor state — THIS FIXES THE CRASH
    m_editorManager->closeAllDocuments();

    // Clean up reactors
    delete m_autosave;
    m_autosave = nullptr;
    if (m_fileWatch) {
        m_fileWatch->stopWatching();
    }
    delete m_fileWatch;
    m_fileWatch = nullptr;
    delete m_sessionManager;
    m_sessionManager = nullptr;

    delete m_templateService;
    m_templateService = nullptr;
    delete m_dailyNoteService;
    m_dailyNoteService = nullptr;

    m_vaultService->noteService()->setSearchIndex(nullptr);

    m_backlinksPanel->setIndex(nullptr);
    m_backlinksPanel->setMetadataCache(nullptr);
    m_backlinksPanel->setCurrentNote(nullptr);
    m_outlinksPanel->setIndex(nullptr);
    m_outlinksPanel->setMetadataCache(nullptr);
    m_outlinksPanel->setVaultModel(nullptr);
    m_outlinksPanel->setCurrentNote(nullptr);
    m_outlinePanel->setCurrentNote(nullptr);
    m_localGraphPanel->setIndex(nullptr);
    m_localGraphPanel->setMetadataCache(nullptr);
    m_localGraphPanel->setVaultModel(nullptr);
    m_localGraphPanel->setCurrentNote(nullptr);
    m_propertiesPanel->setCurrentNote(nullptr);
    m_propertiesPanel->setMetadataCache(nullptr);

    // Cluster J Phase 6 — drop renderer pointers into the cache + resources
    // BEFORE the cache + resources are torn down (the embed renderer holds
    // raw pointers and will dereference them on the next preview).
    if (m_embedRenderer) {
        m_embedRenderer->setMetadataCache(nullptr);
        m_embedRenderer->setResources(nullptr);
    }
    m_popoverResources.reset();

    // Close MetadataCache (flushes persistent store) BEFORE deleting the
    // SQLiteIndex — the index subscribes to cache signals and must not
    // receive a post-mortem emission.
    if (m_metadataCache) {
        m_metadataCache->close();
    }
    delete m_searchIndex;
    m_searchIndex = nullptr;
    delete m_metadataCache;
    m_metadataCache = nullptr;
    delete m_linkResolver;
    m_linkResolver = nullptr;
    m_searchPanel->setIndex(nullptr);
    m_searchPanel->setMetadataCache(nullptr);

    delete m_treeModel;
    m_treeModel = nullptr;
    m_fileExplorer->setModel(nullptr);

    // Switch to welcome screen
    m_centralStack->setCurrentIndex(0);
    m_welcomeScreen->refreshRecentVaults();
    setSidebarsVisibleInternal(false, true);

    updateWindowTitle();
    updateVaultActions();
}

void MainWindow::openGraphView()
{
    if (!m_vaultService->isOpen() || !m_searchIndex) return;
    m_editorManager->openGraphView(m_searchIndex, m_vaultService->vault());

    // Wire the sidebar graph controls panel + MetadataCache to the newly
    // opened graph tab.
    if (auto *space = m_editorManager->activeViewSpace()) {
        const auto graphTabs = space->findChildren<GraphViewTab *>();
        if (!graphTabs.isEmpty()) {
            auto *graphTab = graphTabs.first();
            if (m_graphControlsPanel) {
                graphTab->setControlsPanel(m_graphControlsPanel);
            }
            graphTab->setMetadataCache(m_metadataCache);
        }
    }
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

void MainWindow::insertTemplate()
{
    if (!m_templateService) return;

    auto templates = m_templateService->availableTemplates();
    if (templates.isEmpty()) {
        statusBar()->showMessage(i18n("No templates found in '%1' folder",
                                       m_templateService->templateFolder()), 3000);
        return;
    }

    TemplatePicker picker(templates, this);
    if (picker.exec() != QDialog::Accepted) return;

    QString name = picker.selectedTemplate();
    if (name.isEmpty()) return;

    auto *editor = m_editorManager->activeEditor();
    if (!editor || !editor->noteDocument()) return;

    QString expanded = m_templateService->loadAndExpand(name, editor->noteDocument()->name());
    if (expanded.isEmpty()) return;

    // If note is empty, replace content; otherwise append at end.
    // Compute final body and the absolute offset of the {{cursor}} marker
    // within that body (if any), then strip the marker before setting the
    // note document so the editor never sees the placeholder.
    const QString marker = TemplateService::cursorMarker();
    const bool wasEmpty = editor->noteDocument()->markdown().trimmed().isEmpty();
    QString finalBody = wasEmpty
        ? expanded
        : editor->noteDocument()->markdown() + expanded;

    const int cursorIdx = finalBody.indexOf(marker);
    if (cursorIdx >= 0) {
        finalBody.remove(cursorIdx, marker.size());
    }
    editor->noteDocument()->setMarkdown(finalBody);

    // Position the editor cursor at the former marker location. Markoff's
    // Editor only exposes goToLine(int) from the outside; compute the
    // 0-based line index of the stripped-marker offset. Column-level
    // positioning would need a new Editor API — acceptable limitation for
    // Phase 3; the cursor lands on the right line.
    if (cursorIdx >= 0) {
        const int line = finalBody.left(cursorIdx).count(QLatin1Char('\n'));
        if (auto *mk = editor->editor()) {
            mk->goToLine(line);
        }
    }
}

void MainWindow::openDailyNote()
{
    if (!m_dailyNoteService) return;

    auto *doc = m_dailyNoteService->openOrCreateToday();
    if (doc) {
        m_editorManager->openNote(doc);
    }
}

void MainWindow::onCursorInfoChanged(int line, int column, int wordCount)
{
    m_wordCountLabel->setText(i18n("Words: %1", wordCount));
    m_cursorPosLabel->setText(i18n("Ln %1, Col %2", line, column));
}

void MainWindow::updateVaultActions()
{
    bool open = m_vaultService->isOpen();

    auto *ac = actionCollection();
    auto setEnabled = [ac](const QString &name, bool enabled) {
        if (auto *a = ac->action(name)) a->setEnabled(enabled);
    };

    setEnabled(QStringLiteral("file_close_vault"), open);
    setEnabled(QStringLiteral("file_new_note"), open);
    setEnabled(QStringLiteral("file_new_canvas"), open);
    setEnabled(QStringLiteral("file_save"), open);
    setEnabled(QStringLiteral("quick_switcher"), open);
    setEnabled(QStringLiteral("search_vault"), open);
    setEnabled(QStringLiteral("graph_view"), open);
    setEnabled(QStringLiteral("view_editing_mode"), open);
    setEnabled(QStringLiteral("view_reading_mode"), open);
    setEnabled(QStringLiteral("tab_close"), open);
    setEnabled(QStringLiteral("tab_next"), open);
    setEnabled(QStringLiteral("tab_prev"), open);
    setEnabled(QStringLiteral("insert_template"), open);
    setEnabled(QStringLiteral("open_daily_note"), open);
    setEnabled(QStringLiteral("split_right"), open);
    setEnabled(QStringLiteral("split_down"), open);
}

void MainWindow::updateWindowTitle(NoteEditorWidget *editor)
{
    QString title;
    if (editor && editor->noteDocument()) {
        QString noteName = editor->noteDocument()->name();
        bool modified = editor->noteDocument()->isModified();
        title = noteName;
        if (modified) title += QStringLiteral(" \u2022");
        title += QStringLiteral(" \u2014 ");
    }

    if (m_vaultService->isOpen()) {
        title += m_vaultService->vault()->name();
        title += QStringLiteral(" \u2014 ");
    }

#ifdef CORBOMITE_DEV_BUILD
    title += QStringLiteral("Corbomite [Dev]");
#else
    title += QStringLiteral("Corbomite");
#endif

    setWindowTitle(title);
}

} // namespace Corbomite
