// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasViewTab.h"

#include <canvas/CanvasDocument.h>
#include <canvas/CanvasView.h>

#include <QVBoxLayout>

namespace Corbomite {

CanvasViewTab::CanvasViewTab(const QString &filePath, QWidget *parent)
    : QWidget(parent)
    , m_filePath(filePath)
{
    m_document = new Canvas::CanvasDocument(this);
    m_view = new Canvas::CanvasView(this);

    m_document->loadFromFile(filePath);
    m_view->setDocument(m_document);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    connect(m_document, &Canvas::CanvasDocument::modificationChanged,
            this, &CanvasViewTab::modificationChanged);
}

CanvasViewTab::~CanvasViewTab() = default;

QString CanvasViewTab::filePath() const
{
    return m_filePath;
}

bool CanvasViewTab::save()
{
    return m_document->saveToFile(m_filePath);
}

bool CanvasViewTab::isModified() const
{
    return m_document->isModified();
}

} // namespace Corbomite
