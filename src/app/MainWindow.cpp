// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "ActionContextController.h"
#include "WelcomeScreen.h"
#include "CorbomiteApp.h"
#include "editor/NoteEditorWidget.h"
#include "editor/CanvasMermaidAdapter.h"
// Leaf-agnostic consumption (Phase 1, contract v2): MainWindow dispatches
// every editor operation through the Markoff::MarkdownView base — no
// concrete leaf headers may appear in this file (Task 10 grep gate).
#include <markoff/core/MarkdownView.h>
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/Command.h"
#include "corbomite/core/EditableFileView.h"
#include "corbomite/core/EmptyView.h"
#include "corbomite/core/FileView.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/TextFileView.h"
#include <markoff/core/FindController.h>
#include "canvas/CanvasViewTab.h"
#include <canvas/CanvasDocument.h>
#include <canvas/CanvasScene.h>
#include "corbomite/bases/BasesView.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Plugin.h"
#include "corbomite/core/proxies/ViewRegistrar.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/PluginManager.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/VaultConfig.h"
#include "corbomite/core/NoteDocument.h"
#include "reactors/AutosaveReactor.h"
#include "SessionManager.h"

#include "corbomite/core/Command.h"
#include "corbomite/core/EditorSuggestManager.h"
#include <markoff/core/EmbedRegistry.h>
#include <markoff/core/CodeBlockProcessorRegistry.h>
#include "corbomite/core/PostProcessorRegistry.h"
#include "corbomite/core/MermaidRenderer.h"
#include "corbomite/core/StyledRenderEngine.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/View.h"
#include "corbomite/storage/markoff_adapters/Adapters.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "ExportToPdf.h"
#include "editor/MarkdownView.h"
#include "editor/MarkdownViewActions.h"
#include "canvas/CanvasFileView.h"
#include "canvas/CanvasViewActions.h"
#include "canvas/CanvasViewTab.h"
#include <canvas/CanvasAlignmentStrategy.h>
#include <canvas/CanvasScene.h>
#include <canvas/CanvasView.h>
#include "corbomite/core/HoverLinkSourceRegistry.h"
#include "corbomite/core/PathUtils.h"
#include "corbomite/core/StatusBarRegistry.h"
#include "corbomite/core/LucideIconRegistry.h"
#include "corbomite/core/DecorationProviderRegistry.h"
#include "corbomite/core/ProtocolHandlerRegistry.h"

#include <QDesktopServices>
#include "corbomite/core/MenuEventEmitter.h"
#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/VaultResourceProvider.h"
#include "editor/HoverPopover.h"
#include "editor/TagSuggest.h"
#include "editor/WikiLinkSuggest.h"
#include "dialogs/CalloutPickerDialog.h"
#include "dialogs/CreateVaultDialog.h"
#include "dialogs/InsertTableDialog.h"
#include "dialogs/Notice.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/QuickSwitcher.h"
#include "dialogs/TemplatePicker.h"
#include "RibbonToolBar.h"
#include "RibbonStateController.h"
#include "corbomite/models/TemplateService.h"
#include "corbomite/models/DailyNoteService.h"
#include "corbomitesettings.h"

#include <KAboutApplicationDialog>
#include <KAboutData>
#include "corbomite/core/ThemeService.h"

#include <KColorSchemeManager>
#include <KColorSchemeModel>

#include <KCommandBar>
#include <KLocalizedString>
#include <KStandardAction>
#include <KActionCollection>
#include <KMessageBox>
#include <KStandardGuiItem>
#include <KRecentFilesAction>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KToolBar>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QStatusBar>
#include <QActionGroup>
#include <QInputDialog>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextCursor>
#include <QTabBar>
#include <QScrollBar>

namespace Corbomite {

namespace {

class VaultScopedResources : public Corbomite::Core::VaultResourceProvider
{
public:
    explicit VaultScopedResources(Vault *vault) : m_vault(vault) {}

    QUrl resolveImage(const QString &name) const override
    {
        if (!m_vault) return {};
        const QString path = m_vault->basePath() + QLatin1Char('/') + name;
        if (QFileInfo::exists(path)) return QUrl::fromLocalFile(path);
        return {};
    }

    QByteArray loadImageBytes(const QString &name) const override
    {
        if (!m_vault) return {};
        const QString path = m_vault->basePath() + QLatin1Char('/') + name;
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
        const QString abs = m_vault->basePath() + QLatin1Char('/') + rel;
        QFile f(abs);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString::fromUtf8(f.readAll());
        }
        const QString filename =
            rel.mid(rel.lastIndexOf(QLatin1Char('/')) + 1);
        const auto files = m_vault->getMarkdownFiles();
        for (auto *tf : files) {
            if (!tf) continue;
            if (tf->path == filename
                || tf->path.endsWith(QLatin1Char('/') + filename)) {
                if (auto *doc = m_vault->cachedDocument(tf->path)) {
                    return doc->markdown();
                }
                QFile alt(m_vault->basePath() + QLatin1Char('/') + tf->path);
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
        return QUrl::fromLocalFile(m_vault->basePath() + QLatin1Char('/') + target);
    }

    bool wikiLinkExists(const QString &target) const override
    {
        return m_vault && m_vault->getAbstractFileByPath(target) != nullptr;
    }

private:
    Vault *m_vault;
};

// Walks: editor → its note document → its find controller. Null-safe at every
// step. Takes the editor directly (rather than the MainWindow) so this can
// live in the anonymous namespace without needing access to MainWindow's
// private accessors.
Markoff::FindController *findControllerFor(NoteEditorWidget *neWidget)
{
    if (!neWidget) return nullptr;
    auto *noteDoc = neWidget->noteDocument();
    if (!noteDoc) return nullptr;
    return noteDoc->findController();
}

} // namespace

MainWindow::MainWindow(CorbomiteApp *app, QWidget *parent)
    : CorbomiteMDI::MainWindow(parent)
    , m_app(app)
{
#ifdef CORBOMITE_DEV_BUILD
    setObjectName(QStringLiteral("CorbomiteMainWindowDev"));
    setComponentName(QStringLiteral("corbomite-dev"), i18n("Corbomite [Dev]"));
#else
    setObjectName(QStringLiteral("CorbomiteMainWindow"));
    setComponentName(QStringLiteral("corbomite"), i18n("Corbomite"));
#endif

    // Cluster O Phase O1.T1 — constructed before setupActions() populates
    // actionCollection() (the controller only looks actions up by name on
    // refresh(), so population order doesn't matter) and before
    // setupEditor() creates m_workspace (setWorkspace() is wired below,
    // right after setupEditor() returns).
    m_actionContext = new ActionContextController(actionCollection(), this);
    m_actionContext->setApp(m_app);

    setupActions();
    setupEditor();
    m_actionContext->setWorkspace(m_workspace);
    setupSidebars();
    setupStatusBar();

#ifdef CORBOMITE_DEV_BUILD
    setupGUI(Default, QStringLiteral("corbomite-devui.rc"));
#else
    setupGUI(Default, QStringLiteral("corbomiteui.rc"));
#endif

    // Ribbon must be created AFTER setupGUI — KXMLGUI's Default flag
    // rebuilds toolbars from the .rc file and would otherwise delete a
    // programmatically-added toolbar. Landing here also puts it
    // immediately to the right of the KXMLGUI-created main toolbar,
    // since addToolBar() appends in the same area in insertion order.
    setupRibbonToolBar();

    // Cluster O Phase O3 — ViewActions provider mechanism. Providers are
    // eagerly constructed (O3.T2 — the Hotkeys page needs every type's
    // shortcuts even with no matching tab open) and registered with the
    // controller; only *installation* is dynamic (O3.T3, driven by
    // ActionContextController::rebindActiveView() on every leaf/view-type
    // change). setGuiFactory() must come first — guiFactory() only exists
    // meaningfully once setupGUI() above has run. The toolbar, like the
    // Ribbon above, is created AFTER setupGUI() for the same reason
    // (KXMLGUI's Default flag would otherwise delete a programmatically-
    // added toolbar); registerToolBar()'s immediate applyToolBarPolicies()
    // call is therefore also the D4-trap reapply (nothing could have
    // silently overridden this toolbar's visibility before it existed).
    m_actionContext->setGuiFactory(guiFactory());

    m_markdownViewActions = new MarkdownViewActions(this, this);
    connect(m_markdownViewActions, &MarkdownViewActions::insertTemplateRequested,
            this, &MainWindow::insertTemplate);
    m_actionContext->registerProvider(m_markdownViewActions);

    m_markdownToolBar = new KToolBar(QStringLiteral("markdownToolBar"), this);
    m_markdownToolBar->setWindowTitle(i18n("Markdown Toolbar"));
    addToolBar(Qt::TopToolBarArea, m_markdownToolBar);
    m_markdownToolBar->addActions(m_markdownViewActions->toolBarActions());
    m_actionContext->registerToolBar(QStringLiteral("markdown"), m_markdownToolBar);

    // Cluster O Phase O4 — CanvasViewActions, same eager-construct /
    // register-provider / register-toolbar shape as markdown above.
    m_canvasViewActions = new CanvasViewActions(this);
    m_actionContext->registerProvider(m_canvasViewActions);

    m_canvasToolBar = new KToolBar(QStringLiteral("canvasToolBar"), this);
    m_canvasToolBar->setWindowTitle(i18n("Canvas Toolbar"));
    addToolBar(Qt::TopToolBarArea, m_canvasToolBar);
    m_canvasToolBar->addActions(m_canvasViewActions->toolBarActions());
    m_actionContext->registerToolBar(QStringLiteral("canvas"), m_canvasToolBar);

    // Cluster V Task 1.7 — theme dispatcher. applyTheme() applies the
    // Appearance/Theme kcfg key via KColorSchemeManager; onSettingsApplied
    // is the choke point reused by future appliers (V.2 autosave-delay, etc.)
    applyTheme();
    connect(CorbomiteSettings::self(), &KConfigSkeleton::configChanged,
            this, &MainWindow::onSettingsApplied);

    // C2 — ThemeService owns the active Markoff theme + the registry of
    // installed themes. Constructed after applyTheme() so KColorScheme is
    // initialised before SystemThemeBuilder reads it on first build.
    // onSettingsApplied() refreshes the system theme when the KDE color
    // scheme changes.
    m_themeService = new Corbomite::Core::ThemeService(
        KColorSchemeManager::instance(), this);
    {
        const QString persisted = CorbomiteSettings::self()->markoffTheme();
        if (!persisted.isEmpty())
            m_themeService->setActiveThemeByName(persisted);
    }
    connect(CorbomiteSettings::self(), &KConfigSkeleton::configChanged,
            m_themeService, &Corbomite::Core::ThemeService::refreshSystemTheme);

    connect(m_app, &CorbomiteApp::vaultOpened, this, &MainWindow::onVaultOpened);
    connect(m_app, &CorbomiteApp::vaultClosed, this, &MainWindow::onVaultClosed);

    if (auto *pm = m_app->pluginManager()) {
        // Wire host <-> plugin lifecycle callbacks BEFORE tearing down any
        // stale plugin instances from a prior MainWindow — that way every
        // disablePlugin() routes through releasePluginView() and tool-view
        // cleanup runs on a consistent path. (Previously these connects
        // ran after the teardown loop, which worked only by accident:
        // there are no hosted tool views yet in a fresh constructor. The
        // symmetric wiring removes that landmine for any future caller
        // who triggers teardown from a less-pristine state.)
        connect(pm, &Corbomite::PluginManager::pluginLoaded,
                this, &MainWindow::hostPluginView);
        connect(pm, &Corbomite::PluginManager::pluginUnloading,
                this, &MainWindow::releasePluginView);
        connect(pm, &Corbomite::PluginManager::pluginLoadFailed,
                this, [](const QString &id, const QString &reason) {
            Notice::post(i18n("Plugin \"%1\" failed to load: %2", id, reason));
        });
        // Detach any workspace leaves whose view type was registered by
        // the unloading plugin, before its ViewRegistrar destructor
        // unregisters the factories. Mirrors Obsidian's
        // detachLeavesOfType invocation in plugin teardown.
        // Skipped during shutdown: MainWindow's destructor deletes
        // m_vaultObj before disabling plugins, so by the time this
        // lambda fires from inside ~MainWindow the leaves' FileViews
        // hold dangling NoteDocument pointers that would crash on
        // getViewState. The active-vault gate keeps the runtime path
        // (user disables a plugin via Settings while a vault is open)
        // working; teardown doesn't need detach because every leaf
        // is about to die anyway.
        connect(pm, &Corbomite::PluginManager::pluginUnloading,
                this, [this, pm](const QString &id) {
            if (!m_workspace || !m_vaultObj) return;
            const auto *info = pm->pluginById(id);
            if (!info || !info->context) return;
            auto *views = info->context->views();
            if (!views) return;
            const QStringList types = views->registeredTypes();
            for (const QString &type : types)
                m_workspace->detachLeavesOfType(type);
        });

        QStringList stale;
        for (int i = 0; i < pm->pluginCount(); ++i) {
            const auto &info = pm->pluginByIndex(i);
            if (info.instance) stale.append(info.metaData.base().pluginId());
        }
        for (const QString &id : stale)
            pm->disablePlugin(id, /*persist=*/false);
    }

    m_commandRegistry = new CommandRegistry();

    // Cluster R Task 3.2: register `markdown:add-metadata-property` so
    // MarkdownView.onMoreOptionsMenu's "Add file property" action can dispatch
    // without reaching back into MainWindow directly.
    {
        Command addProp;
        addProp.id = QStringLiteral("markdown:add-metadata-property");
        addProp.name = i18n("Add file property");
        addProp.icon = QStringLiteral("list-add");
        addProp.callback = [this] {
            if (auto *mv = activeMarkdownView())
                mv->insertFrontmatterProperty();
        };
        m_commandRegistry->addCommand(addProp);
    }

    // Bug #1 (2026-04-24, commit 9fb2fe47) — per-view hamburger menus
    // ("…" overflow in view headers) dispatch `split_right`/`split_down`
    // through m_commandRegistry, but these ids only lived in
    // KActionCollection (wired in setupActions()). executeById returned
    // false silently. Bridge the two surfaces here by registering
    // CommandRegistry entries whose checkCallback delegates to the
    // KAction's trigger() — keeps a single source of truth for the
    // vault-open gate applied by updateVaultActions().
    {
        const auto bindToAction = [this](const QString &id,
                                         const QString &label,
                                         const QString &icon) {
            Command c;
            c.id = id;
            c.name = label;
            c.icon = icon;
            c.checkCallback = [this, id](bool checking) -> bool {
                auto *ac = actionCollection();
                auto *act = ac ? ac->action(id) : nullptr;
                if (!act || !act->isEnabled()) return false;
                if (!checking) act->trigger();
                return true;
            };
            m_commandRegistry->addCommand(c);
        };
        bindToAction(QStringLiteral("split_right"),
                     i18n("Split Right"),
                     QStringLiteral("view-split-left-right"));
        bindToAction(QStringLiteral("split_down"),
                     i18n("Split Down"),
                     QStringLiteral("view-split-top-bottom"));
    }

    m_menuEvents = new MenuEventEmitter(this);
    m_hoverSources = new HoverLinkSourceRegistry(this);
    m_hoverSources->registerBuiltins();
    m_hoverPopover = new HoverPopover(this);
    // Vault binding deferred to onVaultOpened — no vault exists yet.

    m_embedRegistry = std::make_unique<Markoff::EmbedRegistry>();
    m_mermaidRenderer = std::make_unique<Corbomite::Core::MermaidRenderer>();
    // P5.4 canvas mermaid seam adapter — wraps m_mermaidRenderer.
    m_canvasMermaidAdapter = std::make_unique<Corbomite::CanvasMermaidAdapter>(m_mermaidRenderer.get());
    m_cardRenderEngine = std::make_unique<Corbomite::StyledRenderEngine>();
    // Hover preview (2026-06-11) — reuse the canvas-card render engine; it is
    // stateless and read-only. Per-vault resources are set in onVaultOpened.
    m_hoverPopover->setRenderEngine(m_cardRenderEngine.get());

    m_suggestManager = new EditorSuggestManager(this);
    // Suggesters start nullptr-bound; MainWindow rebinds on vault
    // open/close via their setters.
    m_wikiSuggest = new WikiLinkSuggest(nullptr);
    m_tagSuggest = new TagSuggest(nullptr);
    m_suggestManager->registerSuggest(m_wikiSuggest);
    m_suggestManager->registerSuggest(m_tagSuggest);

    // Cluster B Phase 1 — host-wide plugin extension registries.
    m_pluginPostProcessors = std::make_unique<Corbomite::Core::PostProcessorRegistry>();
    m_pluginCodeBlocks = std::make_unique<Markoff::CodeBlockProcessorRegistry>();

    // Cluster B Phase 3.2 — wire corbomite:// URL routing through
    // QDesktopServices. obsidian:// is opt-in via Settings (deferred
    // follow-up: persist the toggle and call setUrlHandler on toggle).
    QDesktopServices::setUrlHandler(
        QStringLiteral("corbomite"),
        &Corbomite::ProtocolHandlerRegistry::instance(),
        "dispatch");

    if (m_actionContext) m_actionContext->refresh();
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    // Canvas views hold a raw pointer to m_cardRenderEngine (a unique_ptr
    // member). Member destructors run BEFORE the base ~QObject tears down the
    // child Workspace + view tree, so the engine would be freed first, leaving
    // each CanvasFileView/CanvasScene with a dangling engine pointer during its
    // own teardown. Clear it now — while the engine is still alive — so nothing
    // can dereference a freed engine. (Reordering the member can't fix this:
    // QObject children always outlive every member destructor.)
    // (m_hoverPopover also holds a non-owning pointer to the same engine, but
    // it only dereferences it from renderTarget() during live hover — never
    // during teardown — so it needs no equivalent clear here.)
    if (m_workspace) {
        for (auto *leaf : m_workspace->allLeaves()) {
            auto *view = leaf ? leaf->view() : nullptr;
            if (auto *cv = qobject_cast<CanvasFileView *>(view))
                cv->setRenderEngine(nullptr);
        }
    }

    delete m_autosave;
    m_autosave = nullptr;

    // FileWatchReactor teardown removed (Q.0 P2 T2.2).

    delete m_sessionManager;
    m_sessionManager = nullptr;

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

    // Cluster R Task 3.1 aftershock: plugin CommandRegistrars hold a raw
    // CommandRegistry* that must still be alive when they unload. Disable
    // every loaded plugin before deleting the registry so each plugin's
    // PluginContext (and its CommandRegistrar child) tears down against a
    // live registry. `persist=false` keeps the user's enabled choice in
    // KConfig untouched across restarts.
    if (m_app) {
        if (auto *pm = m_app->pluginManager()) {
            for (int i = 0; i < pm->pluginCount(); ++i) {
                const auto &info = pm->pluginByIndex(i);
                if (info.instance) {
                    pm->disablePlugin(info.metaData.base().pluginId(),
                                      /*persist=*/false);
                }
            }
        }
    }

    delete m_commandRegistry;
    m_commandRegistry = nullptr;

    // TODO(port): m_embedRenderer field disabled — see MainWindow.h.
    m_embedRegistry.reset();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_app->isOpen()) {
        if (!confirmCloseUnsaved()) {
            event->ignore();
            return;
        }
        saveSessionState();
    }

