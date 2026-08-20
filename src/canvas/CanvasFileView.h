// src/canvas/CanvasFileView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/FileView.h"

#include <functional>

namespace Corbomite {

class CanvasViewTab;
class MarkdownRenderEngine;
class MenuSectionHelper;

class CanvasFileView : public FileView
{
    Q_OBJECT
public:
    explicit CanvasFileView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);
    static View *factory(WorkspaceLeaf *leaf);

    QString getViewType() const override;
    QString getIcon() const override;
    bool canAcceptExtension(const QString &ext) const override;

    void setRenderEngine(MarkdownRenderEngine *engine);
    CanvasViewTab *canvasWidget() const;

    // Cluster O Phase O1.T3 — re-light the View::zoom*() polymorphic
    // dispatch onto the real CanvasView viewport transform. Previously
    // MainWindow's zoom actions bypassed the virtuals entirely and only
    // ever reached the markdown leaf; canvas had no zoom action at all.
    void zoomIn() override;
    void zoomOut() override;
    void zoomReset() override;

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
    MarkdownRenderEngine *m_renderEngine = nullptr;
    CommandDispatch m_canvasCommandDispatcher;
};

} // namespace Corbomite
