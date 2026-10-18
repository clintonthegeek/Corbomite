// src/editor/MarkdownView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/TextFileView.h"

#include <functional>

namespace Corbomite {

/// Cluster-audit P1 — locate the CLOSING frontmatter fence in `body`, or -1.
///
/// A closing fence is exactly three dashes on a line of their own. Exposed as
/// a free function (rather than left inline in
/// insertFrontmatterProperty) so it can be tested directly: routing it through
/// a live NoteDocument round-trip would test Markoff's frontmatter parser
/// instead, which independently mangles a `----` line, so an end-to-end
/// assertion fails for a reason unrelated to this scan.
///
/// A fence is a `\n` followed by exactly `---` followed by end-of-body, LF or
/// CR. A LONGER run of dashes (`----`) and a line merely prefixed with dashes
/// (`---not a fence`) are deliberately NOT fences: treating either as one
/// splices a bogus `: ` row into the middle of someone's prose.
int findClosingFrontmatterFence(const QString &body);

class MenuSectionHelper;
class NoteEditorWidget;
class HoverPopover;
class EditorSuggestManager;
class Vault;

class MarkdownView : public TextFileView
{
    Q_OBJECT

public:
    explicit MarkdownView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);

    static View *factory(WorkspaceLeaf *leaf);

    QString getViewType() const override;
    QString getDisplayText() const override;
    QString getIcon() const override;

    QString getViewData() const override;
    void setViewData(const QString &data, bool clear) override;
    void clear() override;

    bool canAcceptExtension(const QString &ext) const override;

    bool setCursorLine(int line) override;

    // Cluster V Phase 1 — route app-shell zoom actions to the currently active
    // editor sub-widget (LivePreview → Markoff::Editor, Source → SourceEditor,
    // Reading → ReadingView). NoteEditorWidget dispatches by viewMode().
    void zoomIn() override;
    void zoomOut() override;
    void zoomReset() override;

    // Cluster O Phase O2.T4 — Tier-B capability overrides. canZoom() is
    // left at the base default (true — markdown really does zoom).
    bool canEdit() const override;
    bool canSave() const override;
    bool canFind() const override;
    bool canUndo() const override;
    bool canRedo() const override;

    QJsonObject getState() const override;
    void setState(const QJsonObject &state) override;
    QJsonObject getEphemeralState() const override;
    void setEphemeralState(const QJsonObject &state) override;

    NoteEditorWidget *editorWidget() const;

    /// Ensure a YAML frontmatter block exists, then append a blank property
    /// row. If the view is currently in Reading mode, switch to LivePreview
    /// so the new row is visible. Cluster R Task 3.2 — backs the
    /// `markdown:add-metadata-property` command.
    void insertFrontmatterProperty();

    void setVault(Vault *vault);
    void setHoverPopover(HoverPopover *popover);
    void setEditorSuggestManager(EditorSuggestManager *manager);

    /// Cluster R Task 3.4: the view needs to dispatch commands
    /// (`split_right`, `markdown:add-metadata-property`, `backlinks:open`,
    /// etc.) back to the host CommandRegistry. Host injects a stateless
    /// dispatcher at setup time.
    using CommandDispatch = std::function<void(const QString &commandId)>;
    void setMarkdownCommandDispatcher(CommandDispatch dispatcher);

    /// Supplies the TFile* + Vault* pair for the Export-to-PDF menu action.
    /// Host wires during setup. Nullable — when unset, Export-to-PDF is a
    /// no-op.
    using PdfExportTrigger = std::function<void(QWidget *parent)>;
    void setPdfExportTrigger(PdfExportTrigger trigger);

    using FindTrigger = std::function<void(QWidget *parent)>;
    void setFindTrigger(FindTrigger trigger);
    void setReplaceTrigger(FindTrigger trigger);

    /// Cluster F-a P3.3/P3.4 — "Merge entire file with…". The action lives in
    /// the markdown hamburger's `action` section and is owned by this view
    /// (the provider mechanism), not a MainWindow leaf-type branch. Nullable;
    /// when unset the item is disabled.
    using MergeTrigger = std::function<void(QWidget *parent)>;
    void setMergeFileTrigger(MergeTrigger trigger);

    /// Cluster R Task 3.4 — hamburger-menu contribution: Split/Reading/Source/
    /// AddProperty/ExportPDF/... plus the view.linked submenu. Chains up to
    /// EditableFileView::onMoreOptionsMenu for universal file-menu items.
    void onMoreOptionsMenu(MenuSectionHelper &helper) override;

protected:
    void onOpen() override;
    void onClose() override;
    void onLoadFile(NoteDocument *file) override;

private:
    NoteEditorWidget *m_editorWidget;
    CommandDispatch m_markdownCommandDispatcher;
    PdfExportTrigger m_pdfExportTrigger;
    FindTrigger m_findTrigger;
    FindTrigger m_replaceTrigger;
    MergeTrigger m_mergeFileTrigger;
};

} // namespace Corbomite