    CorbomiteMDI::MainWindow::closeEvent(event);
}

// --- Helper methods ---

void MainWindow::triggerEditorAction(Markoff::ActionId id)
{
    auto *editor = activeEditor();
    if (!editor) return;

    // TODO(port-foundation-exploration): find/replace dispatch retired —
    // MarkdownView::showFindBar/showReplaceBar virtuals removed by find-
    // session-scope. Find is now consumer-owned via Markoff::FindController +
    // attachFindController; reimplement here as part of the find UI port
    // (the actual next-step feature work). Replace flow deferred to a later
    // micro-spec.
    //
    // For now: no-op on find/replace IDs.
    (void)id;
}

// Cluster O Phase O3.T6 — onSetHeading/onInsertCallout/onInsertTable moved
// into MarkdownViewActions (they now operate on the bound MarkdownView
// instead of MainWindow's activeEditor()/activeMarkdownView() accessors).

// Cluster O Phase O1.T1 — refreshEditorActions()/updateEditorActionStates()
// (which used to disagree — see O1.T6) moved into
// ActionContextController::updateMarkdownActionStates(). onEditorContextChanged
// stays here as a thin public-slot forwarder: it is Phase C6's documented test
// seam (tests can invoke it with a synthetic EditorContext without a live
// Markoff editor), but the actual heading-radio sync + Tier-B refresh now
// live on the controller, which also owns the live wiring (see
// ActionContextController::rebindActiveView()).
void MainWindow::onEditorContextChanged(const Markoff::EditorContext &ctx)
{
    if (m_actionContext) m_actionContext->onEditorContextChanged(ctx);
}

void MainWindow::connectEditorContextMenu(NoteEditorWidget *editor)
{
    if (!editor) return;
    // TODO(port-foundation-exploration): aboutToShowContextMenu signal lived
    // on the old Markoff::Editor. Context-menu wiring needs porting against
    // Live::EditorWidget's eventual context-menu surface (LiveContextMenu
    // Handler is already in the new live leaf; bridge to it as a follow-up).
    (void)editor;
}

void MainWindow::onAboutToShowContextMenu(QMenu *menu,
                                          const Markoff::EditorContext &ctx,
                                          const QPoint & /*globalPos*/)
{
    // TODO(port-foundation-exploration): paired with connectEditorContextMenu.
    (void)menu;
    (void)ctx;
}

MarkdownView *MainWindow::activeMarkdownView() const
{
    if (!m_workspace || !m_workspace->activeLeaf())
        return nullptr;
    return qobject_cast<MarkdownView *>(m_workspace->activeLeaf()->view());
}

Corbomite::Bases::BasesView *MainWindow::activeBasesView() const
{
    if (!m_workspace || !m_workspace->activeLeaf())
        return nullptr;
    return qobject_cast<Corbomite::Bases::BasesView *>(
        m_workspace->activeLeaf()->view());
}

CanvasFileView *MainWindow::activeCanvasView() const
{
    if (!m_workspace || !m_workspace->activeLeaf())
        return nullptr;
    return qobject_cast<CanvasFileView *>(m_workspace->activeLeaf()->view());
}

NoteEditorWidget *MainWindow::activeEditor() const
{
    auto *mv = activeMarkdownView();
    return mv ? mv->editorWidget() : nullptr;
}

void MainWindow::onFind()
{
    // Cluster O Phase O1.T5 — Bases already owns a search box in its
    // toolbar; route edit_find there instead of silently doing nothing
    // (report §3.2/§4.6). Only edit_find has a bases-side equivalent —
    // replace/find-next/find-prev stay markdown-only (disabled elsewhere
    // by ActionContextController::updateFindAndTemplateActions()).
    if (auto *bv = activeBasesView()) {
        bv->focusSearch();
        return;
    }
    auto *neWidget = activeEditor();
    if (!neWidget) return;
    neWidget->showFindBar();
}

void MainWindow::onReplace()
{
    auto *neWidget = activeEditor();
    if (!neWidget) return;
    neWidget->showReplaceBar();
}

void MainWindow::onFindNext()
{
    if (auto *fc = findControllerFor(activeEditor())) fc->findNext();
}

void MainWindow::onFindPrev()
{
    if (auto *fc = findControllerFor(activeEditor())) fc->findPrevious();
}

// Zoom keys are editor-owned: Markoff installs window-level Shortcuts (Ctrl+= /
// Ctrl++ / Ctrl+Shift+= / Ctrl+- / Ctrl+0) in LiveView.qml as "editor-internal
// viewport concerns the host has no opinion about." Corbomite therefore does
// NOT bind those keys (doing so caused a KActionCollection ambiguity →
// "Ctrl+= is ambiguous, no action triggered"). The View-menu zoom items
// dispatch through the polymorphic View::zoomIn/Out/Reset virtuals (Cluster O
// Phase O1.T3) — MarkdownView/CanvasFileView/GraphView each override onto
// their own real viewport. This retires the activeEditor() special-case that
// used to mean canvas and graph had no zoom action at all.
void MainWindow::onZoomIn()
{
    if (m_workspace && m_workspace->activeLeaf() && m_workspace->activeLeaf()->view())
        m_workspace->activeLeaf()->view()->zoomIn();
}

void MainWindow::onZoomOut()
{
    if (m_workspace && m_workspace->activeLeaf() && m_workspace->activeLeaf()->view())
        m_workspace->activeLeaf()->view()->zoomOut();
}

void MainWindow::onZoomReset()
{
    if (m_workspace && m_workspace->activeLeaf() && m_workspace->activeLeaf()->view())
        m_workspace->activeLeaf()->view()->zoomReset();
}

void MainWindow::onAboutApp()
{
    KAboutApplicationDialog dlg(KAboutData::applicationData(), this);
    dlg.exec();
}

// Cluster O Phase O3.T6 — cycleEditorMode() moved into MarkdownViewActions
// (editor_toggle_mode's trigger).

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

    auto *leaf = m_workspace->createLeafInActiveGroup();
    if (!leaf) return;

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
    // Tier 1 (Obsidian-schema workspace.json): full-fidelity, written
    // directly by Workspace — main/active/floating/lastOpenFiles plus
    // passthrough of any Obsidian key Workspace doesn't itself model.
    if (m_workspace && m_vaultObj && m_vaultObj->isLoaded()) {
        m_workspace->writeWorkspaceJson(m_vaultObj->basePath());
    }

    // Tiers 2/3 (Corbomite-native vault-portable + machine-local state).
    if (!m_sessionManager) return;
    m_sessionManager->blockSaving();
    m_sessionManager->saveWindowGeometry(saveGeometry(), saveState());
    // D4: real pixel widths from CorbomiteMDI (Sidebar::lastSize),
    // replacing the previous hardcoded 200/false/200. Left/Right share one
    // visibility toggle at the CorbomiteMDI level (setSidebarsVisibleInternal
    // hides/shows all sidebars together) — there's no independent
    // right-sidebar visibility to persist, so both pass sidebarsVisible().
    m_sessionManager->saveSidebarState(
        sidebarsVisible(), sidebarWidth(KMultiTabBar::Left),
        sidebarsVisible(), sidebarWidth(KMultiTabBar::Right));
    // Expanded-folder persistence moved into FileExplorer plugin
    // (Cluster Q Task 18); follow-up: surface a plugin-side helper
    // that SessionManager can query via WorkspaceController.
    m_sessionManager->unblockSaving();
    m_sessionManager->saveNow();
}

