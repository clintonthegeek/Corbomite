// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "WelcomeScreen.h"
#include "CorbomiteApp.h"
#include "editor/NoteEditorWidget.h"
#include <markoff/Editor.h>
#include <markoff/source/SourceEditor.h>
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/Command.h"
#include "corbomite/core/EditableFileView.h"
#include "corbomite/core/EmptyView.h"
#include "corbomite/core/FileView.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/TextFileView.h"
#include "canvas/CanvasViewTab.h"
#include <canvas/CanvasDocument.h>
#include "corbomite/bases/BasesView.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/Plugin.h"
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
#include <markoff/EmbedRegistry.h>
#include "corbomite/core/MermaidRenderer.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/View.h"
#include "corbomite/markoff_adapters/Adapters.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "ExportToPdf.h"
#include "editor/MarkdownView.h"
#include "canvas/CanvasFileView.h"
#include "corbomite/core/HoverLinkSourceRegistry.h"
#include "corbomite/core/MenuEventEmitter.h"
#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/VaultResourceProvider.h"
#include "markoff/reading/EmbedRenderer.h"
#include "editor/HoverPopover.h"
#include "editor/TagSuggest.h"
#include "editor/WikiLinkSuggest.h"
#include "dialogs/CalloutPickerDialog.h"
#include "dialogs/CreateVaultDialog.h"
#include "dialogs/InsertTableDialog.h"
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
#include <KHelpMenu>
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
#include <QActionGroup>
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

    setupActions();
    setupEditor();
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

    m_menuEvents = new MenuEventEmitter(this);
    m_hoverSources = new HoverLinkSourceRegistry(this);
    m_hoverSources->registerBuiltins();
    m_hoverPopover = new HoverPopover(this);
    // Vault binding deferred to onVaultOpened — no vault exists yet.

    m_embedRegistry = std::make_unique<Markoff::EmbedRegistry>();
    m_mermaidRenderer = std::make_unique<Corbomite::Core::MermaidRenderer>();
    m_embedRenderer = std::make_unique<Markoff::Reading::EmbedRenderer>(
        m_embedRegistry.get(), /*cache=*/nullptr,
        /*resources=*/nullptr);
    Markoff::Reading::registerBuiltinEmbedFactories(*m_embedRegistry,
                                                    *m_embedRenderer);
    m_hoverPopover->setEmbedRenderer(m_embedRenderer.get());

    m_suggestManager = new EditorSuggestManager(this);
    // Suggesters start nullptr-bound; MainWindow rebinds on vault
    // open/close via their setters.
    m_wikiSuggest = new WikiLinkSuggest(nullptr);
    m_tagSuggest = new TagSuggest(nullptr);
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

    m_embedRenderer.reset();
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

    // C7: Find/Replace IDs route to the active leaf's MarkdownView virtuals
    // (Live and Source both override; Reading inherits no-op). Source's
    // SearchController + named QAction accessors handle next/prev when a
    // query is set; otherwise both leaves open the find bar.
    auto *leaf = editor->activeLeaf();
    if (leaf) {
        switch (id) {
            case Markoff::ActionId::FindNext:
                if (auto *src = qobject_cast<Markoff::Source::SourceEditor *>(leaf))
                    if (auto *act = src->findNextAction()) { act->trigger(); return; }
                leaf->showFindBar();
                return;
            case Markoff::ActionId::FindPrevious:
                if (auto *src = qobject_cast<Markoff::Source::SourceEditor *>(leaf))
                    if (auto *act = src->findPrevAction()) { act->trigger(); return; }
                leaf->showFindBar();
                return;
            case Markoff::ActionId::Replace:
                leaf->showReplaceBar();
                return;
            default:
                break;
        }
    }

    // Fallback for non-Find/Replace IDs: dispatch via Live's action() map.
    if (!editor->editor()) return;
    if (auto *act = editor->editor()->action(id))
        act->trigger();
}

void MainWindow::onSetHeading(int level)
{
    auto *editor = activeEditor();
    if (!editor || !editor->editor()) return;
    // SetHeading1..SetHeading6 are consecutive in ActionId.
    const auto id = static_cast<Markoff::ActionId>(
        static_cast<int>(Markoff::ActionId::SetHeading1) + (level - 1));
    if (auto *act = editor->editor()->action(id))
        act->trigger();
}

void MainWindow::onInsertCallout()
{
    auto *editor = activeEditor();
    if (!editor || !editor->editor()) return;
    CalloutPickerDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    editor->editor()->insertCallout(dlg.selectedType(), dlg.title());
}

