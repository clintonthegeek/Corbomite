// src/canvas/CanvasView.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/FileView.h"

namespace Corbomite {

class CanvasViewTab;
class MarkdownRenderEngine;

class CanvasView : public FileView
{
    Q_OBJECT
public:
    explicit CanvasView(WorkspaceLeaf *leaf, QWidget *parent = nullptr);
    static View *factory(WorkspaceLeaf *leaf);

    QString getViewType() const override;
    QString getIcon() const override;
    bool canAcceptExtension(const QString &ext) const override;

    void setRenderEngine(MarkdownRenderEngine *engine);
    CanvasViewTab *canvasWidget() const;

protected:
    void onLoadFile(NoteDocument *file) override;
    void onUnloadFile(NoteDocument *file) override;

private:
    CanvasViewTab *m_canvasWidget = nullptr;
    MarkdownRenderEngine *m_renderEngine = nullptr;
};

} // namespace Corbomite
