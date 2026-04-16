// src/canvas/CanvasFileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasFileView.h"
#include "CanvasViewTab.h"
#include "corbomite/core/NoteDocument.h"

#include <QVBoxLayout>

namespace Corbomite {

CanvasFileView::CanvasFileView(WorkspaceLeaf *leaf, QWidget *parent)
    : FileView(leaf, parent)
{
}

View *CanvasFileView::factory(WorkspaceLeaf *leaf)
{
    return new CanvasFileView(leaf);
}

QString CanvasFileView::getViewType() const { return QStringLiteral("canvas"); }
QString CanvasFileView::getIcon() const { return QStringLiteral("palette"); }

bool CanvasFileView::canAcceptExtension(const QString &ext) const
{
    return ext.compare(QStringLiteral("canvas"), Qt::CaseInsensitive) == 0;
}

void CanvasFileView::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_renderEngine = engine;
    if (m_canvasWidget)
        m_canvasWidget->setRenderEngine(engine);
}

CanvasViewTab *CanvasFileView::canvasWidget() const { return m_canvasWidget; }

void CanvasFileView::onLoadFile(NoteDocument *file)
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

void CanvasFileView::onUnloadFile(NoteDocument *file)
{
    if (m_canvasWidget && m_canvasWidget->isModified())
        m_canvasWidget->save();
    FileView::onUnloadFile(file);
}

} // namespace Corbomite