void MainWindow::onInsertTable()
{
    auto *editor = activeEditor();
    if (!editor || !editor->editor()) return;
    InsertTableDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    editor->editor()->insertTable(dlg.rows(), dlg.cols(), dlg.firstRowAsHeader());
}

void MainWindow::refreshEditorActions()
{
    KActionCollection *ac = actionCollection();
    auto *mv = activeMarkdownView();
    const bool isMarkdown = mv != nullptr;

    // Every action that requires a MarkdownView active.
    static const QStringList editorActionIds = {
        QStringLiteral("edit_find_next"), QStringLiteral("edit_find_previous"),
        QStringLiteral("edit_replace"),
        QStringLiteral("format_bold"), QStringLiteral("format_italic"),
        QStringLiteral("format_strikethrough"), QStringLiteral("format_inline_code"),
        QStringLiteral("insert_link"), QStringLiteral("insert_wiki_link"),
        QStringLiteral("insert_image"), QStringLiteral("insert_code_block"),
        QStringLiteral("insert_block_quote"), QStringLiteral("insert_horizontal_rule"),
        QStringLiteral("toggle_checkbox"),
        QStringLiteral("heading_1"), QStringLiteral("heading_2"),
        QStringLiteral("heading_3"), QStringLiteral("heading_4"),
        QStringLiteral("heading_5"), QStringLiteral("heading_6"),
        QStringLiteral("heading_increase"), QStringLiteral("heading_decrease"),
        QStringLiteral("insert_table"), QStringLiteral("insert_callout"),
        QStringLiteral("fold_all"), QStringLiteral("unfold_all"),
        QStringLiteral("toggle_fold"),
        QStringLiteral("editor_toggle_mode"),
    };
    for (const auto &id : editorActionIds) {
        if (auto *act = ac->action(id)) act->setEnabled(isMarkdown);
    }

    // Table submenu is additionally gated on cursorInTable().
    const bool inTable = isMarkdown && mv->editorWidget()
                         && mv->editorWidget()->editor()
                         && mv->editorWidget()->editor()->cursorInTable();
    static const QStringList tableActionIds = {
        QStringLiteral("table_row_above"), QStringLiteral("table_row_below"),
        QStringLiteral("table_col_left"),  QStringLiteral("table_col_right"),
        QStringLiteral("table_delete_row"),QStringLiteral("table_delete_col"),
    };
    for (const auto &id : tableActionIds) {
        if (auto *act = ac->action(id)) act->setEnabled(inTable);
    }

    // Reflect the current heading level in the H1..H6 radio group.
    if (isMarkdown && mv->editorWidget() && mv->editorWidget()->editor()) {
        const int level = mv->editorWidget()->editor()->currentHeadingLevel();
        for (int i = 1; i <= 6; ++i) {
            if (auto *act = ac->action(QStringLiteral("heading_%1").arg(i)))
                act->setChecked(i == level);
        }
    } else {
        for (int i = 1; i <= 6; ++i) {
            if (auto *act = ac->action(QStringLiteral("heading_%1").arg(i)))
                act->setChecked(false);
        }
    }
}

void MainWindow::connectEditorContext(NoteEditorWidget *editor)
{
    if (!editor) return;
    auto *ed = editor->editor();
    if (!ed) return;
    connect(ed, &Markoff::Editor::contextChanged,
            this, &MainWindow::onEditorContextChanged,
            Qt::UniqueConnection);
    // Prime initial state so the toolbar reflects the snapshot even
    // before the first cursor movement.
    onEditorContextChanged(ed->context());
}

void MainWindow::onEditorContextChanged(const Markoff::EditorContext &ctx)
{
    KActionCollection *ac = actionCollection();
    if (!ac) return;

    auto setCheck = [ac](const QString &id, bool on) {
        if (auto *a = ac->action(id)) a->setChecked(on);
    };
    auto setEnable = [ac](const QString &id, bool on) {
        if (auto *a = ac->action(id)) a->setEnabled(on);
    };

    using BK = Markoff::EditorContext::BlockKind;

    // Format toolbar check-state
    setCheck(QStringLiteral("format_bold"),          ctx.inBold);
    setCheck(QStringLiteral("format_italic"),        ctx.inItalic);
    setCheck(QStringLiteral("format_strikethrough"), ctx.inStrikethrough);
    setCheck(QStringLiteral("format_inline_code"),   ctx.inInlineCode);

    // Heading radio
    for (int i = 1; i <= 6; ++i) {
        setCheck(QStringLiteral("heading_%1").arg(i),
                 ctx.headingLevel == i);
    }
    setEnable(QStringLiteral("heading_increase"),
              !ctx.readOnly && ctx.headingLevel < 6);
    setEnable(QStringLiteral("heading_decrease"),
              !ctx.readOnly && ctx.headingLevel >= 1);

    // Table delete gating — more precise than the existing
    // refreshEditorActions path which only gates on cursorInTable().
    const bool inTable = (ctx.blockKind == BK::Table);
    if (ctx.table) {
        setEnable(QStringLiteral("table_delete_row"),
                  !ctx.readOnly && inTable && ctx.table->rows > 1);
        setEnable(QStringLiteral("table_delete_col"),
                  !ctx.readOnly && inTable && ctx.table->cols > 1);
    }

    // Fold-at-cursor only on headings
    setEnable(QStringLiteral("toggle_fold"),
              ctx.blockKind == BK::Heading);
}