void MainWindow::rewirePluginCoreServices()
{
    auto *pm = m_app->pluginManager();
    if (!pm) return;
    pm->setContextConfigurator([this](Corbomite::PluginContext *ctx) {
        ctx->setCoreServices(m_vaultObj, m_fileManager, m_metadataCache,
                              m_searchIndex, m_workspace, m_commandRegistry,
                              m_viewRegistry, m_menuEvents,
                              nullptr /* QNetworkAccessManager */);
        ctx->setExtensionRegistries(m_hoverSources, m_suggestManager,
                                       m_pluginPostProcessors.get(),
                                       m_ribbonToolBar,
                                       m_embedRegistry.get(),
                                       m_pluginCodeBlocks.get(),
                                       m_statusBarRegistry,
                                       &Corbomite::LucideIconRegistry::instance(),
                                       &Corbomite::DecorationProviderRegistry::instance(),
                                       &Corbomite::ProtocolHandlerRegistry::instance());
        if (m_vaultObj && m_vaultObj->isLoaded()) {
            const QString dir = m_vaultObj->basePath()
                              + QLatin1Char('/') + m_vaultObj->configDir()
                              + QStringLiteral("/plugins/")
                              + ctx->metaData().base().pluginId();
            QDir().mkpath(dir);
            ctx->setPluginDataDir(dir);
        }
    });
    // Plugins already enabled (e.g. on subsequent vault open) get their
    // contexts re-wired in place so proxies see the fresh services.
    for (int i = 0; i < pm->pluginCount(); ++i) {
        const auto &info = pm->pluginByIndex(i);
        if (info.context) {
            info.context->setCoreServices(m_vaultObj, m_fileManager,
                m_metadataCache, m_searchIndex, m_workspace, m_commandRegistry,
                m_viewRegistry, m_menuEvents, nullptr);
            info.context->setExtensionRegistries(m_hoverSources,
                m_suggestManager, m_pluginPostProcessors.get(),
                m_ribbonToolBar, m_embedRegistry.get(),
                m_pluginCodeBlocks.get(),
                m_statusBarRegistry,
                &Corbomite::LucideIconRegistry::instance(),
                &Corbomite::DecorationProviderRegistry::instance(),
                &Corbomite::ProtocolHandlerRegistry::instance());
            if (m_vaultObj && m_vaultObj->isLoaded()) {
                const QString dir = m_vaultObj->basePath()
                                  + QLatin1Char('/') + m_vaultObj->configDir()
                                  + QStringLiteral("/plugins/")
                                  + info.metaData.base().pluginId();
                QDir().mkpath(dir);
                info.context->setPluginDataDir(dir);
            }
        }
    }
}

void MainWindow::hostPluginView(const QString &pluginId)
{
    auto *pm = m_app->pluginManager();
    if (!pm) return;
    auto *info = pm->pluginById(pluginId);
    if (!info || !info->instance) return;

    QObject *viewObj = info->instance->createView(this);
    auto *widget = qobject_cast<QWidget *>(viewObj);
    if (!widget) {
        if (viewObj) viewObj->deleteLater();
        return;
    }

    const QJsonObject raw = info->metaData.base().rawData();
    const QString dockArea = raw.value(QStringLiteral("X-Corbomite-DockArea"))
                                 .toString(QStringLiteral("right"));
    const QString dockTitle = raw.value(QStringLiteral("X-Corbomite-DockTitle"))
                                  .toString(info->metaData.base().name());
    const QString dockIcon = raw.value(QStringLiteral("X-Corbomite-DockIcon"))
                                  .toString(info->metaData.base().iconName());

    KMultiTabBar::KMultiTabBarPosition pos =
        (dockArea == QLatin1String("left")) ? KMultiTabBar::Left
                                              : KMultiTabBar::Right;

    const QString toolViewId = pluginId + QStringLiteral("_panel");
    auto *toolView = createToolView(
        nullptr,
        toolViewId,
        pos,
        QIcon::fromTheme(dockIcon.isEmpty() ? QStringLiteral("preferences-plugin")
                                              : dockIcon),
        dockTitle);
    if (!toolView || !toolView->layout()) {
        // MDI refused the slot — most likely an identifier collision with
        // a prior tool view that hasn't been released yet (see
        // CorbomiteMDI::createToolView). Log so the next occurrence isn't
        // silent; drop the orphaned plugin widget.
        qWarning() << "MainWindow::hostPluginView: MDI refused tool view for"
                   << pluginId << "(id=" << toolViewId
                   << ") — widget discarded";
        widget->deleteLater();
        return;
    }
    toolView->setObjectName(toolViewId);
    widget->setParent(toolView);
    toolView->layout()->addWidget(widget);
    m_hostedPluginViews.insert(pluginId, widget);

    // Restore persisted per-plugin session state (tree expand, etc). Runs
    // after the widget is parented so model-bound view restoration sees
    // the final widget hierarchy.
    if (m_sessionManager) {
        const QJsonObject state = m_sessionManager->pluginSessionState(pluginId);
        if (!state.isEmpty()) {
            info->instance->loadSessionState(widget, state);
        }
    }
}

void MainWindow::releasePluginView(const QString &pluginId)
{
    auto it = m_hostedPluginViews.find(pluginId);
    if (it == m_hostedPluginViews.end()) return;
    QWidget *widget = it.value();
    m_hostedPluginViews.erase(it);

    // Persist per-plugin session state before the view is torn down.
    if (widget && m_sessionManager && m_app) {
        if (auto *pm = m_app->pluginManager()) {
            if (const auto *info = pm->pluginById(pluginId)) {
                if (info->instance) {
                    const QJsonObject state =
                        info->instance->saveSessionState(widget);
                    m_sessionManager->setPluginSessionState(pluginId, state);
                }
            }
        }
    }

    if (!widget) return;
    QWidget *toolView = widget->parentWidget();
    // Destroy synchronously so the next hostPluginView for the same
    // plugin id (e.g. on vault switch) doesn't collide with a still-alive
    // tool view of the same identifier. A queued deleteLater would make
    // CorbomiteMDI::MainWindow::createToolView see the old instance and
    // refuse to host the new one, leaving sidebars empty until the next
    // relayout that never comes.
    delete (toolView ? toolView : widget);
}

void MainWindow::propagateServicesToView(View *view)
{
    if (!view) return;

    // Every EditableFileView leaf gets the universal file-menu callbacks
    // so its hamburger + tab-header rename routes through FileManager's
    // promptForFileRename/Move/Deletion modals. Core has no vault deps,
    // so the hookup is via std::function injections bound here.
    if (auto *efv = qobject_cast<EditableFileView *>(view)) {
        auto *fm = m_fileManager;
        auto *vaultObj = m_vaultObj;
        auto *cmds = m_commandRegistry;

        const auto resolveAbs = [vaultObj](NoteDocument *doc) -> QString {
            if (!vaultObj || !doc) return QString();
            const QString base = vaultObj->basePath();
            if (base.isEmpty()) return QString();
            return base + QLatin1Char('/') + doc->relativePath();
        };
        const auto resolveTFile = [vaultObj](NoteDocument *doc) -> TAbstractFile * {
            if (!vaultObj || !doc) return nullptr;
            return vaultObj->getAbstractFileByPath(doc->relativePath());
        };

        efv->setRenameCallback(
            [fm, resolveTFile](NoteDocument *doc, QWidget *parent) {
                if (!fm) return;
                if (auto *f = resolveTFile(doc))
                    fm->promptForFileRename(f, parent);
            });
        efv->setMoveCallback(
            [fm, resolveTFile](NoteDocument *doc, QWidget *parent) {
                if (!fm) return;
                if (auto *f = resolveTFile(doc))
                    fm->promptForMove(f, parent);
            });
        efv->setDeleteCallback(
            [fm, resolveTFile](NoteDocument *doc, QWidget *parent) {
                if (!fm) return;
                if (auto *f = resolveTFile(doc))
                    fm->promptForDeletion(f, parent);
            });
        // Cluster S task 3.2: route the per-view "Bookmark…" hamburger entry
        // through the bookmarks plugin's Q_INVOKABLE modal slot. Guarded by
        // plugin presence — when the plugin is disabled the callback stays
        // unset and EditableFileView grays the menu entry out.
        if (auto *pm = m_app ? m_app->pluginManager() : nullptr) {
            if (const auto *info = pm->pluginById(QStringLiteral("corbomite-bookmarks"))) {
                if (auto *instance = info->instance) {
                    efv->setBookmarkCallback(
                        [instance](NoteDocument *doc, QWidget *parent) {
                            if (!doc) return;
                            QMetaObject::invokeMethod(instance,
                                "openBookmarkModalForFile",
                                Q_ARG(QString, doc->relativePath()),
                                Q_ARG(QWidget *, parent));
                        });
                }
            }
        }
        efv->setVaultAbsolutePathResolver(resolveAbs);
        efv->setVaultNameResolver([vaultObj]() -> QString {
            if (!vaultObj) return QString();
            return QFileInfo(vaultObj->basePath()).fileName();
        });
        efv->setCommandDispatcher([cmds](const QString &commandId) {
            if (cmds) cmds->executeById(commandId);
        });
    }

    if (auto *mv = qobject_cast<MarkdownView *>(view)) {
        if (m_app->isOpen())
            mv->setVault(m_vaultObj);
        mv->setHoverPopover(m_hoverPopover);
        mv->setEditorSuggestManager(m_suggestManager);

        // Cluster R Task 3.4: inject command-dispatch + PDF-export wiring so
        // the hamburger menu actions can reach the host registry + Vault.
        auto *cmds = m_commandRegistry;
        mv->setMarkdownCommandDispatcher(
            [cmds](const QString &commandId) {
                if (cmds) cmds->executeById(commandId);
            });
        auto *vaultObj = m_vaultObj;
        mv->setPdfExportTrigger([this, vaultObj](QWidget *parent) {
            auto *mv2 = qobject_cast<MarkdownView *>(parent);
            auto *doc = mv2 ? mv2->file() : nullptr;
            if (!doc || !vaultObj) return;
            auto *tfile = vaultObj->getFileByPath(doc->relativePath());
            if (!tfile) return;
            Corbomite::ExportToPdf::exportFile(tfile, vaultObj, parent);
        });
        mv->setFindTrigger([this](QWidget *) { onFind(); });
        mv->setReplaceTrigger([this](QWidget *) { onReplace(); });

        auto *editor = mv->editorWidget();
        if (editor) {
            // P5.4 seams — mermaid rendering + embed dispatch on the canvas
            // LivePreview leaf.
            editor->setMermaidRenderer(m_canvasMermaidAdapter.get());
            editor->setEmbedRegistry(m_embedRegistry.get());

            // Task 0.2: NoteEditorWidget::linkActivated now fires (via
            // DefaultLinkService wired in NoteEditorWidget ctor + Reading
            // construction). Resolve the raw wikilink target through
            // LinkResolver for proper vault-wide disambiguation before
            // dispatching to onNoteActivated.
            if (!editor->property("_mw_linkact").toBool()) {
                editor->setProperty("_mw_linkact", true);
                connect(editor, &NoteEditorWidget::linkActivated,
                        this, [this, editor](const QString &rawTarget, bool openInNewTab) {
                    if (rawTarget.isEmpty()) return;
                    // Plain click (openInNewTab == false) navigates the
                    // currently active leaf in place; an explicit
                    // middle-click (true) keeps the existing
                    // open-or-switch-to-new-leaf behavior. See
                    // navigateActiveLeafTo's doc comment (MainWindow.h).
                    const auto dispatch = [this, openInNewTab](const QString &path) {
                        if (openInNewTab) onNoteActivated(path);
                        else navigateActiveLeafTo(path);
                    };
                    // If already a vault-relative path (has .md / .canvas),
                    // dispatch directly; otherwise resolve via LinkResolver.
                    if (rawTarget.endsWith(QStringLiteral(".canvas"))) {
                        dispatch(rawTarget);
                        return;
                    }
                    if (m_linkResolver) {
                        const QString fromCtx = editor->noteDocument()
                            ? editor->noteDocument()->relativePath()
                            : QString{};
                        const auto resolved = m_linkResolver->resolve(fromCtx, rawTarget);
                        if (resolved.resolved && !resolved.path.isEmpty()) {
                            dispatch(resolved.path);
                            return;
                        }
                    }
                    // Fall back: append .md and let the dispatch target
                    // create the note if it does not exist.
                    dispatch(rawTarget.endsWith(QStringLiteral(".md"))
                                 ? rawTarget
                                 : rawTarget + QStringLiteral(".md"));
                });
            }
            // Cluster K: canvas leaf's inline title band doubles as a
            // rename affordance. Property-guarded like linkActivated above
            // — this block re-runs on every leaf re-attach.
            if (!editor->property("_mw_titlerename").toBool()) {
                editor->setProperty("_mw_titlerename", true);
                connect(editor, &NoteEditorWidget::titleRenameRequested,
                        this, [this, editor](const QString &newTitle) {
                    if (!m_fileManager || !editor->noteDocument()) return;
                    const QString trimmed = newTitle.trimmed();
                    // The band is plain text with no character filtering
                    // (View::handleTitleKeyPress accepts anything) — a
                    // literal '/' would silently turn this into a folder
                    // move via renameFileByPath's path-join below, which is
                    // not what typing a slash into a title means. Reject
                    // rather than guess a substitution.
                    if (trimmed.isEmpty() || trimmed.contains(QLatin1Char('/'))) return;
                    const QString oldRel = editor->noteDocument()->relativePath();
                    const QString ext = QFileInfo(oldRel).suffix();
                    QString folder = QFileInfo(oldRel).path();
                    if (folder == QStringLiteral(".")) folder.clear();
                    QString newRel = folder.isEmpty() ? trimmed : folder + QStringLiteral("/") + trimmed;
                    if (!ext.isEmpty()) newRel += QStringLiteral(".") + ext;
                    if (newRel == oldRel) return;
                    // Silent no-op on collision (Vault::rename's own
                    // contract) — matches the band re-syncing to the actual
                    // on-disk name on the next syncInlineTitleForCanvas
                    // call (NoteDocument::pathChanged only fires on an
                    // actual rename, so a failed one leaves the band
                    // showing the rejected text until the next attach/
                    // detach; acceptable for a first pass, no crash/data
                    // loss risk either way).
                    m_fileManager->renameFileByPath(oldRel, newRel);
                });
            }
            connect(editor, &NoteEditorWidget::cursorInfoChanged,
                    this, &MainWindow::onCursorInfoChanged, Qt::UniqueConnection);
            // Guard with property — Qt::UniqueConnection doesn't work for lambdas.
            if (!editor->property("_mw_viewmode").toBool()) {
                editor->setProperty("_mw_viewmode", true);
                connect(editor, &NoteEditorWidget::viewModeChanged,
                        this, [this](NoteEditorWidget::ViewMode) {
                    if (m_actionContext) m_actionContext->refresh();
                });
            }
        }
        return;
    }

    if (auto *cv = qobject_cast<CanvasFileView *>(view)) {
        // Cluster R Task 3.6 — hamburger-menu command dispatcher.
        auto *cmds = m_commandRegistry;
        cv->setCanvasCommandDispatcher([cmds](const QString &commandId) {
            if (cmds) cmds->executeById(commandId);
        });
        // Feed card rendering from the headless styled renderer (this is the
        // setRenderEngine chain's only production caller).
        cv->setRenderEngine(m_cardRenderEngine.get());
        return;
    }

    if (auto *bv = qobject_cast<Corbomite::Bases::BasesView *>(view)) {
        bv->setServices(m_vaultObj, m_metadataCache, m_fileManager);
        // Bases Phase 2 — `this` in formulas should track the active leaf's
        // file. Subscribe to Workspace::activeLeafChanged and re-resolve
        // through the live Vault on each transition; also seed once with
        // whatever leaf is currently active.
        if (m_workspace && m_vaultObj) {
            auto refresh = [this, bv](WorkspaceLeaf *leaf) {
                if (!leaf || !m_vaultObj) {
                    bv->setCurrentFile(nullptr);
                    return;
                }
                const QJsonObject vs = leaf->getViewState();
                const QString path = vs.value(QStringLiteral("state"))
                                          .toObject()
                                          .value(QStringLiteral("file"))
                                          .toString();
                bv->setCurrentFile(path.isEmpty()
                                       ? nullptr
                                       : m_vaultObj->getFileByPath(path));
            };
            connect(m_workspace, &Workspace::activeLeafChanged, bv, refresh);
            refresh(m_workspace->activeLeaf());
        }
        bv->setOpenInNewTabHandler([this](const QString &path) {
            openFileInWorkspace(path);
        });
        bv->setTagSearchHandler([this](const QString &tag) {
            // Reuse the search panel; the DSL strips a leading '#', so a bare
            // tag works (note frontmatter tags are stored without '#').
            showSearchForQuery(QStringLiteral("tag:%1").arg(tag));
        });
        bv->setRenamePrompt([this](const QString &path) {
            if (!m_vaultObj || !m_fileManager) return;
            if (auto *f = m_vaultObj->getAbstractFileByPath(path))
                m_fileManager->promptForFileRename(f, this);
        });
        bv->setDeletePrompt([this](const QString &path) {
            if (!m_vaultObj || !m_fileManager) return;
            if (auto *f = m_vaultObj->getAbstractFileByPath(path))
                m_fileManager->promptForDeletion(f, this);
        });
        return;
    }

    // Graph view service wiring now happens inside the corbomite-graph-view
    // plugin: it captures Vault / SQLiteIndex / MetadataCache from its
    // PluginContext and passes them into each GraphView via the factory
    // closure registered with ViewRegistrar.
}

