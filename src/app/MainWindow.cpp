// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "WelcomeScreen.h"
#include "VaultService.h"
#include "editor/NoteEditorWidget.h"
#include <markoff/Editor.h>
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/FileView.h"
#include "corbomite/core/TextFileView.h"
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
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/VaultConfig.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/core/NoteDocument.h"
#include "reactors/AutosaveReactor.h"
#include "SessionManager.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/EditorSuggestManager.h"
#include "corbomite/core/EmbedRegistry.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/View.h"
#include "editor/MarkdownView.h"
#include "canvas/CanvasFileView.h"
#include "graph/GraphView.h"
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
#include <QDebug>
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
#include <QScrollBar>

namespace Corbomite {

namespace {

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
        if (auto *doc = m_vault->cachedDocument(rel)) {
            return doc->markdown();
        }
        const QString abs = m_vault->path() + QLatin1Char('/') + rel;
        QFile f(abs);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString::fromUtf8(f.readAll());
        }
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

    m_embedRegistry = std::make_unique<Corbomite::Core::EmbedRegistry>();
    m_embedRenderer = std::make_unique<Corbomite::ReadingView::EmbedRenderer>(
        m_embedRegistry.get(), /*cache=*/nullptr, /*resources=*/nullptr);
    Corbomite::ReadingView::registerBuiltinEmbedFactories(*m_embedRegistry,
                                                          *m_embedRenderer);
    m_hoverPopover->setEmbedRenderer(m_embedRenderer.get());

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
    delete m_autosave;
    m_autosave = nullptr;

    // FileWatchReactor teardown removed (Q.0 P2 T2.2).

    delete m_sessionManager;
    m_sessionManager = nullptr;

    if (m_embedRenderer) {
        m_embedRenderer->setMetadataCache(nullptr);
        m_embedRenderer->setResources(nullptr);
    }
    if (m_hoverPopover) m_hoverPopover->setEmbedRenderer(nullptr);
    m_popoverResources.reset();

    if (m_metadataCache) {
        m_metadataCache->close();
    }
    // Q.0 P6 — FileManager references MetadataCache; tear down first.
    delete m_fileManager;
    m_fileManager = nullptr;
    if (m_vaultObj) m_vaultObj->unload();
    delete m_vaultObj;
    m_vaultObj = nullptr;
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

    m_embedRenderer.reset();
    m_embedRegistry.reset();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_vaultService->isOpen()) {
        if (!confirmCloseUnsaved()) {
            event->ignore();
            return;
        }
        saveSessionState();
    }

    CorbomiteMDI::MainWindow::closeEvent(event);
}

// --- Helper methods ---

MarkdownView *MainWindow::activeMarkdownView() const
{
    if (!m_workspace || !m_workspace->activeLeaf())
        return nullptr;
    return qobject_cast<MarkdownView *>(m_workspace->activeLeaf()->view());
}

NoteEditorWidget *MainWindow::activeEditor() const
{
    auto *mv = activeMarkdownView();
    return mv ? mv->editorWidget() : nullptr;
}

void MainWindow::openFileInWorkspace(const QString &relativePath)
{
    if (!m_workspace || !m_viewRegistry)
        return;

    // Check if file is already open in any leaf
    for (auto *leaf : m_workspace->allLeaves()) {
        if (leaf->isDeferred()) {
            QJsonObject vs = leaf->getViewState();
            QJsonObject state = vs[QStringLiteral("state")].toObject();
            if (state[QStringLiteral("file")].toString() == relativePath) {
                leaf->loadIfDeferred();
                m_workspace->setActiveLeaf(leaf);
                return;
            }
            continue;
        }
        if (auto *fv = qobject_cast<FileView *>(leaf->view())) {
            if (fv->file() && fv->file()->relativePath() == relativePath) {
                m_workspace->setActiveLeaf(leaf);
                return;
            }
        }
    }

    auto *tabs = m_workspace->activeTabs();
    if (!tabs) return;

    auto *leaf = m_workspace->createLeafInTabs(tabs);

    QString ext = QFileInfo(relativePath).suffix().toLower();
    QString type = m_viewRegistry->getTypeByExtension(ext);
    if (type.isEmpty()) type = QStringLiteral("markdown");

    QJsonObject viewState;
    viewState[QStringLiteral("type")] = type;
    viewState[QStringLiteral("state")] = QJsonObject{
        {QStringLiteral("file"), relativePath}
    };
    leaf->setViewState(viewState);
    m_workspace->setActiveLeaf(leaf);
    m_workspace->pushLastOpenFile(relativePath);
}