void MainWindow::connectEditorContextMenu(NoteEditorWidget *editor)
{
    if (!editor) return;
    auto *ed = editor->editor();
    if (!ed) return;
    connect(ed, &Markoff::Editor::aboutToShowContextMenu,
            this, &MainWindow::onAboutToShowContextMenu,
            Qt::UniqueConnection);
}

void MainWindow::onAboutToShowContextMenu(QMenu *menu,
                                          const Markoff::EditorContext &ctx,
                                          const QPoint & /*globalPos*/)
{
    if (!menu) return;
    using BK = Markoff::EditorContext::BlockKind;
    Corbomite::MenuSectionHelper helper(menu);
    KActionCollection *ac = actionCollection();
    if (!ac) return;

    auto add = [&](const QString &section, const QString &id) {
        if (auto *a = ac->action(id)) helper.addToSection(a, section);
    };

    // Format section — always available when active view is a MarkdownView.
    add(QStringLiteral("action"), QStringLiteral("format_bold"));
    add(QStringLiteral("action"), QStringLiteral("format_italic"));
    add(QStringLiteral("action"), QStringLiteral("format_strikethrough"));
    add(QStringLiteral("action"), QStringLiteral("format_inline_code"));

    // Heading / Insert — grouped into the "action" section after format
    // entries (canonical section ordering means the helper flushes them
    // with a single separator gap from built-ins).
    for (int i = 1; i <= 6; ++i)
        add(QStringLiteral("action"), QStringLiteral("heading_%1").arg(i));
    add(QStringLiteral("action"), QStringLiteral("insert_link"));
    add(QStringLiteral("action"), QStringLiteral("insert_wiki_link"));
    add(QStringLiteral("action"), QStringLiteral("insert_callout"));
    add(QStringLiteral("action"), QStringLiteral("insert_table"));

    // Context-specific entries keyed off the snapshot.
    if (ctx.blockKind == BK::Table) {
        add(QStringLiteral("action"), QStringLiteral("table_row_above"));
        add(QStringLiteral("action"), QStringLiteral("table_row_below"));
        add(QStringLiteral("action"), QStringLiteral("table_col_left"));
        add(QStringLiteral("action"), QStringLiteral("table_col_right"));
        add(QStringLiteral("action"), QStringLiteral("table_delete_row"));
        add(QStringLiteral("action"), QStringLiteral("table_delete_col"));
    }

    helper.finalize();
}

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

void MainWindow::onFind()
{
    // Route Ctrl+F to Markoff::Editor's Find action when the active view is a
    // Markdown view in LivePreview mode. Source-mode Find is a future Qutepart
    // fork Phase 3 concern; ReadingView has no search bar yet.
    auto *editor = activeEditor();
    if (!editor) return;
    if (auto *markoff = editor->editor()) {
        if (auto *act = markoff->action(Markoff::ActionId::Find)) act->trigger();
    }
}

void MainWindow::onZoomIn()
{
    auto *leaf = m_workspace ? m_workspace->activeLeaf() : nullptr;
    if (auto *v = leaf ? leaf->view() : nullptr) v->zoomIn();
}

void MainWindow::onZoomOut()
{
    auto *leaf = m_workspace ? m_workspace->activeLeaf() : nullptr;
    if (auto *v = leaf ? leaf->view() : nullptr) v->zoomOut();
}

void MainWindow::onZoomReset()
{
    auto *leaf = m_workspace ? m_workspace->activeLeaf() : nullptr;
    if (auto *v = leaf ? leaf->view() : nullptr) v->zoomReset();
}

void MainWindow::onAboutApp()
{
    KAboutApplicationDialog dlg(KAboutData::applicationData(), this);
    dlg.exec();
}