// --- Actions ---

void MainWindow::setupActions()
{
    KActionCollection *ac = actionCollection();

    KStandardAction::quit(qApp, &QApplication::quit, ac);
    auto *prefsAction = KStandardAction::preferences(this, [this]() {
        // Cluster O Phase O3.T2 — the Hotkeys page needs every provider's
        // collection, not just the universal one, so every type's
        // shortcuts show even with no matching tab open. m_markdownViewActions
        // is null the first time setupActions() itself runs (constructed
        // later in the ctor) but always set by the time a user can reach
        // Preferences via this action.
        SettingsDialog::ActionCollections collections{
            {actionCollection(), QString()},
        };
        if (m_markdownViewActions)
            collections.append({m_markdownViewActions->actionCollection(),
                                 i18n("Markdown Editor")});
        SettingsDialog dialog(m_app->pluginManager(), m_themeService,
                               collections, this);
        dialog.exec();
    }, ac);
    ac->setDefaultShortcut(prefsAction, QKeySequence(Qt::CTRL | Qt::Key_Comma));

    auto *openVault = ac->addAction(QStringLiteral("file_open_vault"));
    openVault->setText(i18n("Open Vault..."));
    openVault->setIcon(QIcon::fromTheme(QStringLiteral("folder-open")));
    ac->setDefaultShortcut(openVault, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(openVault, &QAction::triggered, this, &MainWindow::openVaultDialog);

    m_recentVaults = KStandardAction::openRecent(this, [this](const QUrl &url) {
        if (m_app->isOpen()) {
            if (!confirmCloseUnsaved()) return;
        }
        m_app->openVault(url.toLocalFile());
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
    connect(newCanvas, &QAction::triggered, this, [this]() { createNewCanvas(); });

    auto *newBase = ac->addAction(QStringLiteral("file_new_base"));
    newBase->setText(i18n("New Base"));
    newBase->setIcon(QIcon::fromTheme(QStringLiteral("x-office-spreadsheet")));
    connect(newBase, &QAction::triggered, this, [this]() { createNewBase(); });

    auto *save = ac->addAction(QStringLiteral("file_save"));
    save->setText(i18n("Save"));
    save->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    ac->setDefaultShortcut(save, QKeySequence(Qt::CTRL | Qt::Key_S));
    connect(save, &QAction::triggered, this, &MainWindow::saveCurrentNote);

    KStandardAction::undo(this, [this]() {
        if (auto *bv = activeBasesView()) {
            bv->undo();
            return;
        }
        if (auto *cv = activeCanvasView()) {
            if (auto *tab = cv->canvasWidget())
                if (auto *scene = tab->canvasScene())
                    scene->undoStack()->undo();
            return;
        }
        if (auto *editor = activeEditor())
            if (auto *leaf = editor->activeLeaf())
                leaf->undo();   // base-implemented: doc->undoD2(); no-op while read-only
    }, ac);

    KStandardAction::redo(this, [this]() {
        if (auto *bv = activeBasesView()) {
            bv->redo();
            return;
        }
        if (auto *cv = activeCanvasView()) {
            if (auto *tab = cv->canvasWidget())
                if (auto *scene = tab->canvasScene())
                    scene->undoStack()->redo();
            return;
        }
        if (auto *editor = activeEditor())
            if (auto *leaf = editor->activeLeaf())
                leaf->redo();
    }, ac);

    KStandardAction::find(this, &MainWindow::onFind, ac);
    KStandardAction::replace(this, &MainWindow::onReplace, ac);
    KStandardAction::findNext(this, &MainWindow::onFindNext, ac);
    KStandardAction::findPrev(this, &MainWindow::onFindPrev, ac);

    KStandardAction::aboutApp(this, &MainWindow::onAboutApp, ac);
    // No About KDE — Corbomite is not a KDE-branded product; Help only
    // exposes About Corbomite (homepage via KAboutData).

    auto *toggleLeft = ac->addAction(QStringLiteral("view_toggle_left_sidebar"));
    toggleLeft->setText(i18n("Toggle Left Sidebar"));
    ac->setDefaultShortcut(toggleLeft, QKeySequence(Qt::CTRL | Qt::Key_Backslash));
    connect(toggleLeft, &QAction::triggered, this, [this]() {
        setSidebarsVisible(!sidebarsVisible());
    });

    // NB: no setDefaultShortcut() on the zoom actions — Ctrl+= / Ctrl+- /
    // Ctrl+0 are owned by the editor (Markoff LiveView.qml window Shortcuts).
    // Binding them here re-introduces the "Ctrl+= is ambiguous" collision.
    // These menu items forward to the editor's zoom via onZoom*().
    auto *zoomIn = ac->addAction(QStringLiteral("view_zoom_in"));
    zoomIn->setText(i18n("Zoom In"));
    zoomIn->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    connect(zoomIn, &QAction::triggered, this, &MainWindow::onZoomIn);

    auto *zoomOut = ac->addAction(QStringLiteral("view_zoom_out"));
    zoomOut->setText(i18n("Zoom Out"));
    zoomOut->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    connect(zoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);

    auto *zoomReset = ac->addAction(QStringLiteral("view_zoom_reset"));
    zoomReset->setText(i18n("Reset Zoom"));
    zoomReset->setIcon(QIcon::fromTheme(QStringLiteral("zoom-original")));
    connect(zoomReset, &QAction::triggered, this, &MainWindow::onZoomReset);

    // editor_toggle_mode moved to MarkdownViewActions (Cluster O O3.T6).

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

    // insert_template moved to MarkdownViewActions (Cluster O O3.T6) —
    // MainWindow::insertTemplate() stays here (owns TemplateService) and is
    // connected to the provider's insertTemplateRequested() signal instead.

    auto *dailyNote = ac->addAction(QStringLiteral("open_daily_note"));
    dailyNote->setText(i18n("Open Daily Note"));
    dailyNote->setIcon(QIcon::fromTheme(QStringLiteral("view-calendar-day")));
    connect(dailyNote, &QAction::triggered, this, &MainWindow::openDailyNote);

    // View > Editor Mode radio group moved to MarkdownViewActions
    // (Cluster O O3.T6) — view_source_mode/view_editing_mode/
    // view_reading_mode object names are preserved there.

    // Tab shortcuts
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
        if (auto *next = m_workspace->nextLeafInActiveGroup())
            m_workspace->setActiveLeaf(next);
    });

    auto *prevTab = ac->addAction(QStringLiteral("tab_prev"));
    prevTab->setText(i18n("Previous Tab"));
    ac->setDefaultShortcut(prevTab, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    connect(prevTab, &QAction::triggered, this, [this]() {
        if (!m_workspace) return;
        if (auto *prev = m_workspace->previousLeafInActiveGroup())
            m_workspace->setActiveLeaf(prev);
    });

    auto *splitRight = ac->addAction(QStringLiteral("split_right"));
    splitRight->setText(i18n("Split Right"));
    ac->setDefaultShortcut(splitRight, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right));
    connect(splitRight, &QAction::triggered, this, [this]() {
        if (!m_workspace) return;
        if (auto *leaf = m_workspace->activeLeaf())
            m_workspace->duplicateLeaf(leaf, Qt::Horizontal);
    });

    auto *splitDown = ac->addAction(QStringLiteral("split_down"));
    splitDown->setText(i18n("Split Down"));
    ac->setDefaultShortcut(splitDown, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down));
    connect(splitDown, &QAction::triggered, this, [this]() {
        if (!m_workspace) return;
        if (auto *leaf = m_workspace->activeLeaf())
            m_workspace->duplicateLeaf(leaf, Qt::Vertical);
    });

    // Undo close tab
    auto *undoClose = ac->addAction(QStringLiteral("tab_undo_close"));
    undoClose->setText(i18n("Undo Close Tab"));
    ac->setDefaultShortcut(undoClose, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(undoClose, &QAction::triggered, this, [this]() {
        if (m_workspace)
            m_workspace->undoCloseLeaf();
    });

    // Back/forward (D2): per-leaf navigation history, already wired to the
    // tab-frame's nav buttons (ItemView) via WorkspaceLeaf::goBack/
    // goForward. Ctrl+Alt+Left/Right is the KDE/browser convention for the
    // same action at the keyboard-shortcut level.
    m_actionGoBack = ac->addAction(QStringLiteral("go_back"));
    m_actionGoBack->setText(i18n("Navigate Back"));
    m_actionGoBack->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    ac->setDefaultShortcut(m_actionGoBack, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left));
    m_actionGoBack->setEnabled(false);
    connect(m_actionGoBack, &QAction::triggered, this, [this]() {
        if (m_workspace)
            if (auto *leaf = m_workspace->activeLeaf())
                leaf->goBack();
    });

    m_actionGoForward = ac->addAction(QStringLiteral("go_forward"));
    m_actionGoForward->setText(i18n("Navigate Forward"));
    m_actionGoForward->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    ac->setDefaultShortcut(m_actionGoForward, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right));
    m_actionGoForward->setEnabled(false);
    connect(m_actionGoForward, &QAction::triggered, this, [this]() {
        if (m_workspace)
            if (auto *leaf = m_workspace->activeLeaf())
                leaf->goForward();
    });

    // D3: jump-to-tab. Ctrl+1..8 = that 1-based tab index in the active
    // tab group; Ctrl+9 = the last tab. Standard browser/editor
    // convention (Firefox, Chrome, most terminal emulators).
    auto jumpToTab = [this](int index /* -1 == last */) {
        if (!m_workspace) return;
        auto *active = m_workspace->activeLeaf();
        if (!active) return;
        const QString groupId = m_workspace->tabGroupIdOf(active);
        if (groupId.isEmpty()) return;
        const auto members = m_workspace->groupMembers(groupId);
        if (members.isEmpty()) return;
        const int target = (index < 0) ? members.size() - 1
                                        : qMin(index, members.size() - 1);
        if (target < 0) return;
        m_workspace->setActiveLeaf(members.at(target));
    };
    for (int i = 1; i <= 8; ++i) {
        auto *act = ac->addAction(QStringLiteral("tab_jump_%1").arg(i));
        act->setText(i18n("Go to Tab %1", i));
        ac->setDefaultShortcut(act, QKeySequence(Qt::CTRL | (Qt::Key_0 + i)));
        connect(act, &QAction::triggered, this, [jumpToTab, i]() { jumpToTab(i - 1); });
    }
    auto *jumpLastTab = ac->addAction(QStringLiteral("tab_jump_last"));
    jumpLastTab->setText(i18n("Go to Last Tab"));
    ac->setDefaultShortcut(jumpLastTab, QKeySequence(Qt::CTRL | Qt::Key_9));
    connect(jumpLastTab, &QAction::triggered, this, [jumpToTab]() { jumpToTab(-1); });

    // D3: pin-tab. Primitives (WorkspaceLeaf::pinned/setPinned,
    // Workspace::propagatePinToGroup) already existed from Cluster K/L
    // groundwork; this is the one-call command wrapper the plan asked for.
    // No default shortcut — Obsidian/Kate both leave pin as a menu/
    // command-palette-only action, not a global keybinding.
    m_actionPinTab = ac->addAction(QStringLiteral("tab_pin_toggle"));
    m_actionPinTab->setText(i18n("Pin Tab"));
    m_actionPinTab->setIcon(QIcon::fromTheme(QStringLiteral("view-pin")));
    m_actionPinTab->setCheckable(true);
    connect(m_actionPinTab, &QAction::triggered, this, [this](bool checked) {
        if (!m_workspace) return;
        if (auto *leaf = m_workspace->activeLeaf()) {
            leaf->setPinned(checked);
            m_workspace->propagatePinToGroup(leaf);
        }
    });

    // D3: move-to-new-window. One-call wrapper over the popout/floating
    // primitive kept from Phase L3's C1 audit (WorkspaceWindow/popoutLeaf).
    auto *moveToNewWindow = ac->addAction(QStringLiteral("tab_move_to_new_window"));
    moveToNewWindow->setText(i18n("Move Tab to New Window"));
    connect(moveToNewWindow, &QAction::triggered, this, [this]() {
        if (!m_workspace) return;
        if (auto *leaf = m_workspace->activeLeaf())
            m_workspace->popoutLeaf(leaf);
    });

    // D3: toggle-stacked. Decision per the plan's "advisory-only or hide"
    // choice: advisory-only. isTabGroupStacked/setTabGroupStacked already
    // exist and round-trip the bit through workspace.json (Obsidian
    // interop), but KDDW has no stacked-tabs rendering mode to hook —
    // building one is out of scope for a "missing tab command" finding.
    // The action flips the bit and tells the user via the status bar
    // rather than silently doing nothing, so it isn't a dead button.
    m_actionToggleStacked = ac->addAction(QStringLiteral("tab_toggle_stacked"));
    m_actionToggleStacked->setText(i18n("Toggle Stacked Tabs"));
    m_actionToggleStacked->setCheckable(true);
    connect(m_actionToggleStacked, &QAction::triggered, this, [this](bool checked) {
        if (!m_workspace) return;
        auto *leaf = m_workspace->activeLeaf();
        if (!leaf) return;
        const QString groupId = m_workspace->tabGroupIdOf(leaf);
        if (groupId.isEmpty()) return;
        m_workspace->setTabGroupStacked(groupId, checked);
        statusBar()->showMessage(
            checked ? i18n("Stacked tabs: saved (advisory only — layout unchanged)")
                    : i18n("Stacked tabs: off"),
            3000);
    });

    // Cluster V Phase 2+3 (Format/Heading/Insert/Table/Fold editor
    // actions) moved into MarkdownViewActions (Cluster O O3.T6).
    // Initial enable-state: no active MarkdownView yet (m_actionContext's
    // workspace isn't wired until setupEditor() runs; refresh() tolerates a
    // null workspace and is re-run once it is).
    m_actionContext->refresh();
}