bool MainWindow::confirmCloseUnsaved()
{
    if (!m_workspace) return true;

    QStringList modifiedPaths;
    for (auto *leaf : m_workspace->allLeaves()) {
        auto *mv = qobject_cast<MarkdownView *>(leaf->view());
        if (!mv) continue;
        auto *editor = mv->editorWidget();
        if (editor && editor->noteDocument() && editor->noteDocument()->isModified()) {
            modifiedPaths.append(editor->noteDocument()->relativePath());
        }
    }
    modifiedPaths.removeDuplicates();

    if (modifiedPaths.isEmpty())
        return true;

    QStringList names;
    for (const QString &path : std::as_const(modifiedPaths)) {
        QString name = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        names.append(name);
    }

    QString message;
    if (names.size() == 1) {
        message = i18n("The document \"%1\" has unsaved changes.\n\nDo you want to save before closing?", names.first());
    } else {
        message = i18n("The following documents have unsaved changes:\n\n%1\n\nDo you want to save before closing?",
                        names.join(QStringLiteral("\n")));
    }

    auto result = KMessageBox::warningTwoActionsCancel(
        this,
        message,
        i18n("Unsaved Changes"),
        KStandardGuiItem::save(),
        KStandardGuiItem::discard()
    );

    if (result == KMessageBox::Cancel)
        return false;

    if (result == KMessageBox::PrimaryAction) {
        for (auto *leaf : m_workspace->allLeaves()) {
            auto *tfv = qobject_cast<TextFileView *>(leaf->view());
            if (tfv) tfv->saveImmediately();
        }
    }

    return true;
}

void MainWindow::saveSessionState()
{
    if (!m_sessionManager) return;
    m_sessionManager->blockSaving();
    m_sessionManager->saveWindowGeometry(saveGeometry(), saveState());
    m_sessionManager->saveSidebarState(sidebarsVisible(), 200, false, 200);
    if (m_workspace) {
        QJsonObject wsJson = m_workspace->serialize();
        QString activeId = m_workspace->activeLeaf()
            ? m_workspace->activeLeaf()->id() : QString();
        if (!wsJson.contains(QStringLiteral("main"))) {
            qWarning() << "MainWindow: Workspace::serialize() missing 'main' key; skipping workspace layout save";
        } else {
            m_sessionManager->setWorkspaceLayout(
                wsJson[QStringLiteral("main")].toObject(), activeId);
        }
    }
    if (m_fileExplorer) {
        m_sessionManager->saveExpandedFolders(m_fileExplorer->expandedFolders());
    }
    m_sessionManager->unblockSaving();
    m_sessionManager->saveNow();
}

