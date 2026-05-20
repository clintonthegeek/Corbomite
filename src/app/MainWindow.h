// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mdi/CorbomiteMDI.h"
#include <QHash>
#include <QLabel>
#include <QCloseEvent>
#include <QStackedWidget>

#include <markoff/Editor.h>
#include <markoff/core/EditorContext.h>

#include <memory>

class QMenu;

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
class MarkdownView;
class AutosaveReactor;
// FileWatchReactor forward decl removed — moved into Corbomite::detail::Watcher
// inside libs/vault/ during Q.0 Phase 2 Task 2.2. Re-exposed via Vault's
// public signal API in Q.0 Phase 7.
class SessionManager;
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
    void onSettingsApplied();
    void onZoomIn();
    void onZoomOut();
    void onZoomReset();
    void onAboutApp();
    void onAboutKde();
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
    void setupSidebars();
    void setupStatusBar();
    void setupEditor();
    void setupRibbonToolBar();

    // Action handlers
    void openVaultDialog();
    void closeVault();
    void createNewNote();
    void saveCurrentNote();
    void showQuickSwitcher();
    void showCommandPalette();
    void showSearchPanel();
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
    NoteEditorWidget *activeEditor() const;

    void connectEditorContext(NoteEditorWidget *editor);
    void connectEditorContextMenu(NoteEditorWidget *editor);

    /// Forward `id` to the active `MarkdownView`'s Markoff editor. No-op
    /// when the active view is not a MarkdownView.
    void triggerEditorAction(Markoff::ActionId id);
    bool confirmCloseUnsaved();
    void saveSessionState();
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
    std::unique_ptr<Markoff::Reading::EmbedRenderer> m_embedRenderer;
    std::unique_ptr<Corbomite::Core::VaultResourceProvider> m_popoverResources;
    // Phase C1 DI-seam adapters. Registry adapter retired in C4 Task 13
    // (m_embedRegistry is now Markoff::EmbedRegistry directly).
    // Remaining adapters rebuilt on each vault open (they close over
    // per-vault m_linkResolver / m_metadataCache).
    std::unique_ptr<Corbomite::Core::MermaidRenderer> m_mermaidRenderer;
    std::unique_ptr<Corbomite::MarkoffAdapters::LinkResolverAdapter>
        m_linkResolverAdapter;
    std::unique_ptr<Corbomite::MarkoffAdapters::MetadataCacheAdapter>
        m_metadataCacheAdapter;
    std::unique_ptr<Corbomite::MarkoffAdapters::MetadataParserImpl>
        m_metadataParserImpl;
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