void MainWindow::setupEditor()
{
    m_centralStack = new QStackedWidget(centralWidget());
    centralWidget()->layout()->addWidget(m_centralStack);

    // Index 0: Welcome screen
    m_welcomeScreen = new WelcomeScreen(m_app, m_centralStack);
    m_centralStack->addWidget(m_welcomeScreen);

    connect(m_welcomeScreen, &WelcomeScreen::vaultRequested, this, [this](const QString &path) {
        if (m_app->isOpen()) {
            if (!confirmCloseUnsaved()) return;
        }
        m_app->openVault(path);
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

        m_app->openVault(vaultPath);
    });

    // Create ViewRegistry — built-in view factories
    m_viewRegistry = new ViewRegistry(this);
    m_viewRegistry->registerViewWithExtensions(
        {QStringLiteral("md")}, QStringLiteral("markdown"),
        &MarkdownView::factory);
    m_viewRegistry->registerViewWithExtensions(
        {QStringLiteral("canvas")}, QStringLiteral("canvas"),
        &CanvasFileView::factory);
    m_viewRegistry->registerViewWithExtensions(
        {QStringLiteral("base")}, QStringLiteral("bases"),
        &Corbomite::Bases::BasesView::factory);
    // "empty" — blank-leaf placeholder (audit: views.md §1.tD). Handler
    // routes each button to the host action: Create new file → createNewNote,
    // Go to file → showQuickSwitcher, Close → Workspace::closeLeaf.
    m_viewRegistry->registerView(
        QStringLiteral("empty"),
        [this](Corbomite::WorkspaceLeaf *leaf) -> Corbomite::View * {
            return new Corbomite::EmptyView(
                leaf,
                [this, leaf](const QString &action) {
                    if (action == QLatin1String("new-file"))
                        createNewNote();
                    else if (action == QLatin1String("go-to-file"))
                        showQuickSwitcher();
                    else if (action == QLatin1String("close") && m_workspace)
                        m_workspace->closeLeaf(leaf);
                });
        });
    // "graph" view type is registered by the corbomite-graph-view plugin's
    // onLoad via ViewRegistrar. Plugins are loaded before workspace layout
    // deserialize in onVaultOpened, so the type is available by the time
    // any restored "graph" leaf instantiates.

    // Index 1: Workspace (replaces EditorViewManager)
    m_workspace = new Workspace(m_viewRegistry, this);

    // Bridge ThemeService::themeChanged → Workspace::cssChange so plugins
    // observing the proxy surface see Obsidian's `css-change` event whenever
    // the active Markoff theme actually changes (built-in switch, user theme,
    // or KDE color-scheme follow). Connection is host-lifetime; ThemeService
    // and Workspace both outlive vault-switch teardowns.
    if (m_themeService) {
        connect(m_themeService, &Corbomite::Core::ThemeService::themeChanged,
                m_workspace, [this](const Markoff::Theme &) {
            m_workspace->emitCssChange();
        });

        // Also persist the active theme to `.obsidian/appearance.json` so
        // Obsidian sees the same theme on next open. The vocabulary is:
        // empty/absent → follow system; "moonstone" → Light; "obsidian" →
        // Dark; custom CSS-theme names pass through unchanged. Idempotent —
        // the writer compares against the file's current `theme` and skips
        // when they already match, which avoids the read-on-open ping-pong
        // (when a vault open reads its appearance.json and applies the
        // theme, this slot fires but the value is already on disk).
        connect(m_themeService, &Corbomite::Core::ThemeService::themeChanged,
                this, [this](const Markoff::Theme &) {
            if (!m_pluginVaultConfig || !m_themeService) return;
            QString obsTheme;
            const QString name = m_themeService->activeThemeName();
            if (!name.isEmpty()
                && name != QStringLiteral("Follow system")) {
                if (name == QStringLiteral("Light")) {
                    obsTheme = QStringLiteral("moonstone");
                } else if (name == QStringLiteral("Dark")) {
                    obsTheme = QStringLiteral("obsidian");
                } else {
                    // Custom CSS theme: pass through (matches the
                    // VaultConfig::obsidianAppearanceTheme contract).
                    obsTheme = name;
                }
            }
            QJsonObject appearance =
                m_pluginVaultConfig->readAppearanceJson().value_or(QJsonObject{});
            const QString existing =
                appearance.value(QStringLiteral("theme")).toString();
            if (existing == obsTheme) return;
            appearance[QStringLiteral("theme")] = obsTheme;
            m_pluginVaultConfig->writeAppearanceJson(appearance);
        });
    }

    // Cluster R Task 3.1: plugin `:open` commands dispatch through
    // Workspace::revealDockView → this slot, which resolves slug to tool-view
    // id (`<slug>_panel`) and raises it.
    connect(m_workspace, &Workspace::revealDockViewRequested, this,
            [this](const QString &slug) {
        if (slug.isEmpty()) return;
        // Special-cased slugs (graph:open-local → the graph panel plugin id
        // differs from the slug). Map known slugs to plugin ids; unknown
        // slugs are treated as plugin-id-equivalent.
        static const QHash<QString, QString> slugToPluginId = {
            {QStringLiteral("backlinks"), QStringLiteral("corbomite-backlinks")},
            {QStringLiteral("outlinks"),  QStringLiteral("corbomite-outlinks")},
            {QStringLiteral("outline"),   QStringLiteral("corbomite-outline")},
            {QStringLiteral("properties"), QStringLiteral("corbomite-properties")},
            {QStringLiteral("local-graph"), QStringLiteral("corbomite-local-graph")},
            {QStringLiteral("bookmarks"),   QStringLiteral("corbomite-bookmarks")},
        };
        const QString pluginId = slugToPluginId.value(slug, slug);
        const QString toolViewId = pluginId + QStringLiteral("_panel");
        auto *tv = toolView(toolViewId);
        if (!tv) return;
        showToolView(tv);
        if (auto *pm = m_app ? m_app->pluginManager() : nullptr) {
            if (const auto *info = pm->pluginById(pluginId)) {
                auto it = m_hostedPluginViews.constFind(pluginId);
                if (info->instance && it != m_hostedPluginViews.constEnd()) {
                    info->instance->focus(*it);
                }
            }
        }
    });

    // Cluster R Task 3.7: route Workspace::commandRequested → the host
    // CommandRegistry. Used by GraphView / CanvasFileView / MarkdownView
    // hamburger actions that need to invoke a registered command without
    // holding the CommandRegistry directly.
    connect(m_workspace, &Workspace::commandRequested, this,
            [this](const QString &commandId) {
        if (m_commandRegistry) m_commandRegistry->executeById(commandId);
    });

    m_workspaceContainer = new QWidget(m_centralStack);
    auto *wsLayout = new QVBoxLayout(m_workspaceContainer);
    wsLayout->setContentsMargins(0, 0, 0, 0);
    wsLayout->addWidget(m_workspace->rootWidget());
    m_centralStack->addWidget(m_workspaceContainer);

    // Workspace re-emits substrate tab signals through its own surface so
    // we don't have to subscribe per-Tabs container. Wire once.
    connect(m_workspace, &Workspace::tabSelectRequested, this,
            [this](WorkspaceLeaf *leaf) { m_workspace->setActiveLeaf(leaf); });
    connect(m_workspace, &Workspace::tabCloseRequested, this,
            [this](WorkspaceLeaf *leaf) { m_workspace->closeLeaf(leaf); });

    // Keep the container in sync with re-parented root widgets, and wire
    // per-leaf service propagation.
    connect(m_workspace, &Workspace::layoutChanged, this, [this]() {
        auto *rootWidget = m_workspace->rootWidget();
        if (rootWidget && rootWidget->parentWidget() != m_workspaceContainer) {
            m_workspaceContainer->layout()->addWidget(rootWidget);
        }

        for (auto *leaf : m_workspace->allLeaves()) {
            // Wire deferred-load service propagation (once per leaf)
            if (!leaf->property("_mw_leaf_connected").toBool()) {
                leaf->setProperty("_mw_leaf_connected", true);
                connect(leaf, &WorkspaceLeaf::viewChanged, this,
                        [this](View *v) { propagateServicesToView(v); });
                connect(leaf, &WorkspaceLeaf::viewTypeUnresolved, this,
                        [](const QString &type, const QString &reason) {
                    Notice::post(i18n("Could not restore view \"%1\": %2", type, reason));
                });
            }

            // Propagate services to any view that exists now
            if (leaf->view())
                propagateServicesToView(leaf->view());
        }
    });

    // Obsidian parity: the workspace must never be left without at least one
    // leaf while a vault is open. When the user closes the last tab, spawn a
    // fresh empty-view leaf (audit: views.md §1.tD) so they land on the "No
    // file is open" placeholder instead of an empty toolbar.
    connect(m_workspace, &Workspace::layoutChanged, this, [this]() {
        if (!m_app || !m_app->isOpen()) return;
        if (!m_workspace->allLeaves().isEmpty()) return;
        auto *leaf = m_workspace->createLeafInActiveGroup();
        if (!leaf) return;
        QJsonObject vs;
        vs[QStringLiteral("type")] = QStringLiteral("empty");
        leaf->setViewState(vs);
        m_workspace->setActiveLeaf(leaf);
    });

    // When the active leaf changes, propagate services and update UI
    connect(m_workspace, &Workspace::activeLeafChanged,
            this, [this](WorkspaceLeaf *leaf) {
        if (leaf && leaf->view())
            propagateServicesToView(leaf->view());
        updateWindowTitle(activeEditor());

        // Cluster O Phase O1.T1/T2 — all action-state rebinding (back/
        // forward history, pin/stacked check-state, format/heading
        // enable-state, editor-mode radio sync) is now one call: the
        // controller rebinds its own per-leaf AND per-view connections
        // (including the in-place view-type-swap case fixed by T2) and
        // runs a full refresh().
        if (m_actionContext) m_actionContext->bindActiveLeaf(leaf);

        auto *editor = activeEditor();
        // Update sidebar panels
        // All sidebar panels (Backlinks/Outlinks/Outline/Properties/
        // LocalGraph/FileExplorer/Search) are now InternalPlugins that
        // react to active-leaf changes via WorkspaceController.
        Q_UNUSED(editor);

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

        // TODO(port-foundation-exploration): cursorPositionChanged(int,int)
        // was on the old Markoff::Editor. EditorWidget's cursor signals come
        // via binding()->cursorState() — re-wire when the Table submenu's
        // cursorInTable gate is reimplemented against the new context shape.
        if (editor) {
            // Phase C6 — context-menu contribution (currently a TODO
            // no-op body; format/heading/context wiring now lives on
            // ActionContextController, see bindActiveLeaf() above).
            connectEditorContextMenu(editor);
            // C2 — wire ThemeService so this editor follows theme changes.
            if (m_themeService)
                editor->setThemeService(m_themeService);
        }
    });


    // Start on welcome screen
    m_centralStack->setCurrentIndex(0);
    setSidebarsVisibleInternal(false, true);
}