void MainWindow::propagateServicesToView(View *view)
{
    if (!view) return;

    if (auto *mv = qobject_cast<MarkdownView *>(view)) {
        if (m_vaultService->isOpen())
            mv->setVaultModel(m_vaultService->vault());
        mv->setHoverPopover(m_hoverPopover);
        mv->setEditorSuggestManager(m_suggestManager);

        auto *editor = mv->editorWidget();
        if (editor) {
            connect(editor, &NoteEditorWidget::linkActivated,
                    this, &MainWindow::onNoteActivated, Qt::UniqueConnection);
            connect(editor, &NoteEditorWidget::cursorInfoChanged,
                    this, &MainWindow::onCursorInfoChanged, Qt::UniqueConnection);
            // Guard with property — Qt::UniqueConnection doesn't work for lambdas.
            if (!editor->property("_mw_viewmode").toBool()) {
                editor->setProperty("_mw_viewmode", true);
                connect(editor, &NoteEditorWidget::viewModeChanged,
                        this, [this](NoteEditorWidget::ViewMode mode) {
                    if (mode == NoteEditorWidget::ViewMode::Reading)
                        m_cursorPosLabel->setText(i18n("Reading"));
                });
            }
        }
        return;
    }

    if (auto *graphTab = view->findChild<GraphViewTab *>()) {
        if (m_graphControlsPanel)
            graphTab->setControlsPanel(m_graphControlsPanel);
        graphTab->setMetadataCache(m_metadataCache);
    }
}

// --- Actions ---

