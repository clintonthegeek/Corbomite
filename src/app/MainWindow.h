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

namespace Corbomite::MarkoffAdapters {
class LinkResolverAdapter;
class MetadataCacheAdapter;
class MetadataParserImpl;
}

namespace Markoff::Reading {
class EmbedRenderer;
}

namespace Corbomite {

class StatusBarRegistry;
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
    /// Exposed for integration tests that verify LinkResolver freshness.
    LinkResolver *linkResolver() const { return m_linkResolver; }

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
    void cycleEditorMode();
    void onInsertCallout();
    void onInsertTable();
    void onSetHeading(int level);
    void refreshEditorActions();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupActions();
    void applyTheme();
    void applyVaultPortableSettings();
    void applyAutosaveDelay();
    /// Iterates open leaves, calling each NoteEditorWidget::
    /// applyReadableLineWidth. MainWindow stays leaf-type-agnostic; the
    /// Markoff::Canvas type only appears inside NoteEditorWidget.cpp.
    void applyReadableLineWidth();
    void setupSidebars();
    void setupStatusBar();
    void setupEditor();
    void setupRibbonToolBar();

    // Action handlers
    void openVaultDialog();
    void closeVault();
    void createNewNote();
    void createNewCanvas(const QString &folder = QString());
    void saveCurrentNote();
    void showQuickSwitcher();
    void showCommandPalette();
    void showSearchPanel();
    void showSearchForQuery(const QString &query);
    void openGraphView();
    void insertTemplate();
    void openDailyNote();
    void onVaultOpened(const QString &path);
    void onVaultClosed();
    void onCursorInfoChanged(int line, int column, int wordCount);
    void updateVaultActions();
    void updateWindowTitle(NoteEditorWidget *editor = nullptr);
    void openFileInWorkspace(const QString &relativePath);
    MarkdownView *activeMarkdownView() const;
    Corbomite::Bases::BasesView *activeBasesView() const;
    CanvasFileView *activeCanvasView() const;
    NoteEditorWidget *activeEditor() const;

    void connectEditorContext(NoteEditorWidget *editor);
    void connectEditorContextMenu(NoteEditorWidget *editor);

    // Contract v2: format verbs + heading actions enabled iff the active
    // Markoff leaf advertises editing (hasEditing() — false in Reading,
    // false while read-only).
    void updateEditorActionStates();

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

    /// Cluster L Phase L4 (D2): refresh the global back/forward QActions'
    /// enabled state from the active leaf's `LeafHistory::canGoBack()`/
    /// `canGoForward()`. Called on `Workspace::activeLeafChanged` and on
    /// the active leaf's `viewChanged` (fired after every navigate/
    /// goBack/goForward) so the actions never go stale mid-leaf.
    void updateBackForwardActions();

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
    // D2 back/forward: global actions + the connection to the currently
    // active leaf's viewChanged, rebound on every activeLeafChanged.
    QAction *m_actionGoBack = nullptr;
    QAction *m_actionGoForward = nullptr;
    QMetaObject::Connection m_activeLeafHistoryConnection;
    // D3 tab commands: pin-tab/toggle-stacked reflect the active leaf's
    // state, rebound on every activeLeafChanged alongside D2's history hook.
    QAction *m_actionPinTab = nullptr;
    QAction *m_actionToggleStacked = nullptr;
    QMetaObject::Connection m_activeLeafPinnedConnection;

    /// D3: rebinds m_actionPinTab/m_actionToggleStacked's checked state to
    /// the active leaf. Called from the same Workspace::activeLeafChanged
    /// handler that drives updateBackForwardActions().
    void updateTabStateActions();
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
    // TODO(port-foundation-exploration): Reading::EmbedRenderer +
    // MarkoffAdapters::* unique_ptrs disabled — those types are forward-
    // declared in this header but their definitions are #if 0-out in
    // the .cpp / Adapters.h. unique_ptr dtor needs sizeof which can't
    // see incomplete types from this header. Re-enable when those types
    // are restored.
    // std::unique_ptr<Markoff::Reading::EmbedRenderer> m_embedRenderer;
    std::unique_ptr<Corbomite::Core::VaultResourceProvider> m_popoverResources;
    std::unique_ptr<Corbomite::Core::MermaidRenderer> m_mermaidRenderer;
    // P5.4 canvas mermaid seam — adapts m_mermaidRenderer's SVG-bytes-out
    // API to Markoff::Canvas::MermaidRenderer's pixmap-out contract. Built
    // once alongside m_mermaidRenderer; outlives every NoteEditorWidget.
    std::unique_ptr<Corbomite::CanvasMermaidAdapter> m_canvasMermaidAdapter;
    // Headless styled renderer handed to every CanvasFileView for card content.
    std::unique_ptr<Corbomite::StyledRenderEngine> m_cardRenderEngine;
    // std::unique_ptr<Corbomite::MarkoffAdapters::LinkResolverAdapter> m_linkResolverAdapter;
    // std::unique_ptr<Corbomite::MarkoffAdapters::MetadataCacheAdapter> m_metadataCacheAdapter;
    // std::unique_ptr<Corbomite::MarkoffAdapters::MetadataParserImpl> m_metadataParserImpl;
    EditorSuggestManager *m_suggestManager = nullptr;
    WikiLinkSuggest *m_wikiSuggest = nullptr;
    TagSuggest *m_tagSuggest = nullptr;
    RibbonToolBar *m_ribbonToolBar = nullptr;
    RibbonStateController *m_ribbonState = nullptr;

    // Cluster B Phase 1 — host-wide plugin extension registries.
    // PostProcessor and CodeBlockProcessor are functional registries that
    // plugins can register against; ReadingView dispatch wiring is a
    // post-Cluster-B follow-up (registrations are stored but not yet
    // consumed during render).
    std::unique_ptr<Corbomite::Core::PostProcessorRegistry> m_pluginPostProcessors;
    std::unique_ptr<Markoff::CodeBlockProcessorRegistry> m_pluginCodeBlocks;

    // Cluster B Phase 2 — host-wide status-bar registry.
    Corbomite::StatusBarRegistry *m_statusBarRegistry = nullptr;

    // Cluster Q (Tasks 13-20) — tool views hosting plugin createView()
    // output. Keyed by plugin id; the value is the QWidget the
    // CorbomiteMDI tool view holds. Used by releasePluginView to find
    // and tear down the tool view when the plugin unloads.
    QHash<QString, QWidget *> m_hostedPluginViews;
};

} // namespace Corbomite