void MainWindow::onAboutKde()
{
    // KF6 removed KAboutKdeDialog; KHelpMenu::aboutKDE() opens the canonical
    // "About KDE" dialog. KHelpMenu owns the dialog it spawns, so we parent
    // the helper to `this` to outlive this function call.
    auto *helpMenu = new KHelpMenu(this);
    helpMenu->aboutKDE();
}

void MainWindow::cycleEditorMode()
{
    auto *mv = activeMarkdownView();
    if (!mv) return;
    auto *w = mv->editorWidget();
    if (!w) return;
    using VM = NoteEditorWidget::ViewMode;
    switch (w->viewMode()) {
        case VM::Source:      w->setViewMode(VM::LivePreview); break;
        case VM::LivePreview: w->setViewMode(VM::Reading);     break;
        case VM::Reading:     w->setViewMode(VM::Source);      break;
    }
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

    if (auto *cv = qobject_cast<CanvasFileView *>(view)) {
        // Cluster R Task 3.6 — hamburger-menu command dispatcher.
        auto *cmds = m_commandRegistry;
        cv->setCanvasCommandDispatcher([cmds](const QString &commandId) {
            if (cmds) cmds->executeById(commandId);
        });
        return;
    }

    if (auto *bv = qobject_cast<Corbomite::Bases::BasesView *>(view)) {
        bv->setServices(m_vaultObj, m_metadataCache, m_fileManager);
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
        SettingsDialog dialog(m_app->pluginManager(), m_themeService, this);
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
    connect(newCanvas, &QAction::triggered, this, [this]() {
        if (!m_app->isOpen()) return;
        bool ok;
        QString name = QInputDialog::getText(this, i18n("New Canvas"),
                                              i18n("Canvas name:"), QLineEdit::Normal,
                                              QString(), &ok);
        if (ok && !name.isEmpty()) {
            QString relPath = name + QStringLiteral(".canvas");
            QString absPath = m_vaultObj->basePath() + QLatin1Char('/') + relPath;
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

    KStandardAction::find(this, &MainWindow::onFind, ac);

    KStandardAction::aboutApp(this, &MainWindow::onAboutApp, ac);
    KStandardAction::aboutKDE(this, &MainWindow::onAboutKde, ac);

    auto *toggleLeft = ac->addAction(QStringLiteral("view_toggle_left_sidebar"));
    toggleLeft->setText(i18n("Toggle Left Sidebar"));
    ac->setDefaultShortcut(toggleLeft, QKeySequence(Qt::CTRL | Qt::Key_Backslash));
    connect(toggleLeft, &QAction::triggered, this, [this]() {
        setSidebarsVisible(!sidebarsVisible());
    });

    auto *zoomIn = ac->addAction(QStringLiteral("view_zoom_in"));
    zoomIn->setText(i18n("Zoom In"));
    zoomIn->setIcon(QIcon::fromTheme(QStringLiteral("zoom-in")));
    ac->setDefaultShortcut(zoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal));
    connect(zoomIn, &QAction::triggered, this, &MainWindow::onZoomIn);

    auto *zoomOut = ac->addAction(QStringLiteral("view_zoom_out"));
    zoomOut->setText(i18n("Zoom Out"));
    zoomOut->setIcon(QIcon::fromTheme(QStringLiteral("zoom-out")));
    ac->setDefaultShortcut(zoomOut, QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(zoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);

    auto *zoomReset = ac->addAction(QStringLiteral("view_zoom_reset"));
    zoomReset->setText(i18n("Reset Zoom"));
    zoomReset->setIcon(QIcon::fromTheme(QStringLiteral("zoom-original")));
    ac->setDefaultShortcut(zoomReset, QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(zoomReset, &QAction::triggered, this, &MainWindow::onZoomReset);

    {
        auto *toggleMode = ac->addAction(QStringLiteral("editor_toggle_mode"));
        toggleMode->setText(i18n("Toggle Editor Mode"));
        toggleMode->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
        ac->setDefaultShortcut(toggleMode, QKeySequence(Qt::CTRL | Qt::Key_E));
        connect(toggleMode, &QAction::triggered, this, &MainWindow::cycleEditorMode);
    }

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

    // View > Editor Mode submenu — three checkable radio actions that
    // directly select one of the three ViewModes. Keeps the pre-existing
    // action object names (referenced by e2e tests + API-REFERENCE).
    // Check-state is kept in sync with the active MarkdownView via the
    // NoteEditorWidget::viewModeChanged signal (hooked up below in the
    // activeLeafChanged connection).
    auto *modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);
    auto addModeAction = [this, ac, modeGroup](
        const QString &id, const QString &label, const QString &icon,
        NoteEditorWidget::ViewMode mode) {
        auto *act = ac->addAction(id);
        act->setText(label);
        act->setIcon(QIcon::fromTheme(icon));
        act->setCheckable(true);
        act->setActionGroup(modeGroup);
        connect(act, &QAction::triggered, this, [this, mode]() {
            if (auto *editor = activeEditor())
                editor->setViewMode(mode);
        });
        return act;
    };
    // Ctrl+E is owned by `editor_toggle_mode` (3-way cycle per spec §3.3)
    // — no per-mode shortcut is registered here.
    addModeAction(QStringLiteral("view_source_mode"),  i18n("Source"),
                  QStringLiteral("text-plain"),
                  NoteEditorWidget::ViewMode::Source);
    addModeAction(QStringLiteral("view_editing_mode"), i18n("Live Preview"),
                  QStringLiteral("text-x-markdown"),
                  NoteEditorWidget::ViewMode::LivePreview);
    addModeAction(QStringLiteral("view_reading_mode"), i18n("Reading"),
                  QStringLiteral("view-preview"),
                  NoteEditorWidget::ViewMode::Reading);

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

    // -----------------------------------------------------------------
    // Cluster V Phase 2+3 — Markoff editor actions (Format/Heading/
    // Insert/Table/Fold/Edit Find extensions). Each entry registers a
    // KActionCollection slot that forwards to the active MarkdownView's
    // Markoff::Editor. Enable-state is maintained by refreshEditorActions()
    // on Workspace::activeLeafChanged (and cursor-moved for the Table
    // submenu). KCommandBar palette picks these up automatically via its
    // action-collection walk in MainWindow::showCommandPalette.
    // -----------------------------------------------------------------

    using Id = Markoff::ActionId;
    auto addEditorAction = [this, ac](const QString &objName, Id id,
                                      const QString &icon, const QString &label,
                                      const QKeySequence &shortcut = {}) -> QAction* {
        auto *act = ac->addAction(objName);
        act->setText(label);
        if (!icon.isEmpty())
            act->setIcon(QIcon::fromTheme(icon));
        if (!shortcut.isEmpty())
            ac->setDefaultShortcut(act, shortcut);
        connect(act, &QAction::triggered, this,
                [this, id]() { triggerEditorAction(id); });
        return act;
    };

    // Edit > Find / Find Next / Find Previous / Replace
    addEditorAction(QStringLiteral("edit_find_next"), Id::FindNext,
                    QStringLiteral("go-down-search"), i18n("Find Next"),
                    QKeySequence::FindNext);
    addEditorAction(QStringLiteral("edit_find_previous"), Id::FindPrevious,
                    QStringLiteral("go-up-search"), i18n("Find Previous"),
                    QKeySequence::FindPrevious);
    addEditorAction(QStringLiteral("edit_replace"), Id::Replace,
                    QStringLiteral("edit-find-replace"), i18n("Replace..."),
                    QKeySequence::Replace);

    // Format: Bold / Italic / Strikethrough / Inline code — all checkable
    // so MainWindow::onEditorContextChanged (C6) can reflect the current
    // inline-span state on the toolbar/menubar.
    if (auto *a = addEditorAction(QStringLiteral("format_bold"), Id::ToggleBold,
                    QStringLiteral("format-text-bold"), i18n("Bold"),
                    QKeySequence::Bold))
        a->setCheckable(true);
    if (auto *a = addEditorAction(QStringLiteral("format_italic"), Id::ToggleItalic,
                    QStringLiteral("format-text-italic"), i18n("Italic"),
                    QKeySequence::Italic))
        a->setCheckable(true);
    if (auto *a = addEditorAction(QStringLiteral("format_strikethrough"), Id::ToggleStrikethrough,
                    QStringLiteral("format-text-strikethrough"), i18n("Strikethrough"),
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X)))
        a->setCheckable(true);
    if (auto *a = addEditorAction(QStringLiteral("format_inline_code"), Id::ToggleInlineCode,
                    QStringLiteral("code-context"), i18n("Inline Code"),
                    QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft)))
        a->setCheckable(true);

    // Format/Insert: links + block elements (no dialog)
    addEditorAction(QStringLiteral("insert_link"), Id::InsertLink,
                    QStringLiteral("insert-link"), i18n("Insert Link"),
                    QKeySequence(Qt::CTRL | Qt::Key_K));
    addEditorAction(QStringLiteral("insert_wiki_link"), Id::InsertWikiLink,
                    QStringLiteral("insert-link"), i18n("Insert Wiki Link"),
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    addEditorAction(QStringLiteral("insert_image"), Id::InsertImage,
                    QStringLiteral("insert-image"), i18n("Insert Image"));
    addEditorAction(QStringLiteral("insert_code_block"), Id::InsertCodeBlock,
                    QStringLiteral("code-block"), i18n("Insert Code Block"));
    addEditorAction(QStringLiteral("insert_block_quote"), Id::InsertBlockQuote,
                    QStringLiteral("format-text-blockquote"), i18n("Insert Block Quote"));
    addEditorAction(QStringLiteral("insert_horizontal_rule"), Id::InsertHorizontalRule,
                    QStringLiteral("distribute-horizontal-center"), i18n("Insert Horizontal Rule"));
    addEditorAction(QStringLiteral("toggle_checkbox"), Id::ToggleCheckbox,
                    QStringLiteral("checkbox"), i18n("Toggle Checkbox"));

    // Heading: H1..H6 as a checkable radio group + Increase/Decrease
    auto *headingGroup = new QActionGroup(this);
    headingGroup->setExclusive(true);
    for (int level = 1; level <= 6; ++level) {
        auto *act = ac->addAction(QStringLiteral("heading_%1").arg(level));
        act->setText(i18n("Heading %1", level));
        act->setIcon(QIcon::fromTheme(QStringLiteral("format-text-heading")));
        act->setCheckable(true);
        act->setActionGroup(headingGroup);
        ac->setDefaultShortcut(
            act, QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_0 + level)));
        connect(act, &QAction::triggered, this,
                [this, level]() { onSetHeading(level); });
    }
    addEditorAction(QStringLiteral("heading_increase"), Id::IncreaseHeading,
                    QStringLiteral("format-header-more"), i18n("Increase Heading Level"));
    addEditorAction(QStringLiteral("heading_decrease"), Id::DecreaseHeading,
                    QStringLiteral("format-header-less"), i18n("Decrease Heading Level"));

    // Insert: Table... / Callout... (dialog-wrapped, not Markoff-action-forwarded)
    auto *insertTable = ac->addAction(QStringLiteral("insert_table"));
    insertTable->setText(i18n("Insert Table..."));
    insertTable->setIcon(QIcon::fromTheme(QStringLiteral("insert-table")));
    connect(insertTable, &QAction::triggered, this, &MainWindow::onInsertTable);

    auto *insertCallout = ac->addAction(QStringLiteral("insert_callout"));
    insertCallout->setText(i18n("Insert Callout..."));
    insertCallout->setIcon(QIcon::fromTheme(QStringLiteral("dialog-information")));
    connect(insertCallout, &QAction::triggered, this, &MainWindow::onInsertCallout);

    // Table operations (enable-gated on Editor::cursorInTable). Markoff's
    // ActionId enum has no per-row/column entries — forward to the
    // member-function API directly.
    auto addTableAction = [this, ac](const QString &objName, const QString &icon,
                                     const QString &label,
                                     void (Markoff::Editor::*fn)()) {
        auto *act = ac->addAction(objName);
        act->setText(label);
        if (!icon.isEmpty())
            act->setIcon(QIcon::fromTheme(icon));
        connect(act, &QAction::triggered, this, [this, fn]() {
            auto *editor = activeEditor();
            if (editor && editor->editor()) (editor->editor()->*fn)();
        });
    };
    addTableAction(QStringLiteral("table_row_above"),   QStringLiteral("edit-table-insert-row-above"),
                   i18n("Insert Row Above"),    &Markoff::Editor::tableInsertRowAbove);
    addTableAction(QStringLiteral("table_row_below"),   QStringLiteral("edit-table-insert-row-below"),
                   i18n("Insert Row Below"),    &Markoff::Editor::tableInsertRowBelow);
    addTableAction(QStringLiteral("table_col_left"),    QStringLiteral("edit-table-insert-column-left"),
                   i18n("Insert Column Left"),  &Markoff::Editor::tableInsertColumnLeft);
    addTableAction(QStringLiteral("table_col_right"),   QStringLiteral("edit-table-insert-column-right"),
                   i18n("Insert Column Right"), &Markoff::Editor::tableInsertColumnRight);
    addTableAction(QStringLiteral("table_delete_row"),  QStringLiteral("edit-table-delete-row"),
                   i18n("Delete Row"),          &Markoff::Editor::tableDeleteRow);
    addTableAction(QStringLiteral("table_delete_col"),  QStringLiteral("edit-table-delete-column"),
                   i18n("Delete Column"),       &Markoff::Editor::tableDeleteColumn);

    // View > Fold All / Unfold All / Toggle Fold at Cursor
    addEditorAction(QStringLiteral("fold_all"), Id::FoldAll,
                    QStringLiteral("collapse-all"), i18n("Fold All"),
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Minus));
    addEditorAction(QStringLiteral("unfold_all"), Id::UnfoldAll,
                    QStringLiteral("expand-all"), i18n("Unfold All"),
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Equal));
    addEditorAction(QStringLiteral("toggle_fold"), Id::ToggleFoldAtCursor,
                    QStringLiteral("code-function"), i18n("Toggle Fold at Cursor"),
                    QKeySequence(Qt::CTRL | Qt::Key_Period));

    // Initial enable-state: no active MarkdownView yet, so disable the
    // whole editor-action set. Workspace::activeLeafChanged will refresh
    // as soon as a leaf becomes active.
    refreshEditorActions();
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

    // Obsidian parity: the workspace must never be left without at least one
    // leaf while a vault is open. When the user closes the last tab, spawn a
    // fresh empty-view leaf (audit: views.md §1.tD) so they land on the "No
    // file is open" placeholder instead of an empty toolbar.
    connect(m_workspace, &Workspace::layoutChanged, this, [this]() {
        if (!m_app || !m_app->isOpen()) return;
        if (!m_workspace->allLeaves().isEmpty()) return;
        auto *tabs = m_workspace->activeTabs();
        if (!tabs) return;
        auto *leaf = m_workspace->createLeafInTabs(tabs);
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

        // Cluster V Phase 2+3 — refresh enable-state + heading radio
        // check-state on every active-leaf change, and hook cursor-moved
        // on the new Markoff editor so the Table submenu's cursorInTable
        // gate updates live while the user types. Also sync the View >
        // Editor Mode radio submenu to the current ViewMode and keep it
        // in sync via the viewModeChanged signal.
        refreshEditorActions();
        if (editor && editor->editor()) {
            disconnect(editor->editor(), &Markoff::Editor::cursorPositionChanged,
                       this, nullptr);
            connect(editor->editor(), &Markoff::Editor::cursorPositionChanged,
                    this, [this](int, int) { refreshEditorActions(); });
        }
        if (editor) {
            // Phase C6 — wire Markoff EditorContext signals for live
            // Format/Heading/Table state and context-menu contribution.
            connectEditorContext(editor);
            connectEditorContextMenu(editor);
            // C2 — wire ThemeService so this editor follows theme changes.
            if (m_themeService)
                editor->setThemeService(m_themeService);
        }
        if (editor) {
            disconnect(editor, &NoteEditorWidget::viewModeChanged, this, nullptr);
            const auto syncMode = [this](NoteEditorWidget::ViewMode m) {
                KActionCollection *ac = actionCollection();
                QString id;
                switch (m) {
                case NoteEditorWidget::ViewMode::Source:      id = QStringLiteral("view_source_mode");  break;
                case NoteEditorWidget::ViewMode::LivePreview: id = QStringLiteral("view_editing_mode"); break;
                case NoteEditorWidget::ViewMode::Reading:     id = QStringLiteral("view_reading_mode"); break;
                }
                if (auto *act = ac->action(id)) act->setChecked(true);
            };
            syncMode(editor->viewMode());
            connect(editor, &NoteEditorWidget::viewModeChanged,
                    this, syncMode);
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

void MainWindow::onNoteActivated(const QString &relativePath)
{
    if (relativePath.endsWith(QStringLiteral(".canvas"))) {
        openFileInWorkspace(relativePath);
        return;
    }

    if (m_vaultObj && m_vaultObj->getAbstractFileByPath(relativePath)) {
        openFileInWorkspace(relativePath);
    } else if (m_fileManager) {
        QString name = relativePath;
        if (name.endsWith(QStringLiteral(".md"))) name.chop(3);
        QString folder;
        int lastSlash = name.lastIndexOf(QLatin1Char('/'));
        if (lastSlash >= 0) {
            folder = name.left(lastSlash);
            name = name.mid(lastSlash + 1);
        }
        auto *tf = m_fileManager->createMarkdownNote(name, folder);
        if (tf)
            openFileInWorkspace(tf->path);
    }
}

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

    // LinkResolver
    delete m_linkResolver;
    m_linkResolver = new LinkResolver();
    {
        QStringList allPaths;
        const auto files = m_vaultObj->getMarkdownFiles();
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
    m_linkResolverAdapter =
        std::make_unique<Corbomite::MarkoffAdapters::LinkResolverAdapter>(
            m_linkResolver);
    m_metadataCacheAdapter =
        std::make_unique<Corbomite::MarkoffAdapters::MetadataCacheAdapter>(
            m_metadataCache);
    m_metadataParserImpl =
        std::make_unique<Corbomite::MarkoffAdapters::MetadataParserImpl>(
            m_linkResolver);
    if (m_embedRenderer) {
        m_embedRenderer->setMetadataCache(m_metadataCacheAdapter.get());
        m_embedRenderer->setResources(m_popoverResources.get());
        m_embedRenderer->setMetadataParser(m_metadataParserImpl.get());
    }

    // Wire suggesters + hover popover against the live vault.
    m_hoverPopover->setVault(m_vaultObj);
    if (m_wikiSuggest) m_wikiSuggest->setVault(m_vaultObj);

    QDir().mkpath(configPath);

    delete m_searchIndex;
    m_searchIndex = new SQLiteIndex(this);
    m_searchIndex->open(configPath + QStringLiteral("/index.sqlite"));
    m_searchIndex->setVaultRoot(path);
    m_searchIndex->setMetadataCache(m_metadataCache);
    if (m_tagSuggest) m_tagSuggest->setIndex(m_searchIndex);

    m_metadataCache->open(configPath + QStringLiteral("/metadata-cache.db"));

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
        if (!tf || tf->extension != QLatin1String("md")) return;
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
        if (!f || !m_metadataCache) return;
        m_metadataCache->onFileDeleted(f->path);
    });
    connect(m_vaultObj, &Vault::renamed, this,
            [this](TAbstractFile *f, const QString &oldPath) {
        if (!f || !m_metadataCache) return;
        m_metadataCache->onFileDeleted(oldPath);
        auto *tf = dynamic_cast<TFile *>(f);
        if (!tf || tf->extension != QLatin1String("md")) return;
        const QByteArray bytes = m_vaultObj->read(tf);
        const qint64 mtimeMs = tf->stat ? tf->stat->mtimeMs : 0;
        m_metadataCache->onFileChanged(tf->path, bytes, mtimeMs);
    });

    // Session manager — restore workspace
    delete m_sessionManager;
    m_sessionManager = new SessionManager(this);
    m_sessionManager->setSessionPath(path + QStringLiteral("/.obsidian/workspace.json"));
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
    }

    m_sessionManager->blockSaving();

    // Plugin lifecycle moved ahead of workspace.deserialize so view types
    // registered by plugins (notably "graph" by corbomite-graph-view) are
    // available when a restored leaf instantiates. Hosted-view attachment
    // still rides the pluginLoaded signal that enablePlugin emits below.
    rewirePluginCoreServices();
    if (auto *pm = m_app->pluginManager()) pm->loadEnabledStateFromConfig();

    if (m_sessionManager->hasLoadedSession()) {
        QJsonObject wsLayout = m_sessionManager->workspaceLayout();
        if (!wsLayout.isEmpty()) {
            // Wrap in the full workspace JSON expected by Workspace::deserialize
            QJsonObject fullWs;
            fullWs[QStringLiteral("main")] = wsLayout;
            fullWs[QStringLiteral("active")] = m_sessionManager->activeLeafId();
            QJsonArray lof;
            for (const auto &f : m_sessionManager->lastOpenFiles())
                lof.append(f);
            if (!lof.isEmpty())
                fullWs[QStringLiteral("lastOpenFiles")] = lof;
            m_workspace->deserialize(fullWs);
        }
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

    updateVaultActions();
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
    }

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

    if (m_wikiSuggest) m_wikiSuggest->setVault(nullptr);
    if (m_tagSuggest) m_tagSuggest->setIndex(nullptr);
    if (m_hoverPopover) m_hoverPopover->setVault(nullptr);


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

    m_centralStack->setCurrentIndex(0);
    m_welcomeScreen->refreshRecentVaults();
    setSidebarsVisibleInternal(false, true);

    updateWindowTitle();
    updateVaultActions();
}

void MainWindow::openGraphView()
{
    if (!m_app->isOpen() || !m_searchIndex || !m_workspace) return;

    auto *tabs = m_workspace->activeTabs();
    if (!tabs) return;
    auto *leaf = m_workspace->createLeafInTabs(tabs);

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
    bool open = m_app->isOpen();

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

void MainWindow::onSettingsApplied()
{
    applyTheme();
    // Future appliers (V.2 autosave-delay etc.) hook here.
}

} // namespace Corbomite
