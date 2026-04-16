// src/canvas/CanvasView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasView.h"
#include "CanvasViewTab.h"
#include "corbomite/core/NoteDocument.h"

#include <QVBoxLayout>

namespace Corbomite {

CanvasView::CanvasView(WorkspaceLeaf *leaf, QWidget *parent)
    : FileView(leaf, parent)
{
}

View *CanvasView::factory(WorkspaceLeaf *leaf)
{
    return new CanvasView(leaf);
}

QString CanvasView::getViewType() const { return QStringLiteral("canvas"); }
QString CanvasView::getIcon() const { return QStringLiteral("palette"); }

bool CanvasView::canAcceptExtension(const QString &ext) const
{
    return ext.compare(QStringLiteral("canvas"), Qt::CaseInsensitive) == 0;
}

void CanvasView::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_renderEngine = engine;
    if (m_canvasWidget)
        m_canvasWidget->setRenderEngine(engine);
}

CanvasViewTab *CanvasView::canvasWidget() const { return m_canvasWidget; }

void CanvasView::onLoadFile(NoteDocument *file)
{
    FileView::onLoadFile(file);
    if (!m_canvasWidget && file) {
        m_canvasWidget = new CanvasViewTab(file->filePath(), contentWidget());
        auto *layout = new QVBoxLayout(contentWidget());
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_canvasWidget);
        if (m_renderEngine)
            m_canvasWidget->setRenderEngine(m_renderEngine);
    }
}

void CanvasView::onUnloadFile(NoteDocument *file)
{
    if (m_canvasWidget && m_canvasWidget->isModified())
        m_canvasWidget->save();
    FileView::onUnloadFile(file);
}

} // namespace Corbomite
