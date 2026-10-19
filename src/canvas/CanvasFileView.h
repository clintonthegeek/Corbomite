// src/canvas/CanvasFileView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/FileView.h"

#include <functional>

namespace Corbomite {

class CanvasViewTab;
namespace Core {
class ThemeService;
}
class MarkdownRenderEngine;
class MenuSectionHelper;

class CanvasFileView : public FileView
{
    Q_OBJECT
public:
    explicit CanvasFileView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);
    ~CanvasFileView() override;
    static View *factory(WorkspaceLeaf *leaf);

    QString getViewType() const override;
    QString getIcon() const override;
    bool canAcceptExtension(const QString &ext) const override;

    /// Cluster M M5.3 — canvas viewport persistence.
    ///
    /// The viewport rides in the ViewState's `state` payload under
    /// `viewState`, exactly where Obsidian's canvas puts it
    /// (`app/own/2028-n9.js:850`: `t.viewState = this.canvas.getState()`) —
    /// **not** in the leaf's `eState`, which Obsidian never serialises and in
    /// which the canvas writes nothing at all.
    ///
    /// Field names and units are Obsidian's, verbatim: `x`/`y` are the scene
    /// point at the viewport centre, `zoom` is log2 of the scale factor.
    /// Renaming any of them would make a workspace.json written by one app
    /// unreadable by the other, so this is a shared-schema surface
    /// (workspace compat doctrine, L0).
    ///
    /// Source: `docs/obsidian-audit/addenda/2026-10-03-canvas-viewport-ephemeral-state-resolution.md`.
    QJsonObject getState() const override;
    void setState(const QJsonObject &state) override;

    void setRenderEngine(MarkdownRenderEngine *engine);
    CanvasViewTab *canvasWidget() const;

    /// M6.3 — forward to the canvas tab so a vault create/rename re-resolves
    /// dangling file cards. Empty path refreshes every card.
    void refreshFileCards(const QString &vaultRelativePath = QString());

    /// Cluster M leftover — canvas dark mode. Follows the host's ThemeService
    /// so an Obsidian-authored dark vault gets a dark canvas instead of a
    /// white one with white cards on it. Same injection shape as
    /// NoteEditorWidget::setThemeService.
    void setThemeService(Corbomite::Core::ThemeService *service);

    /// Cluster M leftover — the vault's file list for the canvas file picker,
    /// as vault-relative paths. Passing the Vault's own list (rather than
    /// letting the picker walk the filesystem) means the picker automatically
    /// honours whatever Vault already excludes, and costs one in-memory list
    /// instead of a recursive directory scan per open.
    void setFileLister(std::function<QStringList()> lister);

    /// Cluster M M5.4 — where the canvas read-only lock is persisted. Points
    /// CanvasViewTab at SessionManager's tier 3. Null store means the lock is
    /// in-memory only (which is the correct behaviour for a canvas opened
    /// outside a vault session). MainWindow currently doesn't inject one, so
    /// onLoadFile() falls back to resolving the window's own SessionManager —
    /// see the comment there.
    using LocalDataReader = std::function<QJsonObject(const QString &key)>;
    using LocalDataWriter = std::function<void(const QString &key, const QJsonObject &value)>;
    void setLocalDataStore(LocalDataReader reader, LocalDataWriter writer);

    /// M5.4 — true when this canvas is locked. Always false when no file is
    /// loaded, which is what makes `canvas_lock` checkable rather than
    /// tri-state.
    bool isReadOnly() const;

    // Cluster O Phase O1.T3 — re-light the View::zoom*() polymorphic
    // dispatch onto the real CanvasView viewport transform. Previously
    // MainWindow's zoom actions bypassed the virtuals entirely and only
    // ever reached the markdown leaf; canvas had no zoom action at all.
    void zoomIn() override;
    void zoomOut() override;
    void zoomReset() override;

    // Cluster O Phase O2.T1 — Tier-B capability overrides. canZoom() is
    // left at the base default (true).
    bool canEdit() const override;
    bool canSave() const override;
    bool hasSelection() const override;
    bool canUndo() const override;
    bool canRedo() const override;

    /// Cluster R Task 3.6 — command-dispatch hook used by the hamburger
    /// menu's Split/pane/linked-view entries. Injected by MainWindow.
    using CommandDispatch = std::function<void(const QString &commandId)>;
    void setCanvasCommandDispatcher(CommandDispatch dispatcher);

    /// Cluster R Task 3.6 — hamburger menu: Split/Export/Bookmark-stub +
    /// view.linked submenu. Does NOT chain to EditableFileView (canvas is
    /// not an editable-text file view in the current hierarchy); rename /
    /// move / delete remain a follow-up for when CanvasFileView is
    /// promoted to EditableFileView.
    void onMoreOptionsMenu(MenuSectionHelper &helper) override;

    /// External edit to this canvas' file. Nothing unsaved here → adopt the
    /// file and say so; edits in flight → keep them and say so (the pending
    /// autosave then replaces the file, which the notice warns about).
    /// Obsidian's own canvas silently replaces the view with the file in
    /// both cases — its canvas view sets `isPlaintext = false`, which skips
    /// both the merge and the "File changed" notice
    /// (`app/own/2028-n9.js:805`, `app/own/1813-CZ.js:83-92`).
    void onExternalModify(const QString &relativePath) override;

protected:
    void onLoadFile(NoteDocument *file) override;
    void onUnloadFile(NoteDocument *file) override;

private Q_SLOTS:
    /// Cluster R Task 3.6 — modal: Area (selected / full canvas), Format
    /// (PNG / SVG), transparent background, show edges. Writes through
    /// CanvasScene::renderToImage / renderToSvg.
    void showExportAsImageModal();

private:
    CanvasViewTab *m_canvasWidget = nullptr;
    /// Vault-relative path the live document is registered under in
    /// Canvas::CanvasDocumentRegistry (M6.2). Empty when not attached.
    QString m_registeredCanvasPath;
    /// Raw pointer + explicit disconnect, matching
    /// NoteEditorWidget::setThemeService — the ThemeService outlives every
    /// view (it is owned by MainWindow), and QPointer would force a
    /// ThemeService.h include here for no benefit.
    Corbomite::Core::ThemeService *m_themeService = nullptr;
    std::function<QStringList()> m_fileLister;
    LocalDataReader m_localDataReader;
    LocalDataWriter m_localDataWriter;
    MarkdownRenderEngine *m_renderEngine = nullptr;
    CommandDispatch m_canvasCommandDispatcher;
    /// The last external revision this view reacted to, so one external write
    /// warns once rather than once per `Vault::modified` event.
    QByteArray m_lastExternalDisk;
};

} // namespace Corbomite