void MainWindow::setupActions()
{
    KActionCollection *ac = actionCollection();

    KStandardAction::quit(qApp, &QApplication::quit, ac);
    auto *prefsAction = KStandardAction::preferences(this, [this]() {
        SettingsDialog dialog(this);
        dialog.exec();
    }, ac);
    ac->setDefaultShortcut(prefsAction, QKeySequence(Qt::CTRL | Qt::Key_Comma));

    auto *openVault = ac->addAction(QStringLiteral("file_open_vault"));
    openVault->setText(i18n("Open Vault..."));
    openVault->setIcon(QIcon::fromTheme(QStringLiteral("folder-open")));
    ac->setDefaultShortcut(openVault, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(openVault, &QAction::triggered, this, &MainWindow::openVaultDialog);

    m_recentVaults = KStandardAction::openRecent(this, [this](const QUrl &url) {
        if (m_vaultService->isOpen()) {
            if (!confirmCloseUnsaved()) return;
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
            Canvas::CanvasDocument emptyDoc;
            emptyDoc.saveToFile(absPath);
            openFileInWorkspace(relPath);
        }
    });

    auto *save = ac->addAction(QStringLiteral("file_save"));
    save->setText(i18n("Save"));
    save->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    ac->setDefaultShortcut(save, QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(save, &QAction::triggered, this, &MainWindow::saveCurrentNote);

    KStandardAction::undo(this, [this]() {
        auto *editor = activeEditor();
        if (editor) editor->editor()->undo();
    }, ac);

    KStandardAction::redo(this, [this]() {
        auto *editor = activeEditor();
        if (editor) editor->editor()->redo();
    }, ac);

    KStandardAction::find(this, [this]() {
        Q_UNUSED(this)
    }, ac);

    KStandardAction::aboutApp(qApp, []() {}, ac);
    KStandardAction::aboutKDE(qApp, []() {}, ac);

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

    auto *editingMode = ac->addAction(QStringLiteral("view_editing_mode"));
    editingMode->setText(i18n("Live Preview"));
    editingMode->setIcon(QIcon::fromTheme(QStringLiteral("text-x-markdown")));
    connect(editingMode, &QAction::triggered, this, [this]() {
        if (auto *editor = activeEditor())
            editor->setViewMode(NoteEditorWidget::ViewMode::LivePreview);
    });

    auto *sourceMode = ac->addAction(QStringLiteral("view_source_mode"));
    sourceMode->setText(i18n("Source"));
    sourceMode->setIcon(QIcon::fromTheme(QStringLiteral("text-plain")));
    connect(sourceMode, &QAction::triggered, this, [this]() {
        if (auto *editor = activeEditor())
            editor->setViewMode(NoteEditorWidget::ViewMode::Source);
    });

    auto *readingMode = ac->addAction(QStringLiteral("view_reading_mode"));
    readingMode->setText(i18n("Reading"));
    readingMode->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
    ac->setDefaultShortcut(readingMode, QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(readingMode, &QAction::triggered, this, [this]() {
        if (auto *editor = activeEditor())
            editor->setViewMode(NoteEditorWidget::ViewMode::Reading);
    });

    // Tab shortcuts — use Workspace's WorkspaceTabs
    auto *closeTab = ac->addAction(QStringLiteral("tab_close"));
    closeTab->setText(i18n("Close Tab"));
    closeTab->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    ac->setDefaultShortcut(closeTab, QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(closeTab, &QAction::triggered, this, [this]() {
        if (!m_workspace) return;
        auto *leaf = m_workspace->activeLeaf();
        if (leaf)
            m_workspace->closeLeaf(leaf);
    });

    auto *nextTab = ac->addAction(QStringLiteral("tab_next"));
    nextTab->setText(i18n("Next Tab"));
    ac->setDefaultShortcut(nextTab, QKeySequence(Qt::CTRL | Qt::Key_Tab));
    connect(nextTab, &QAction::triggered, this, [this]() {
        if (!m_workspace) return;
        auto *tabs = m_workspace->activeTabs();
        if (tabs && tabs->childCount() > 1) {
            int next = (tabs->currentTab() + 1) % tabs->childCount();
            tabs->setCurrentTab(next);
        }
    });

    auto *prevTab = ac->addAction(QStringLiteral("tab_prev"));
    prevTab->setText(i18n("Previous Tab"));
    ac->setDefaultShortcut(prevTab, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    connect(prevTab, &QAction::triggered, this, [this]() {
        if (!m_workspace) return;
        auto *tabs = m_workspace->activeTabs();
        if (tabs && tabs->childCount() > 1) {
            int prev = tabs->currentTab() - 1;
            if (prev < 0) prev = tabs->childCount() - 1;
            tabs->setCurrentTab(prev);
        }
    });

    auto *splitRight = ac->addAction(QStringLiteral("split_right"));
    splitRight->setText(i18n("Split Right"));
    ac->setDefaultShortcut(splitRight, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right));
    connect(splitRight, &QAction::triggered, this, [this]() {
        if (!m_workspace) return;
        if (auto *leaf = m_workspace->activeLeaf())
            m_workspace->splitLeaf(leaf, Qt::Horizontal);
    });

    auto *splitDown = ac->addAction(QStringLiteral("split_down"));
    splitDown->setText(i18n("Split Down"));
    ac->setDefaultShortcut(splitDown, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down));
    connect(splitDown, &QAction::triggered, this, [this]() {
        if (!m_workspace) return;
        if (auto *leaf = m_workspace->activeLeaf())
            m_workspace->splitLeaf(leaf, Qt::Vertical);
    });

    // Undo close tab
    auto *undoClose = ac->addAction(QStringLiteral("tab_undo_close"));
    undoClose->setText(i18n("Undo Close Tab"));
    ac->setDefaultShortcut(undoClose, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(undoClose, &QAction::triggered, this, [this]() {
        if (m_workspace)
            m_workspace->undoCloseLeaf();
    });
}

void MainWindow::setupEditor()
{
    m_centralStack = new QStackedWidget(centralWidget());
    centralWidget()->layout()->addWidget(m_centralStack);

    // Index 0: Welcome screen
    m_welcomeScreen = new WelcomeScreen(m_vaultService, m_centralStack);
    m_centralStack->addWidget(m_welcomeScreen);

    connect(m_welcomeScreen, &WelcomeScreen::vaultRequested, this, [this](const QString &path) {
        if (m_vaultService->isOpen()) {
            if (!confirmCloseUnsaved()) return;
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

    // Create ViewRegistry — built-in view factories
    m_viewRegistry = new ViewRegistry(this);
    m_viewRegistry->registerViewWithExtensions(
        {QStringLiteral("md")}, QStringLiteral("markdown"),
        &MarkdownView::factory);
    m_viewRegistry->registerViewWithExtensions(
        {QStringLiteral("canvas")}, QStringLiteral("canvas"),
        &CanvasFileView::factory);
    m_viewRegistry->registerView(QStringLiteral("graph"), &GraphView::factory);

    // Index 1: Workspace (replaces EditorViewManager)
    m_workspace = new Workspace(m_viewRegistry, this);
    m_workspaceContainer = new QWidget(m_centralStack);
    auto *wsLayout = new QVBoxLayout(m_workspaceContainer);
    wsLayout->setContentsMargins(0, 0, 0, 0);
    wsLayout->addWidget(m_workspace->mainRoot()->widget());
    m_centralStack->addWidget(m_workspaceContainer);

    // Keep the container, tab signals, and service propagation in sync.
    connect(m_workspace, &Workspace::layoutChanged, this, [this]() {
        auto *rootWidget = m_workspace->mainRoot()->widget();
        if (rootWidget->parentWidget() != m_workspaceContainer) {
            m_workspaceContainer->layout()->addWidget(rootWidget);
        }

        for (auto *leaf : m_workspace->allLeaves()) {
            // Wire tab container signals (once per container)
            auto *tabs = qobject_cast<WorkspaceTabs *>(leaf->parentItem());
            if (tabs && !tabs->property("_mw_tabs_connected").toBool()) {
                tabs->setProperty("_mw_tabs_connected", true);
                connect(tabs, &WorkspaceTabs::currentTabChanged, this,
                        [this, tabs](int index) {
                    if (auto *l = tabs->leafAt(index))
                        m_workspace->setActiveLeaf(l);
                });
                connect(tabs, &WorkspaceTabs::tabCloseRequested, this,
                        [this, tabs](int index) {
                    if (auto *l = tabs->leafAt(index))
                        m_workspace->closeLeaf(l);
                });
            }

            // Wire deferred-load service propagation (once per leaf)
            if (!leaf->property("_mw_leaf_connected").toBool()) {
                leaf->setProperty("_mw_leaf_connected", true);
                connect(leaf, &WorkspaceLeaf::viewChanged, this,
                        [this](View *v) { propagateServicesToView(v); });
            }

            // Propagate services to any view that exists now
            if (leaf->view())
                propagateServicesToView(leaf->view());
        }
    });

    // When the active leaf changes, propagate services and update UI
    connect(m_workspace, &Workspace::activeLeafChanged,
            this, [this](WorkspaceLeaf *leaf) {
        if (leaf && leaf->view())
            propagateServicesToView(leaf->view());
        updateWindowTitle(activeEditor());

        auto *editor = activeEditor();
        // Update sidebar panels
        if (editor && editor->noteDocument()) {
            if (m_backlinksPanel) m_backlinksPanel->setCurrentNote(editor->noteDocument());
            if (m_outlinksPanel) m_outlinksPanel->setCurrentNote(editor->noteDocument());
            if (m_outlinePanel) m_outlinePanel->setCurrentNote(editor->noteDocument());
            if (m_localGraphPanel) m_localGraphPanel->setCurrentNote(editor->noteDocument());
            if (m_propertiesPanel) m_propertiesPanel->setCurrentNote(editor->noteDocument());
        } else {
            if (m_backlinksPanel) m_backlinksPanel->setCurrentNote(nullptr);
            if (m_outlinksPanel) m_outlinksPanel->setCurrentNote(nullptr);
            if (m_outlinePanel) m_outlinePanel->setCurrentNote(nullptr);
            if (m_localGraphPanel) m_localGraphPanel->setCurrentNote(nullptr);
            if (m_propertiesPanel) m_propertiesPanel->setCurrentNote(nullptr);
        }

        if (editor && editor->noteDocument() && m_autosave)
            m_autosave->watchDocument(editor->noteDocument());

        if (editor && editor->noteDocument()) {
            disconnect(editor->noteDocument(), &NoteDocument::modificationChanged,
                       this, nullptr);
            connect(editor->noteDocument(), &NoteDocument::modificationChanged,
                    this, [this]() {
                updateWindowTitle(activeEditor());
            });
        }
    });


    // Start on welcome screen
    m_centralStack->setCurrentIndex(0);
    setSidebarsVisibleInternal(false, true);
}

void MainWindow::setupSidebars()
{
    auto *toolView = createToolView(
        nullptr,
        QStringLiteral("files_panel"),
        KMultiTabBar::Left,
        QIcon::fromTheme(QStringLiteral("folder")),
        i18n("Files")
    );

    m_fileExplorer = new FileExplorerPanel(toolView);
    m_fileExplorer->setMenuEventEmitter(m_menuEvents);
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
            if (doc)
                openFileInWorkspace(doc->relativePath());
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
        if (doc) openFileInWorkspace(doc->relativePath());
    });

    auto *propertiesView = createToolView(
        nullptr,
        QStringLiteral("properties_panel"),
        KMultiTabBar::Right,
        QIcon::fromTheme(QStringLiteral("document-properties")),
        i18n("Properties")
    );
    m_propertiesPanel = new PropertiesPanel(propertiesView);
    propertiesView->layout()->addWidget(m_propertiesPanel);

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
        auto *editor = activeEditor();
        if (!editor) return;
        editor->editor()->goToLine(lineNumber);
    });

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
        if (!confirmCloseUnsaved()) return;
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
    if (!confirmCloseUnsaved()) return;
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
        if (doc)
            openFileInWorkspace(doc->relativePath());
    }
}

void MainWindow::saveCurrentNote()
{
    if (!m_workspace) return;
    auto *leaf = m_workspace->activeLeaf();
    if (!leaf || !leaf->view()) return;

    // For MarkdownView the NoteDocument is the source of truth for
    // modification state. Save through NoteService which clears
    // NoteDocument::isModified() and notifies AutosaveReactor.
    auto *editor = activeEditor();
    if (editor && editor->noteDocument()) {
        m_vaultService->noteService()->saveNote(editor->noteDocument());
        return;
    }

    if (auto *tfv = qobject_cast<TextFileView *>(leaf->view()))
        tfv->saveImmediately();
}

void MainWindow::showCommandPalette()
{
    auto *bar = new KCommandBar(this);

    QList<KCommandBar::ActionGroup> groups;

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
    if (m_workspace) {
        recent = m_workspace->lastOpenFiles();
    }

    auto *switcher = new QuickSwitcher(m_vaultService->vault(), recent, this);

    QPoint topCenter = mapToGlobal(QPoint(width() / 2 - 300, 80));
    switcher->move(topCenter);

    connect(switcher, &QuickSwitcher::noteSelected,
            this, &MainWindow::onNoteActivated);
    connect(switcher, &QuickSwitcher::createNoteRequested,
            this, [this](const QString &name) {
        auto *doc = m_vaultService->noteService()->createNote(name, QString());
        if (doc) openFileInWorkspace(doc->relativePath());
    });

    switcher->show();
}

void MainWindow::onNoteActivated(const QString &relativePath)
{
    if (relativePath.endsWith(QStringLiteral(".canvas"))) {
        openFileInWorkspace(relativePath);
        return;
    }

    auto *doc = m_vaultService->noteService()->openNote(relativePath);
    if (doc) {
        openFileInWorkspace(relativePath);
    } else {
        QString name = relativePath;
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        QString folder;
        int lastSlash = name.lastIndexOf(QLatin1Char('/'));
        if (lastSlash >= 0) {
            folder = name.left(lastSlash);
            name = name.mid(lastSlash + 1);
        }
        doc = m_vaultService->noteService()->createNote(name, folder);
        if (doc)
            openFileInWorkspace(doc->relativePath());
    }
}

void MainWindow::onVaultOpened()
{
    auto *vault = m_vaultService->vault();

    m_centralStack->setCurrentIndex(1);
    setSidebarsVisible(true);

    updateWindowTitle();

    m_recentVaults->addUrl(QUrl::fromLocalFile(vault->path()));
    auto config = KSharedConfig::openConfig();
    KConfigGroup recentGroup = config->group(QStringLiteral("RecentVaults"));
    m_recentVaults->saveEntries(recentGroup);
    config->sync();

    delete m_treeModel;
    m_treeModel = new NotesTreeModel(vault, this);
    m_fileExplorer->setModel(m_treeModel);

    delete m_autosave;
    m_autosave = new AutosaveReactor(m_vaultService->noteService(), this);

    // TODO Q.0 P7: re-wire file-watcher through Vault. FileWatchReactor moved
    // to libs/vault as private Corbomite::detail::Watcher in Q.0 P2 T2.2;
    // Vault's public `created` / `modified` / `deletedFile` / `renamed`
    // signals (Q.0 P2 T2.4) will drive the TextFileView + MetadataCache
    // pipelines below once Phase 7 migrates them. For now the pipeline is
    // suppressed — external filesystem changes won't propagate to the UI.

    // Set file resolver so FileView::setState can load NoteDocuments
    m_viewRegistry->setFileResolver([this](const QString &relPath) -> NoteDocument * {
        if (!m_vaultService || !m_vaultService->isOpen())
            return nullptr;
        return m_vaultService->noteService()->openNote(relPath);
    });

    // LinkResolver
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

    // MetadataCache + SQLiteIndex
    delete m_metadataCache;
    m_metadataCache = new MetadataCache(*m_linkResolver, this);

    // Q.0 P6 — canonical Vault + FileManager. Runs alongside the legacy
    // VaultService/VaultModel pair; panels migrate onto these during this
    // phase.
    if (!m_fsAdapter) m_fsAdapter = std::make_unique<FileSystemAdapter>();
    delete m_fileManager;
    m_fileManager = nullptr;
    delete m_vaultObj;
    m_vaultObj = new Vault(m_fsAdapter.get(), this);
    m_vaultObj->load(vault->path());
    m_fileManager = new FileManager(m_vaultObj, m_metadataCache, this);

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

    QDir().mkpath(vault->configPath());
    m_metadataCache->open(vault->configPath() + QStringLiteral("/metadata-cache.db"));

    m_searchIndex->reconcileWithCache();

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

    m_backlinksPanel->setIndex(m_searchIndex);
    m_backlinksPanel->setMetadataCache(m_metadataCache);
    m_outlinksPanel->setIndex(m_searchIndex);
    m_outlinksPanel->setVault(m_vaultObj);
    m_outlinksPanel->setMetadataCache(m_metadataCache);
    m_localGraphPanel->setIndex(m_searchIndex);
    m_localGraphPanel->setVault(m_vaultObj);
    m_localGraphPanel->setMetadataCache(m_metadataCache);
    m_propertiesPanel->setMetadataCache(m_metadataCache);
    m_propertiesPanel->setVault(m_vaultObj);
    m_propertiesPanel->setFileManager(m_fileManager);

    // Update MetadataCache on note saves
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

    // TODO Q.0 P7: re-wire file-watcher signal consumers onto Vault::created
    // / Vault::modified / Vault::deletedFile / Vault::renamed (Q.0 P2 T2.4
    // introduces the producers). Previously these three blocks wired
    // FileWatchReactor's fileModifiedExternally / fileDeletedExternally /
    // fileCreatedExternally signals to TextFileView::onExternalModify and
    // MetadataCache::onFileChanged / onFileDeleted; gap accepted per plan.

    // Session manager — restore workspace
    delete m_sessionManager;
    m_sessionManager = new SessionManager(this);
    m_sessionManager->setSessionPath(vault->path() + QStringLiteral("/.obsidian/workspace.json"));
    m_sessionManager->load();

    const auto geometry = m_sessionManager->windowGeometry();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
    const auto windowStateBytes = m_sessionManager->windowState();
    if (!windowStateBytes.isEmpty()) restoreState(windowStateBytes);

    const auto sidebar = m_sessionManager->sidebarState();
    if (!sidebar.isEmpty()) {
        const bool leftVisible = sidebar.value(QStringLiteral("leftVisible")).toBool(true);
        setSidebarsVisibleInternal(leftVisible, true);
    }

    m_sessionManager->blockSaving();

    if (m_sessionManager->hasLoadedSession()) {
        QJsonObject wsLayout = m_sessionManager->workspaceLayout();
        if (!wsLayout.isEmpty()) {
            // Wrap in the full workspace JSON expected by Workspace::deserialize
            QJsonObject fullWs;
            fullWs[QStringLiteral("main")] = wsLayout;
            fullWs[QStringLiteral("active")] = m_sessionManager->activeLeafId();
            m_workspace->deserialize(fullWs);
        }
    }

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

    {
        FileSystemAdapter fs;
        VaultConfig vaultConfig(&fs, vault->path());
        m_dailyNoteService->initFromVaultConfig(vaultConfig);
    }

    updateVaultActions();
}

void MainWindow::onVaultClosed()
{
    saveSessionState();

    // Reset workspace to empty default layout. destroyTree() safely
    // releases all views before the vault destroys NoteDocuments.
    if (m_workspace)
        m_workspace->resetToDefaultLayout();

    // Clear file resolver
    if (m_viewRegistry)
        m_viewRegistry->setFileResolver(nullptr);

    delete m_autosave;
    m_autosave = nullptr;
    // FileWatchReactor teardown removed (Q.0 P2 T2.2).
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
    m_outlinksPanel->setVault(nullptr);
    m_outlinksPanel->setCurrentNote(nullptr);
    m_outlinePanel->setCurrentNote(nullptr);
    m_localGraphPanel->setIndex(nullptr);
    m_localGraphPanel->setMetadataCache(nullptr);
    m_localGraphPanel->setVault(nullptr);
    m_localGraphPanel->setCurrentNote(nullptr);
    m_propertiesPanel->setCurrentNote(nullptr);
    m_propertiesPanel->setFileManager(nullptr);
    m_propertiesPanel->setVault(nullptr);
    m_propertiesPanel->setMetadataCache(nullptr);

    if (m_embedRenderer) {
        m_embedRenderer->setMetadataCache(nullptr);
        m_embedRenderer->setResources(nullptr);
    }
    m_popoverResources.reset();

    if (m_metadataCache) {
        m_metadataCache->close();
    }
    // Q.0 P6 — FileManager references MetadataCache; tear down first.
    delete m_fileManager;
    m_fileManager = nullptr;
    if (m_vaultObj) m_vaultObj->unload();
    delete m_vaultObj;
    m_vaultObj = nullptr;
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

    m_centralStack->setCurrentIndex(0);
    m_welcomeScreen->refreshRecentVaults();
    setSidebarsVisibleInternal(false, true);

    updateWindowTitle();
    updateVaultActions();
}

void MainWindow::openGraphView()
{
    if (!m_vaultService->isOpen() || !m_searchIndex || !m_workspace) return;

    auto *tabs = m_workspace->activeTabs();
    if (!tabs) return;
    auto *leaf = m_workspace->createLeafInTabs(tabs);

    QJsonObject viewState;
    viewState[QStringLiteral("type")] = QStringLiteral("graph");
    viewState[QStringLiteral("state")] = QJsonObject{};
    leaf->setViewState(viewState);
    m_workspace->setActiveLeaf(leaf);

    // Wire graph controls if the view is a GraphViewTab
    if (auto *graphView = leaf->view() ? leaf->view()->findChild<GraphViewTab *>() : nullptr) {
        if (m_graphControlsPanel)
            graphView->setControlsPanel(m_graphControlsPanel);
        graphView->setMetadataCache(m_metadataCache);
    }
}

void MainWindow::showSearchPanel()
{
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

    auto *editor = activeEditor();
    if (!editor || !editor->noteDocument()) return;

    QString expanded = m_templateService->loadAndExpand(name, editor->noteDocument()->name());
    if (expanded.isEmpty()) return;

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
    if (doc)
        openFileInWorkspace(doc->relativePath());
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