void MainWindow::setupSidebars()
{
    // FileExplorer panel migrated to InternalPlugin
    // "corbomite-file-explorer" (Cluster Q Task 18). MainWindow no
    // longer constructs the tree, NotesTreeModel, or the right-click
    // file ops — all live inside the plugin via VaultProxy +
    // FileManagerProxy + WorkspaceController. Session expanded-folders
    // persistence is a follow-up; see Recent decisions.

    // Search panel migrated to InternalPlugin "corbomite-search"
    // (Cluster Q Task 17).

    // Backlinks panel migrated to InternalPlugin "corbomite-backlinks"
    // (Cluster Q Task 13). MainWindow no longer constructs it; the
    // plugin's createView() output is hosted by hostPluginView() when
    // PluginManager fires pluginLoaded.

    // Outlinks panel migrated to InternalPlugin "corbomite-outlinks"
    // (Cluster Q Task 14). MainWindow no longer constructs it.

    // Properties panel migrated to InternalPlugin "corbomite-properties"
    // (Cluster Q Task 16).

    // Outline panel migrated to InternalPlugin "corbomite-outline"
    // (Cluster Q Task 15). Scroll-to-line on item click is deferred —
    // no host-side editor accessor exposed via WorkspaceController yet.

    // LocalGraph panel migrated to InternalPlugin "corbomite-local-graph"
    // (Cluster Q Task 19).

    // Graph Controls panel is hosted by corbomite-graph-view plugin's
    // createView path — shows up as a Right-side tool view when the
    // plugin loads. No direct construction here.
}

void MainWindow::setupStatusBar()
{
    m_wordCountLabel = new QLabel(i18n("Words: 0"), this);
    m_cursorPosLabel = new QLabel(i18n("Ln 1, Col 1"), this);

    statusBar()->addPermanentWidget(m_wordCountLabel);
    statusBar()->addPermanentWidget(m_cursorPosLabel);

    // Cluster B Phase 2 — host-wide plugin status-bar registry.
    m_statusBarRegistry = new Corbomite::StatusBarRegistry(statusBar(), this);
}

void MainWindow::setupRibbonToolBar()
{
    m_ribbonToolBar = new RibbonToolBar(QStringLiteral("ribbonToolBar"), this);
    m_ribbonToolBar->setWindowTitle(i18n("Ribbon"));
    // Dock to the top toolbar area, after the main toolbar inserted by
    // KXMLGUI. addToolBar() places toolbars left-to-right in the same area
    // in insertion order, so calling this after KXMLGUI has added the
    // main toolbar lands us immediately to its right.
    addToolBar(Qt::TopToolBarArea, m_ribbonToolBar);

    // RibbonStateController is bound lazily in onVaultOpened when a
    // SessionManager exists. Until then, no icons are registered and the
    // toolbar is visibly empty — intentional per the design spec.
}

void MainWindow::openVaultDialog()
{
    if (m_app->isOpen()) {
        if (!confirmCloseUnsaved()) return;
    }

    QString dir = QFileDialog::getExistingDirectory(
        this, i18n("Open Vault"), QDir::homePath());
    if (!dir.isEmpty()) {
        if (!m_app->openVault(dir)) {
            KMessageBox::error(this,
                i18n("Could not open vault at:\n%1\n\nThe directory may not exist or is not readable.", dir),
                i18n("Open Vault Failed"));
        }
    }
}

void MainWindow::closeVault()
{
    if (!m_app->isOpen()) return;
    if (!confirmCloseUnsaved()) return;
    m_app->closeVault();
}

void MainWindow::createNewNote()
{
    if (!m_app->isOpen()) {
        openVaultDialog();
        if (!m_app->isOpen()) return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, i18n("New Note"),
                                          i18n("Note name:"), QLineEdit::Normal,
                                          QString(), &ok);
    if (ok && !name.isEmpty() && m_fileManager) {
        auto *tf = m_fileManager->createMarkdownNote(name, QString());
        if (tf)
            openFileInWorkspace(tf->path);
    }
}

void MainWindow::createNewCanvas(const QString &folder)
{
    // M2.6 — "Create new canvas": Obsidian-style create-then-rename, not
    // createNewNote()'s upfront QInputDialog prompt. Creates
    // "Untitled.canvas" (dedup-numbered on collision, same
    // FileManager::createNewFile()/collisionFreeName() rule
    // createMarkdownNote() uses), opens it, then triggers a rename.
    //
    // Divergence from the plan's literal wording: there is no existing
    // "inline-rename-on-creation" mechanism to mirror — createNewNote()
    // and FileExplorerView::onNewNoteIn() both prompt for a name via
    // QInputDialog *before* creating the file, not after. Building a new
    // inline-edit-on-creation mechanism (tab-title or tree-row inline
    // editor triggered automatically post-create) is out of scope here;
    // this instead reuses FileManager::promptForFileRename() — the same
    // modal rename dialog F2 / the tree's "Rename..." action already use
    // — as the closest existing "start a rename" primitive. Noted for a
    // follow-up if a true inline-rename-on-creation flow gets built for
    // notes generally.
    if (!m_app->isOpen()) {
        openVaultDialog();
        if (!m_app->isOpen()) return;
    }
    if (!m_fileManager || !m_vaultObj) return;

    TFolder *parent = m_vaultObj->getRoot();
    if (!folder.isEmpty()) {
        if (auto *existing = m_vaultObj->getFolderByPath(folder))
            parent = existing;
        else if (auto *created = m_vaultObj->createFolder(folder))
            parent = created;
    }

    Canvas::CanvasDocument emptyDoc; // {"nodes":[],"edges":[]}
    const QByteArray content = QJsonDocument(emptyDoc.toJson()).toJson(QJsonDocument::Indented);

    auto *tf = m_fileManager->createNewFile(parent, QString(), QStringLiteral("canvas"), content);
    if (!tf)
        return;

    openFileInWorkspace(tf->path);
    m_fileManager->promptForFileRename(tf, this);
}

void MainWindow::createNewBase(const QString &folder)
{
    // Cluster D D.6 — same create-then-rename flow as createNewCanvas()
    // above. Unlike canvas, no seed content is needed: BasesQuery::fromString
    // already treats an empty string as a valid default one-view "Table"
    // query (audit's [CRIT] empty-file invariant, BasesQuery.cpp), so
    // BasesView opens a freshly-created empty .base file directly into an
    // editable table with no special-casing here.
    if (!m_app->isOpen()) {
        openVaultDialog();
        if (!m_app->isOpen()) return;
    }
    if (!m_fileManager || !m_vaultObj) return;

    TFolder *parent = m_vaultObj->getRoot();
    if (!folder.isEmpty()) {
        if (auto *existing = m_vaultObj->getFolderByPath(folder))
            parent = existing;
        else if (auto *created = m_vaultObj->createFolder(folder))
            parent = created;
    }

    auto *tf = m_fileManager->createNewFile(parent, QString(), QStringLiteral("base"), QByteArray());
    if (!tf)
        return;

    openFileInWorkspace(tf->path);
    m_fileManager->promptForFileRename(tf, this);
}

