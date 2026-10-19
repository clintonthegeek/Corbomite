// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mdi/CorbomiteMDI.h"
#include <QHash>
#include <QLabel>
#include <QCloseEvent>
#include <QStackedWidget>

// TODO(port): old Markoff::Editor retired
// include <markoff/Editor.h>
#include <markoff/core/ActionId.h>
#include <markoff/core/EditorContext.h>

#include <memory>

class QMenu;
class QAction;

class KRecentFilesAction;
class KToolBar;
class QTimer;

namespace Markoff {
class EmbedRegistry;
class CodeBlockProcessorRegistry;
}

namespace Corbomite::Core {
class MermaidRenderer;
class VaultResourceProvider;
class ThemeService;
class PostProcessorRegistry;
}

namespace Markoff::Reading {
class EmbedRenderer;
}

namespace Canvas {
class CanvasLinkRewriter;
}

namespace Corbomite {

class StatusBarRegistry;
class MessageService;
class FileSystemAdapter;
class Vault;
class FileManager;
class VaultConfig;
class CorbomiteApp;
class Workspace;
class WorkspaceLeaf;
class NoteEditorWidget;
class CanvasMermaidAdapter;
class MarkdownView;
class RecoveryService;
namespace Bases { class BasesView; }
class CanvasFileView;
class AutosaveReactor;
// FileWatchReactor forward decl removed — moved into Corbomite::detail::Watcher
// inside libs/vault/ during Q.0 Phase 2 Task 2.2. Re-exposed via Vault's
// public signal API in Q.0 Phase 7.
class SessionManager;
class StyledRenderEngine;
class SQLiteIndex;
class MetadataCache;
class LinkResolver;
class TemplateService;
class DailyNoteService;
class WelcomeScreen;
class CommandRegistry;
class HotkeyDispatcher;
class MenuEventEmitter;
class HoverLinkSourceRegistry;
class HoverPopover;
class EditorSuggestManager;
class WikiLinkSuggest;
class TagSuggest;
class RibbonToolBar;
class RibbonStateController;
class View;
class ViewRegistry;
class Plugin;
class ActionContextController;
class MarkdownViewActions;
class BasesViewActions;
class CanvasViewActions;

class MainWindow : public CorbomiteMDI::MainWindow {
    Q_OBJECT

public:
    explicit MainWindow(CorbomiteApp *app, QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Accessor used by e2e tests to wire panels/editors defensively
    /// when propagateServicesToView hasn't fired yet under offscreen.
    Vault *vaultObj() const { return m_vaultObj; }
    FileManager *fileManager() const { return m_fileManager; }
    CommandRegistry *commandRegistry() const { return m_commandRegistry; }
    /// The `.obsidian/hotkeys.json` dispatcher. Exposed so tests can drive a
    /// vault's overrides without going through the file.
    HotkeyDispatcher *hotkeyDispatcher() const { return m_hotkeyDispatcher; }
    /// Exposed for integration tests that verify LinkResolver freshness.
    LinkResolver *linkResolver() const { return m_linkResolver; }
    /// Cluster O Phase O1.T1 — exposed for tests exercising action-state
    /// refresh directly (tst_action_context, tst_action_context_no_silent_noop).
    ActionContextController *actionContext() const { return m_actionContext; }
    /// Cluster O Phase O3 — exposed for tests exercising the ViewActions
    /// provider directly, including before any tab of its type is open
    /// (eager construction, O3.T2 — tst_view_actions_provider).
    MarkdownViewActions *markdownViewActions() const { return m_markdownViewActions; }
    /// Cluster O Phase O4 — same rationale as markdownViewActions() above.
    CanvasViewActions *canvasViewActions() const { return m_canvasViewActions; }
    BasesViewActions *basesViewActions() const { return m_basesViewActions; }

public Q_SLOTS:
    void onNoteActivated(const QString &relativePath);
    // Phase C6 — public so e2e/action-wiring tests can invoke with a
    // synthetic EditorContext without needing a live Markoff::Editor.
    void onEditorContextChanged(const Markoff::EditorContext &ctx);
    void onAboutToShowContextMenu(QMenu *menu,
                                  const Markoff::EditorContext &ctx,
                                  const QPoint &globalPos);

private Q_SLOTS:
    void onFind();
    void onReplace();
    void onFindNext();
    void onFindPrev();
    void onSettingsApplied();
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();
    void onAboutApp();
    // cycleEditorMode/onInsertCallout/onInsertTable/onSetHeading moved into
    // MarkdownViewActions (Cluster O Phase O3.T6).

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupActions();
    void applyTheme();
    /// Apply the `MarkoffTheme` kcfg entry to the live ThemeService.
    ///
    /// Was: `refreshSystemTheme` wired straight to kcfg's configChanged.
    /// That ONLY acts when the active name is "Follow system", so an
    /// explicitly-chosen editor theme (Light/Dark) was never re-applied when
    /// the setting changed at runtime — it was read once at construction. The
    /// user-visible symptom was that the editor-theme dropdown appeared to do
    /// nothing until the app-theme dropdown had been touched.
    void applyMarkoffThemeSetting();
    void applyVaultPortableSettings();
    void applyAutosaveDelay();
    /// Iterates open leaves, calling each NoteEditorWidget::
    /// applyReadableLineWidth. MainWindow stays leaf-type-agnostic; the
    /// Markoff::Canvas type only appears inside NoteEditorWidget.cpp.
    void applyReadableLineWidth();
    /// Cluster O Phase O4 (O4.T4) — fans the Canvas kcfg group's three
    /// snap/grid toggles out to EVERY open canvas leaf's
    /// CanvasAlignmentStrategy/CanvasView, not just the focused one (same
    /// shape as applyReadableLineWidth() above).
    void applyCanvasSettings();
    void applyRecoverySettings();
    void setupSidebars();
    void setupStatusBar();
    void setupEditor();
    void setupRibbonToolBar();
    /// Cluster O Phase O3 (§D5) — keep Settings/Help at the tail of the
    /// menubar.
    ///
    /// A ViewActions provider's top-level menu (Format/Heading/Insert/Table,
    /// Graph, Bases, Canvas) is a *new* container for a child KXMLGUIClient,
    /// so KXMLGUI appends it to the end of the menubar — past the Settings
    /// and Help menus the framework adds itself, giving "File Edit View Go
    /// Tab Settings Help Graph". No rc attribute can ask for a middle
    /// position: `append="X"` is only honoured inside a `<MergeLocal
    /// name="X"/>` element, and that element is *removed* by whichever client
    /// merges first, whether or not it used the point
    /// (KXMLGUIClient::mergeXML). So the order is enforced here instead:
    /// providers keep KXMLGUI's natural tail, and these two menus are
    /// re-appended after every factory rebuild (Settings then Help, so the
    /// pair stays together and rightmost).
    void moveStandardMenusLast();

    // Action handlers
    void openVaultDialog();
    void closeVault();
    void createNewNote();
    void createNewCanvas(const QString &folder = QString());
    void createNewBase(const QString &folder = QString());
    void saveCurrentNote();
    void showQuickSwitcher();
    /// The quick switcher's create row: open the typed name if the vault
    /// already holds it (the switcher lists notes only, so `.canvas`/`.base`
    /// files and attachments are invisible to it), otherwise create what the
    /// name asks for — a canvas, a base, or a markdown note.
    void createFromQuickSwitcher(const QString &typedName);
    void showCommandPalette();
    void showSearchPanel();
    void showSearchForQuery(const QString &query);
    void openGraphView();
    void insertTemplate();
    void openDailyNote();
    void onVaultOpened(const QString &path);
    void onVaultClosed();
    /// Re-read `.obsidian/hotkeys.json` and re-arm the dispatcher. Called on
    /// vault open and on every external edit to the file.
    void reloadVaultHotkeys(const QString &path);
    void onCursorInfoChanged(int line, int column, int wordCount);
    void updateWindowTitle(NoteEditorWidget *editor = nullptr);
    void openFileInWorkspace(const QString &relativePath);
    MarkdownView *activeMarkdownView() const;
    Corbomite::Bases::BasesView *activeBasesView() const;
    CanvasFileView *activeCanvasView() const;
    NoteEditorWidget *activeEditor() const;

    void connectEditorContextMenu(NoteEditorWidget *editor);

    /// Forward `id` to the active `MarkdownView`'s Markoff editor. No-op
    /// when the active view is not a MarkdownView.
    void triggerEditorAction(Markoff::ActionId id);
    bool confirmCloseUnsaved();
    void saveSessionState();

    /// Resolves `relativePath` to an existing note, eagerly creating it
    /// (Obsidian create-on-click parity) if it does not exist and canvas
    /// files are excluded from creation. Returns the final relative path,
    /// or an empty string if creation was needed and failed. Shared by
    /// `onNoteActivated` (always a new/reused leaf) and
    /// `navigateActiveLeafTo` (in-place navigation) so the create-on-click
    /// behavior can't drift between the two entry points.
    QString resolveOrCreateNoteTarget(const QString &relativePath);

    /// Plain-click wikilink navigation: unlike `onNoteActivated` (which
    /// always opens a new leaf, or switches to one where the file is
    /// already open), this navigates the CURRENTLY ACTIVE leaf in place via
    /// `WorkspaceLeaf::navigate()` — which also pushes the leaf's own
    /// back/forward history, the mechanism the tab-frame's nav buttons
    /// already read from but that nothing previously called into for link
    /// clicks. Falls back to `onNoteActivated` if there is no active leaf
    /// to navigate (e.g. no vault open yet).
    void navigateActiveLeafTo(const QString &relativePath);
    void propagateServicesToView(View *view);

    /// Look up the loaded plugin by id and host its createView() output
    /// into a tool view determined by X-Corbomite-DockArea metadata.
    /// Triggered from PluginManager::pluginLoaded.
    void hostPluginView(const QString &pluginId);

    /// Tear down the tool view created for `pluginId` (if any).
    /// Triggered from PluginManager::pluginUnloading.
    void releasePluginView(const QString &pluginId);

    /// Wire the granted core services on every plugin context — Vault,
    /// FileManager, MetadataCache, Workspace, CommandRegistry,
    /// ViewRegistry, MenuEventEmitter. Plugin must subscribe to whatever
    /// it needs via the proxies. Called when those services materialise
    /// (post onVaultOpened) so plugins loaded earlier in startup don't
    /// see nullptr.
    void rewirePluginCoreServices();

    CorbomiteApp *m_app;
    // Cluster O Phase O1.T1 — owns Tier B/C action-state refresh (formerly
    // five scattered MainWindow methods). actionCollection() ownership
    // stays here (KXmlGuiWindow requirement); the controller is handed the
    // pointer and rebinds itself to the active leaf on every
    // activeLeafChanged (see the Workspace::activeLeafChanged connection
    // in setupEditor()).
    ActionContextController *m_actionContext = nullptr;
    // Q.0 P6 — canonical Vault aggregate created alongside the legacy
    // VaultModel during the consumer-migration wave. Both coexist until
    // Phase 10 deletes VaultModel. Owned by `this` (QObject parent).
    std::unique_ptr<FileSystemAdapter> m_fsAdapter;
    Vault *m_vaultObj = nullptr;
    // Vault-scoped VaultConfig — bound to PluginManager for the lifetime
    // of the open vault so enable/disable round-trips through
    // .obsidian/{core,community}-plugins.json. Built per-vault-open in
    // onVaultOpened, cleared in onVaultClosed.
    std::unique_ptr<VaultConfig> m_pluginVaultConfig;
    FileManager *m_fileManager = nullptr;
    Workspace *m_workspace = nullptr;
    // D2 back/forward + D3 tab commands: the QActions themselves stay here
    // (created/connected in setupActions()); their enabled/checked state is
    // now refreshed by ActionContextController on every activeLeafChanged
    // (formerly updateBackForwardActions()/updateTabStateActions()).
    QAction *m_actionGoBack = nullptr;
    QAction *m_actionGoForward = nullptr;
    QAction *m_actionPinTab = nullptr;
    QAction *m_actionToggleStacked = nullptr;
    // Stable QWidget wrapper for the Workspace widget tree; owned by
    // m_centralStack so that Workspace::layoutChanged restructurings can
    // re-parent the root widget inside this container without needing to
    // remove/re-add it from the stack.
    QWidget *m_workspaceContainer = nullptr;
    ViewRegistry *m_viewRegistry = nullptr;
    QLabel *m_wordCountLabel = nullptr;
    QLabel *m_cursorPosLabel = nullptr;
    AutosaveReactor *m_autosave = nullptr;
    // m_fileWatch removed — FileWatchReactor moved to libs/vault as private
    // Corbomite::detail::Watcher. Re-wired to Vault's public signal API in
    // Q.0 Phase 7.
    SessionManager *m_sessionManager = nullptr;
    SQLiteIndex *m_searchIndex = nullptr;
    MetadataCache *m_metadataCache = nullptr;
    LinkResolver *m_linkResolver = nullptr;
    KRecentFilesAction *m_recentVaults = nullptr;
    TemplateService *m_templateService = nullptr;
    DailyNoteService *m_dailyNoteService = nullptr;
    QStackedWidget *m_centralStack = nullptr;
    WelcomeScreen *m_welcomeScreen = nullptr;
    CommandRegistry *m_commandRegistry = nullptr;
    // `.obsidian/hotkeys.json` → real shortcuts. Anchors the merged table to
    // ScopeManager's root slot and moves each overridden command's QAction
    // shortcut; vault-scoped (loaded on open, reset on close).
    HotkeyDispatcher *m_hotkeyDispatcher = nullptr;
    QString m_hotkeyPath;
    QTimer *m_hotkeyReloadTimer = nullptr;
    MenuEventEmitter *m_menuEvents = nullptr;
    HoverLinkSourceRegistry *m_hoverSources = nullptr;
    HoverPopover *m_hoverPopover = nullptr;
    Corbomite::Core::ThemeService *m_themeService = nullptr;
    // Cluster J Phase 6 — EmbedRegistry + EmbedRenderer feed HoverPopover
    // (and any future preview surfaces). Built once at MainWindow construction;
    // the per-vault resource adapter is rebuilt on each `onVaultOpened` and
    // released on `onVaultClosed`.
    // C4 Task 13: m_embedRegistry is now the canonical Markoff::EmbedRegistry
    // (concrete, no adapter needed). EmbedRegistryAdapter removed; callers use
    // m_embedRegistry directly as Markoff::EmbedRegistry.
    std::unique_ptr<Markoff::EmbedRegistry> m_embedRegistry;
    // 2026-10-01: the MarkoffAdapters::* members that used to sit here are
    // gone, along with the whole adapter layer they pointed at (the three
    // Markoff base classes they derived from no longer exist in the pinned
    // submodule, so the file could not have compiled if re-enabled). A
    // commented-out Reading::EmbedRenderer remains for the same reason:
    // Markoff::Reading was retired with the QML leaf and there is no
    // EmbedRenderer type to hold. unique_ptr needs sizeof, so a member for
    // an absent type cannot simply be uncommented. Re-add when a live
    // substitute for embed rendering actually lands (see the punch list's
    // "registerEmbed API is a no-op" item).
    std::unique_ptr<Corbomite::Core::VaultResourceProvider> m_popoverResources;
    std::unique_ptr<Corbomite::Core::MermaidRenderer> m_mermaidRenderer;
    // P5.4 canvas mermaid seam — adapts m_mermaidRenderer's SVG-bytes-out
    // API to Markoff::Canvas::MermaidRenderer's pixmap-out contract. Built
    // once alongside m_mermaidRenderer; outlives every NoteEditorWidget.
    std::unique_ptr<Corbomite::CanvasMermaidAdapter> m_canvasMermaidAdapter;
    // Headless styled renderer handed to every CanvasFileView for card content.
    std::unique_ptr<Corbomite::StyledRenderEngine> m_cardRenderEngine;
    // M6.2 — canvas link rewriter registered with FileManager so a vault
    // rename updates `.canvas` structural references (linkUpdaters seam).
    std::unique_ptr<Canvas::CanvasLinkRewriter> m_canvasLinkRewriter;
    // Cluster F-a P0 — host-level periodic File Recovery snapshot service,
    // created on vault open and stopped/reset on close.
    std::unique_ptr<Corbomite::RecoveryService> m_recovery;
    EditorSuggestManager *m_suggestManager = nullptr;
    WikiLinkSuggest *m_wikiSuggest = nullptr;
    TagSuggest *m_tagSuggest = nullptr;
    RibbonToolBar *m_ribbonToolBar = nullptr;
    RibbonStateController *m_ribbonState = nullptr;

    // Cluster O Phase O3 — markdown's ViewActions provider (Tier A) + its
    // persistent toolbar (§D4). Both eagerly constructed at startup;
    // ActionContextController drives dynamic client install/toolbar
    // visibility (see setGuiFactory()/registerProvider()/registerToolBar()).
    MarkdownViewActions *m_markdownViewActions = nullptr;
    KToolBar *m_markdownToolBar = nullptr;
    CanvasViewActions *m_canvasViewActions = nullptr;
    BasesViewActions *m_basesViewActions = nullptr;
    QAction *m_actionLinkWithActivePane = nullptr;
    KToolBar *m_canvasToolBar = nullptr;

    // Cluster B Phase 1 — host-wide plugin extension registries.
    // PostProcessor and CodeBlockProcessor are functional registries that
    // plugins can register against; ReadingView dispatch wiring is a
    // post-Cluster-B follow-up (registrations are stored but not yet
    // consumed during render).
    std::unique_ptr<Corbomite::Core::PostProcessorRegistry> m_pluginPostProcessors;
    std::unique_ptr<Markoff::CodeBlockProcessorRegistry> m_pluginCodeBlocks;

    // Cluster B Phase 2 — host-wide status-bar registry.
    Corbomite::StatusBarRegistry *m_statusBarRegistry = nullptr;
    /// The app's user-message channel (status bar + alert entry + desktop
    /// notification). Owned here; every view reaches it through
    /// `MessageService::forContext()`.
    MessageService *m_messages = nullptr;

    // Cluster Q (Tasks 13-20) — tool views hosting plugin createView()
    // output. Keyed by plugin id; the value is the QWidget the
    // CorbomiteMDI tool view holds. Used by releasePluginView to find
    // and tear down the tool view when the plugin unloads.
    QHash<QString, QWidget *> m_hostedPluginViews;
};

} // namespace Corbomite