void MainWindow::saveCurrentNote()
{
    if (!m_workspace) return;
    auto *leaf = m_workspace->activeLeaf();
    if (!leaf || !leaf->view()) return;

    // For MarkdownView the NoteDocument is the source of truth for
    // modification state. Save through Vault::saveDocument — it writes
    // via Vault::modify so the self-write echo-suppression ledger keeps
    // our own save from re-firing as an external modify event.
    auto *editor = activeEditor();
    if (editor && editor->noteDocument() && m_vaultObj) {
        m_vaultObj->saveDocument(editor->noteDocument());
        return;
    }

    // Cluster O Phase O1.T4 — CanvasFileView is a bare FileView, not a
    // TextFileView, so the fallback below never reached it: Ctrl+S was a
    // real no-op on a canvas tab (report §3.1). BasesView already worked
    // via the TextFileView fallback (BasesView : TextFileView).
    if (auto *cv = qobject_cast<CanvasFileView *>(leaf->view())) {
        if (auto *tab = cv->canvasWidget())
            tab->save();
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
    if (!m_app->isOpen()) return;

    QStringList recent;
    if (m_workspace) {
        recent = m_workspace->lastOpenFiles();
    }

    auto *switcher = new QuickSwitcher(m_vaultObj, recent, this);

    QPoint topCenter = mapToGlobal(QPoint(width() / 2 - 300, 80));
    switcher->move(topCenter);

    connect(switcher, &QuickSwitcher::noteSelected,
            this, &MainWindow::onNoteActivated);
    connect(switcher, &QuickSwitcher::createNoteRequested,
            this, [this](const QString &name) {
        if (!m_fileManager) return;
        auto *tf = m_fileManager->createMarkdownNote(name, QString());
        if (tf) openFileInWorkspace(tf->path);
    });

    switcher->show();
}

QString MainWindow::resolveOrCreateNoteTarget(const QString &relativePath)
{
    if (relativePath.endsWith(QStringLiteral(".canvas")))
        return relativePath;

    if (m_vaultObj && m_vaultObj->getAbstractFileByPath(relativePath))
        return relativePath;

    if (!m_fileManager)
        return {};

    // Obsidian create-on-click parity: clicking a wikilink whose target
    // does not exist eagerly creates the note file on disk, then opens it.
    QString name = relativePath;
    if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
    QString folder;
    int lastSlash = name.lastIndexOf(QLatin1Char('/'));
    if (lastSlash >= 0) {
        folder = name.left(lastSlash);
        name = name.mid(lastSlash + 1);
    }
    auto *tf = m_fileManager->createMarkdownNote(name, folder);
    return tf ? tf->path : QString();
}

void MainWindow::onNoteActivated(const QString &relativePath)
{
    const QString target = resolveOrCreateNoteTarget(relativePath);
    if (!target.isEmpty())
        openFileInWorkspace(target);
}

void MainWindow::navigateActiveLeafTo(const QString &relativePath)
{
    auto *leaf = m_workspace ? m_workspace->activeLeaf() : nullptr;
    if (!leaf) {
        onNoteActivated(relativePath);
        return;
    }

    const QString target = resolveOrCreateNoteTarget(relativePath);
    if (target.isEmpty())
        return;

    QString ext = QFileInfo(target).suffix().toLower();
    QString type = m_viewRegistry ? m_viewRegistry->getTypeByExtension(ext) : QString();
    if (type.isEmpty()) type = QStringLiteral("markdown");

    QJsonObject viewState;
    viewState[QStringLiteral("type")] = type;
    viewState[QStringLiteral("state")] = QJsonObject{
        {QStringLiteral("file"), target}
    };
    // navigate() pushes the leaf's PRE-navigation state to its own
    // LeafHistory before swapping content — this is the same mechanism
    // WorkspaceLeaf::goBack/goForward (already wired to the tab-frame's
    // nav buttons, ItemView.cpp) read from; nothing previously called
    // navigate() from the link-click path, which is why those buttons have
    // never had anything to go back TO.
    leaf->navigate(viewState);
    m_workspace->setActiveLeaf(leaf);
    m_workspace->pushLastOpenFile(target);
}

// Cluster O Phase O1.T1 — updateBackForwardActions()/updateTabStateActions()
// moved into ActionContextController (same bodies, looked up by
// actionCollection()->action(id) instead of the cached m_action* pointers).

void MainWindow::onVaultOpened(const QString &path)
{
    m_centralStack->setCurrentIndex(1);
    setSidebarsVisible(true);

    updateWindowTitle();

    m_recentVaults->addUrl(QUrl::fromLocalFile(path));
    auto config = KSharedConfig::openConfig();
    KConfigGroup recentGroup = config->group(QStringLiteral("RecentVaults"));
    m_recentVaults->saveEntries(recentGroup);
    config->sync();

    // Canonical Vault. Construct before NotesTreeModel / LinkResolver
    // so consumers can bind against the loaded tree. MetadataCache +
    // FileManager are created below (FileManager depends on MetadataCache).
    if (!m_fsAdapter) m_fsAdapter = std::make_unique<FileSystemAdapter>();
    delete m_vaultObj;
    m_vaultObj = new Vault(m_fsAdapter.get(), this);
    m_vaultObj->load(path);

    const QString configPath = path + QStringLiteral("/") + m_vaultObj->configDir();

    // NotesTreeModel lives entirely inside the FileExplorer plugin now —
    // MainWindow used to construct one here "in case" some host-side
    // consumer needed it, but none ever appeared. Removed in Cluster N
    // Task 2.5 when NotesTreeModel migrated onto VaultProxy.

    delete m_autosave;
    m_autosave = new AutosaveReactor(m_vaultObj, this);

    // Set file resolver so FileView::setState can load NoteDocuments
    m_viewRegistry->setFileResolver([this](const QString &relPath) -> NoteDocument * {
        if (!m_vaultObj) return nullptr;
        return m_vaultObj->openDocument(relPath);
    });

    // LinkResolver — populated with ALL files (not just .md) so that
    // attachment embeds (e.g. ![[image.png]]) resolve alongside note wikilinks.
    delete m_linkResolver;
    m_linkResolver = new LinkResolver();
    {
        QStringList allPaths;
        const auto files = m_vaultObj->getFiles();
        allPaths.reserve(files.size());
        for (auto *tf : files) {
            if (tf) allPaths.append(tf->path);
        }
        m_linkResolver->setVaultPaths(allPaths);
    }

    // MetadataCache + SQLiteIndex
    delete m_metadataCache;
    m_metadataCache = new MetadataCache(*m_linkResolver, this);

    // FileManager depends on MetadataCache (constructed just above).
    delete m_fileManager;
    m_fileManager = new FileManager(m_vaultObj, m_metadataCache, this);

    m_popoverResources = std::make_unique<VaultScopedResources>(m_vaultObj);
    // TODO(port-foundation-exploration): MarkoffAdapters (LinkResolverAdapter,
    // MetadataCacheAdapter, MetadataParserImpl) all #if 0-disabled until
    // Markoff::Vault::* concretes are restored. EmbedRenderer wiring same.
    // m_linkResolverAdapter = std::make_unique<...>(m_linkResolver);
    // m_metadataCacheAdapter = std::make_unique<...>(m_metadataCache);
    // m_metadataParserImpl = std::make_unique<...>(m_linkResolver);
    // if (m_embedRenderer) { m_embedRenderer->setMetadataCache(...); ... }

    // Wire suggesters + hover popover against the live vault.
    m_hoverPopover->setResources(m_popoverResources.get());
    if (m_wikiSuggest) {
        m_wikiSuggest->setVault(m_vaultObj);
        m_wikiSuggest->setLinkResolver(m_linkResolver);
        m_wikiSuggest->setMetadataCache(m_metadataCache);
    }

    QDir().mkpath(configPath);

    // Db files live outside the vault under AppLocalDataLocation (or TempLocation
    // if AppLocalDataLocation is unavailable — see PathUtils::vaultLocalDataDir).
    // vaultLocalDataDir() is guaranteed non-empty for a non-empty vault path and
    // guaranteed not vault-relative, so no fallback to configPath is needed or
    // safe.  Using configPath as a write target was the 0.7 defect: it caused a
    // destroy-rebuild loop because the legacy cleanup runs on every open.
    const QString dbDir = PathUtils::vaultLocalDataDir(path);
    QDir().mkpath(dbDir);

    // Legacy cleanup: if the DB files are still in the old .obsidian/ location
    // (pre-Task-0.7), delete them so the vault stays clean. They are entirely
    // regenerable. Best-effort only — never touch any other .obsidian/ files.
    {
        const QString legacyIndex   = configPath + QStringLiteral("/index.sqlite");
        const QString legacyCache   = configPath + QStringLiteral("/metadata-cache.db");
        if (QFile::exists(legacyIndex)) {
            if (QFile::remove(legacyIndex))
                qDebug() << "MainWindow: removed legacy in-vault DB" << legacyIndex;
            else
                qDebug() << "MainWindow: could not remove legacy in-vault DB" << legacyIndex;
        }
        if (QFile::exists(legacyCache)) {
            if (QFile::remove(legacyCache))
                qDebug() << "MainWindow: removed legacy in-vault DB" << legacyCache;
            else
                qDebug() << "MainWindow: could not remove legacy in-vault DB" << legacyCache;
        }
    }

    const QString indexDbPath = dbDir + QStringLiteral("/index.sqlite");
    const QString cacheDbPath = dbDir + QStringLiteral("/metadata-cache.db");

    delete m_searchIndex;
    m_searchIndex = new SQLiteIndex(this);
    m_searchIndex->open(indexDbPath);
    m_searchIndex->setVaultRoot(path);
    m_searchIndex->setMetadataCache(m_metadataCache);
    if (m_tagSuggest) m_tagSuggest->setIndex(m_searchIndex);

    connect(m_metadataCache, &MetadataCache::cacheOpenFailed, this,
            [](const QString &dbPath, const QString &reason) {
        Notice::post(i18n("Failed to open metadata cache \"%1\": %2 "
                           "(search and indexing will not persist across restarts)",
                           dbPath, reason));
    });
    m_metadataCache->open(cacheDbPath);

    m_searchIndex->reconcileWithCache();

    statusBar()->showMessage(i18n("Indexing vault..."));
    connect(m_metadataCache, &MetadataCache::indexFinished, this, [this]() {
        statusBar()->showMessage(i18n("Indexing complete"), 3000);
    });

    {
        QStringList notePaths;
        const auto files = m_vaultObj->getMarkdownFiles();
        notePaths.reserve(files.size());
        for (auto *tf : files) {
            if (tf) notePaths.append(tf->path);
        }
        m_metadataCache->rebuildVault(path, notePaths);
    }

    // Update MetadataCache on note saves
    connect(m_autosave, &AutosaveReactor::noteSaved, this, [this](const QString &relPath) {
        if (!m_metadataCache || !m_vaultObj) return;
        auto *doc = m_vaultObj->cachedDocument(relPath);
        if (!doc) return;
        const QByteArray bytes = doc->markdown().toUtf8();
        const QString absPath =
            m_vaultObj->basePath() + QLatin1Char('/') + relPath;
        const qint64 mtimeMs = QFileInfo(absPath).lastModified().toMSecsSinceEpoch();
        m_metadataCache->onFileChanged(relPath, bytes, mtimeMs);
    });

    // Q.0 P7 — external-filesystem events flow through Vault's signals.
    // Self-writes are suppressed by Vault's echo-suppression ledger, so these
    // handlers only fire for genuinely external mutations.
    connect(m_vaultObj, &Vault::created, this,
            [this](TAbstractFile *f) {
        auto *tf = dynamic_cast<TFile *>(f);
        if (!tf) return;
        // Task 0.3 P1 — keep LinkResolver fresh; feed all file types so
        // attachment embeds (e.g. ![[image.png]]) resolve without a reopen.
        if (m_linkResolver) m_linkResolver->addVaultPath(tf->path);
        if (tf->extension != QLatin1String("md")) return;
        if (!m_metadataCache) return;
        const QByteArray bytes = m_vaultObj->read(tf);
        const qint64 mtimeMs = tf->stat ? tf->stat->mtimeMs : 0;
        m_metadataCache->onFileChanged(tf->path, bytes, mtimeMs);
    });
    connect(m_vaultObj, &Vault::modified, this, [this](TFile *tf) {
        if (!tf) return;
        // Every TextFileView learns about external modifications so it can
        // reload / three-way-merge as appropriate.
        if (m_workspace) {
            for (auto *leaf : m_workspace->allLeaves()) {
                if (auto *tfv = qobject_cast<TextFileView *>(leaf->view())) {
                    tfv->onExternalModify(tf->path);
                }
            }
        }
        if (m_metadataCache && tf->extension == QLatin1String("md")) {
            const QByteArray bytes = m_vaultObj->read(tf);
            const qint64 mtimeMs = tf->stat ? tf->stat->mtimeMs : 0;
            m_metadataCache->onFileChanged(tf->path, bytes, mtimeMs);
        }
    });
    connect(m_vaultObj, &Vault::deletedFile, this, [this](TAbstractFile *f) {
        if (!f) return;
        // Task 0.3 P1 — keep LinkResolver fresh on delete.
        if (m_linkResolver) m_linkResolver->removeVaultPath(f->path);
        if (!m_metadataCache) return;
        m_metadataCache->onFileDeleted(f->path);
    });
    connect(m_vaultObj, &Vault::renamed, this,
            [this](TAbstractFile *f, const QString &oldPath) {
        if (!f) return;
        auto *tf = dynamic_cast<TFile *>(f);
        // Task 0.3 P1 — keep LinkResolver fresh on rename (all file types).
        if (m_linkResolver && tf) {
            m_linkResolver->removeVaultPath(oldPath);
            m_linkResolver->addVaultPath(tf->path);
        }
        if (!m_metadataCache) return;
        m_metadataCache->onFileDeleted(oldPath);
        if (!tf || tf->extension != QLatin1String("md")) return;
        const QByteArray bytes = m_vaultObj->read(tf);
        const qint64 mtimeMs = tf->stat ? tf->stat->mtimeMs : 0;
        m_metadataCache->onFileChanged(tf->path, bytes, mtimeMs);
    });
    connect(m_vaultObj, &Vault::documentSaveFailed, this,
            [](const QString &relPath, const QString &reason) {
        Notice::post(i18n("Failed to save \"%1\": %2", relPath, reason));
    });

    // Session manager — tier 2 (vault-portable) + tier 3 (machine-local)
    // Corbomite-native state. Tier 1 (Obsidian-schema workspace.json) is
    // handled directly by Workspace::readWorkspaceJson below.
    delete m_sessionManager;
    m_sessionManager = new SessionManager(this);
    m_sessionManager->setVaultPath(path);
    m_sessionManager->load();

    if (!m_ribbonState) {
        m_ribbonState = new RibbonStateController(m_ribbonToolBar,
                                                   m_sessionManager, this);
    } else {
        m_ribbonState->rebind(m_sessionManager);
    }
    m_ribbonState->applyFromSession();

    const auto geometry = m_sessionManager->windowGeometry();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
    const auto windowStateBytes = m_sessionManager->windowState();
    if (!windowStateBytes.isEmpty()) restoreState(windowStateBytes);

    const auto sidebar = m_sessionManager->sidebarState();
    if (!sidebar.isEmpty()) {
        const bool leftVisible = sidebar.value(QStringLiteral("leftVisible")).toBool(true);
        setSidebarsVisibleInternal(leftVisible, true);

        // D4: the width half of "restore persisted widths/visibility" —
        // visibility was already wired (above); widths were saved
        // (SessionManager::saveSidebarState) but never read back into
        // CorbomiteMDI. sidebarState()'s width fields default to 200 (the
        // pre-existing hardcoded value) when absent, so this is a no-op on
        // first run / old sessions.
        setSidebarWidth(KMultiTabBar::Left,
                        sidebar.value(QStringLiteral("leftWidth")).toInt(200));
        setSidebarWidth(KMultiTabBar::Right,
                        sidebar.value(QStringLiteral("rightWidth")).toInt(200));
    }

    m_sessionManager->blockSaving();

    // Plugin lifecycle moved ahead of workspace.deserialize so view types
    // registered by plugins (notably "graph" by corbomite-graph-view) are
    // available when a restored leaf instantiates. Hosted-view attachment
    // still rides the pluginLoaded signal that enablePlugin emits below.
    rewirePluginCoreServices();
    if (auto *pm = m_app->pluginManager()) {
        // Bind a vault-scoped VaultConfig so enable/disable mirrors into
        // .obsidian/{core,community}-plugins.json for plugins with an
        // Obsidian counterpart (X-Obsidian-Id manifest field or internal
        // alias dict). Lifetime cleared in onVaultClosed.
        m_pluginVaultConfig =
            std::make_unique<VaultConfig>(m_fsAdapter.get(), path);
        pm->setVaultConfig(m_pluginVaultConfig.get());
        pm->loadEnabledStateFromConfig();
    }

    // Apply the per-vault theme from `.obsidian/appearance.json` so opening
    // a vault that Obsidian wrote with `theme: "obsidian"` switches Corbomite
    // to its Dark theme (and vice versa). When `theme` is empty or absent,
    // ThemeService stays at "Follow system". Custom CSS-theme names pass
    // through and bind via setActiveThemeByName if the user has installed a
    // matching theme in `~/.config/corbomite[-dev]/themes/`. The
    // appearance-write hook above is idempotent so the resulting
    // themeChanged emission won't churn the file.
    if (m_themeService && m_pluginVaultConfig) {
        if (auto app = m_pluginVaultConfig->readAppearanceJson()) {
            const QString obsTheme =
                app->value(QStringLiteral("theme")).toString();
            QString internal = QStringLiteral("Follow system");
            if (!obsTheme.isEmpty()) {
                if (obsTheme == QStringLiteral("moonstone")) {
                    internal = QStringLiteral("Light");
                } else if (obsTheme == QStringLiteral("obsidian")) {
                    internal = QStringLiteral("Dark");
                } else if (m_themeService->availableThemeNames()
                            .contains(obsTheme)) {
                    internal = obsTheme;
                }
            }
            if (internal != m_themeService->activeThemeName()) {
                m_themeService->setActiveThemeByName(internal);
            }
        }
    }

    // Tier 1 (Obsidian-schema workspace.json): Workspace owns full-fidelity
    // load now (main/active/floating/lastOpenFiles + unknown-key
    // passthrough with the _corbomite/left-ribbon denylist). Handles both
    // "session found" and "no/invalid workspace.json" (installs a default
    // layout + active leaf) — see Workspace::readWorkspaceJson.
    if (m_workspace) {
        m_workspace->readWorkspaceJson(path);
    }

    const auto folders = m_sessionManager->expandedFolders();
    // Expanded-folder restore moved into the plugin (Cluster Q Task 18).
    Q_UNUSED(folders);

    m_sessionManager->unblockSaving();

    // Template and Daily Note services
    auto *settings = CorbomiteSettings::self();

    delete m_templateService;
    m_templateService = new TemplateService(m_vaultObj, this);
    m_templateService->setTemplateFolder(settings->templateFolder());
    m_templateService->setDefaultDateFormat(settings->defaultDateFormat());
    m_templateService->setDefaultTimeFormat(settings->defaultTimeFormat());

    {
        FileSystemAdapter fs;
        VaultConfig vaultConfig(&fs, path);
        m_templateService->initFromVaultConfig(vaultConfig);
    }

    delete m_dailyNoteService;
    m_dailyNoteService = new DailyNoteService(m_vaultObj, m_fileManager,
                                                m_templateService, this);
    m_dailyNoteService->setDateFormat(settings->dailyNoteDateFormat());
    m_dailyNoteService->setFolder(settings->dailyNoteFolder());
    m_dailyNoteService->setTemplateName(settings->dailyNoteTemplate());

    {
        FileSystemAdapter fs;
        VaultConfig vaultConfig(&fs, path);
        m_dailyNoteService->initFromVaultConfig(vaultConfig);
    }

    if (m_actionContext) m_actionContext->refresh();
}

void MainWindow::onVaultClosed()
{
    if (m_ribbonState) m_ribbonState->rebind(nullptr);

    // Tear down vault-scoped plugins before clearing the services they
    // hold pointers to. PluginManager::pluginUnloading fires per plugin
    // and triggers releasePluginView.
    if (auto *pm = m_app->pluginManager()) {
        QStringList loadedIds;
        for (int i = 0; i < pm->pluginCount(); ++i) {
            const auto &info = pm->pluginByIndex(i);
            if (info.instance)
                loadedIds.append(info.metaData.base().pluginId());
        }
        // Lifecycle teardown — keep persisted enabled-state intact.
        for (const QString &id : loadedIds) pm->disablePlugin(id, /*persist=*/false);
        // Detach vault-scoped JSON sync so subsequent toggles don't write
        // into the now-closed vault's .obsidian directory.
        pm->setVaultConfig(nullptr);
    }
    m_pluginVaultConfig.reset();

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

    if (m_wikiSuggest) {
        m_wikiSuggest->setVault(nullptr);
        m_wikiSuggest->setLinkResolver(nullptr);
        m_wikiSuggest->setMetadataCache(nullptr);
    }
    if (m_tagSuggest) m_tagSuggest->setIndex(nullptr);
    if (m_hoverPopover) m_hoverPopover->setResources(nullptr);


    // TODO(port-foundation-exploration): EmbedRenderer teardown disabled.
    // if (m_embedRenderer) { m_embedRenderer->setMetadataCache(nullptr); ... }
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
    // Drop the Markoff adapter shims before deleting their wrapped pointers.
    // Consumers were nulled above; resetting now means a fresh open's
    // re-creation can't transiently destroy a still-referenced shim.
    // TODO(port-foundation-exploration): adapter fields disabled.
    // m_linkResolverAdapter.reset(); m_metadataCacheAdapter.reset(); m_metadataParserImpl.reset();
    delete m_metadataCache;
    m_metadataCache = nullptr;
    delete m_linkResolver;
    m_linkResolver = nullptr;

    m_centralStack->setCurrentIndex(0);
    m_welcomeScreen->refreshRecentVaults();
    setSidebarsVisibleInternal(false, true);

    updateWindowTitle();
    if (m_actionContext) m_actionContext->refresh();
}

void MainWindow::openGraphView()
{
    if (!m_app->isOpen() || !m_searchIndex || !m_workspace) return;

    auto *leaf = m_workspace->createLeafInActiveGroup();
    if (!leaf) return;

    QJsonObject viewState;
    viewState[QStringLiteral("type")] = QStringLiteral("graph");
    viewState[QStringLiteral("state")] = QJsonObject{};
    leaf->setViewState(viewState);
    m_workspace->setActiveLeaf(leaf);

    // Service wiring happens inside corbomite-graph-view plugin's factory
    // closure — no direct graphTab manipulation from the host.
}

void MainWindow::showSearchPanel()
{
    // Search panel is an InternalPlugin (Cluster Q Task 17). Surface its
    // tool view via the plugin-id-derived slot, then dispatch focus through
    // the plugin so SearchPlugin can land caret on its QLineEdit rather
    // than the tool-view root widget.
    auto *tv = toolView(QStringLiteral("corbomite-search_panel"));
    if (!tv) return;
    showToolView(tv);
    if (auto *pm = m_app ? m_app->pluginManager() : nullptr) {
        if (const auto *info = pm->pluginById(QStringLiteral("corbomite-search"))) {
            auto it = m_hostedPluginViews.constFind(QStringLiteral("corbomite-search"));
            if (info->instance && it != m_hostedPluginViews.constEnd()) {
                info->instance->focus(*it);
            }
        }
    }
}

void MainWindow::showSearchForQuery(const QString &query)
{
    // Surface the search tool view (as showSearchPanel does), then push the
    // query into the hosted SearchView. The plugin lives in a separate .so,
    // so we cannot link its symbols — invoke setQuery by name via the meta
    // object instead (it is Q_INVOKABLE).
    auto *tv = toolView(QStringLiteral("corbomite-search_panel"));
    if (!tv) return;
    showToolView(tv);
    auto it = m_hostedPluginViews.constFind(QStringLiteral("corbomite-search"));
    if (it != m_hostedPluginViews.constEnd() && *it) {
        QMetaObject::invokeMethod(*it, "setQuery", Q_ARG(QString, query));
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

    // Insert at the caret (one undo-integrated D2 edit) rather than appending
    // at end-of-document. The {{cursor}} marker, if present, positions the
    // caret post-insert. (road-to-dogfood Phase 2 — template-at-cursor.)
    editor->insertAtCursor(expanded, TemplateService::cursorMarker());
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

// Cluster O Phase O1.T7 — updateVaultActions() moved into
// ActionContextController::updateVaultActions(), split out into
// updateSaveAction() (O1.T4, type-aware) and updateEditorModeActions()/
// updateFindAndTemplateActions() (O1.T5/T7, type-aware) so file_save,
// the editor-mode radios, and insert_template stop being enabled on tabs
// where they were previously silent no-ops.

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

    if (m_vaultObj && m_vaultObj->isLoaded()) {
        title += m_vaultObj->getName();
        title += QStringLiteral(" \u2014 ");
    }

#ifdef CORBOMITE_DEV_BUILD
    title += QStringLiteral("Corbomite [Dev]");
#else
    title += QStringLiteral("Corbomite");
#endif

    setWindowTitle(title);
}

void MainWindow::applyTheme()
{
    auto *mgr = KColorSchemeManager::instance();
    if (!mgr) return;
    const QString theme = CorbomiteSettings::self()->theme();
    if (theme.isEmpty() || theme == QLatin1String("system")) {
        // Unset → track OS colour scheme.
        mgr->activateScheme(QModelIndex());
        return;
    }
    const QString schemeId = (theme == QLatin1String("dark"))
        ? QStringLiteral("BreezeDark")
        : QStringLiteral("BreezeLight");
    const QModelIndex idx = mgr->indexForSchemeId(schemeId);
    if (idx.isValid()) mgr->activateScheme(idx);
}

void MainWindow::applyVaultPortableSettings()
{
    if (!m_vaultObj || !m_vaultObj->isLoaded()) {
        return; // No vault open — nothing to persist.
    }
    auto *settings = CorbomiteSettings::self();
    FileSystemAdapter fs; // stateless; cheap to construct
    VaultConfig vc(&fs, m_vaultObj->basePath());
    if (!vc.ensureConfigDir()) {
        return; // Vault not writable — silently skip; toast is V.future scope.
    }

    // appearance.json — theme key. Translate Corbomite's
    // system/light/dark vocabulary into Obsidian's "/moonstone/obsidian
    // (or pass through a custom CSS theme name). Without translation a
    // shared vault sees the value differently in each tool.
    {
        QJsonObject upd;
        const QString theme = VaultConfig::obsidianAppearanceTheme(
            settings->theme());
        if (!theme.isEmpty()) {
            upd.insert(QStringLiteral("theme"), theme);
        }
        if (!upd.isEmpty()) {
            if (!vc.mergeJson(QStringLiteral("appearance.json"), upd)) {
                qWarning() << "applyVaultPortableSettings: failed to write"
                           << "appearance.json";
            }
        }
    }

    // daily-notes.json — folder, format, template (Obsidian's daily-notes
    // plugin keys).
    {
        QJsonObject upd;
        const QString folder = settings->dailyNoteFolder();
        const QString format = settings->dailyNoteDateFormat();
        const QString tmpl = settings->dailyNoteTemplate();
        if (!folder.isEmpty()) upd.insert(QStringLiteral("folder"), folder);
        if (!format.isEmpty()) upd.insert(QStringLiteral("format"), format);
        if (!tmpl.isEmpty())   upd.insert(QStringLiteral("template"), tmpl);
        if (!upd.isEmpty()) {
            if (!vc.mergeJson(QStringLiteral("daily-notes.json"), upd)) {
                qWarning() << "applyVaultPortableSettings: failed to write"
                           << "daily-notes.json";
            }
        }
    }

    // templates.json — folder key.
    {
        QJsonObject upd;
        const QString folder = settings->templateFolder();
        if (!folder.isEmpty()) {
            upd.insert(QStringLiteral("folder"), folder);
        }
        if (!upd.isEmpty()) {
            if (!vc.mergeJson(QStringLiteral("templates.json"), upd)) {
                qWarning() << "applyVaultPortableSettings: failed to write"
                           << "templates.json";
            }
        }
    }
}

void MainWindow::applyAutosaveDelay()
{
    if (!m_autosave) return;
    const int ms = CorbomiteSettings::self()->autoSaveDelayMs();
    m_autosave->setDelayMs(ms);
}

void MainWindow::applyReadableLineWidth()
{
    if (!m_workspace) return;
    const bool readable = CorbomiteSettings::self()->readableLineWidth();
    for (auto *leaf : m_workspace->allLeaves()) {
        if (leaf->isDeferred()) continue;
        auto *mv = qobject_cast<MarkdownView *>(leaf->view());
        auto *editor = mv ? mv->editorWidget() : nullptr;
        if (editor) editor->applyReadableLineWidth(readable);
    }
}

void MainWindow::applyCanvasSettings()
{
    if (!m_workspace) return;
    auto *settings = CorbomiteSettings::self();
    const bool snapGrid = settings->snapToGrid();
    const bool snapObjects = settings->snapToObjects();
    const bool showGrid = settings->showGrid();
    for (auto *leaf : m_workspace->allLeaves()) {
        if (leaf->isDeferred()) continue;
        auto *cv = qobject_cast<CanvasFileView *>(leaf->view());
        auto *tab = cv ? cv->canvasWidget() : nullptr;
        if (!tab) continue;
        if (auto *scene = tab->canvasScene())
            if (auto *align = scene->alignmentStrategy()) {
                align->setSnapToGridEnabled(snapGrid);
                align->setSnapToObjectsEnabled(snapObjects);
            }
        if (auto *view = tab->canvasView())
            view->setGridVisible(showGrid);
    }
}

void MainWindow::onSettingsApplied()
{
    applyTheme();
    applyVaultPortableSettings();
    applyAutosaveDelay();
    applyReadableLineWidth();
    applyCanvasSettings();
}

} // namespace Corbomite
