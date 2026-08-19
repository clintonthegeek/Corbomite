// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasViewTab.h"

#include <canvas/CanvasDocument.h>
#include <canvas/CanvasScene.h>
#include <canvas/CanvasView.h>
#include <corbomite/core/MarkdownRenderEngine.h>

#include <QFile>
#include <QFileInfo>
#include <QVBoxLayout>

namespace Corbomite {

CanvasViewTab::CanvasViewTab(const QString &filePath, const QString &vaultRoot, QWidget *parent)
    : QWidget(parent)
    , m_filePath(filePath)
{
    m_document = new Canvas::CanvasDocument(this);
    m_view = new Canvas::CanvasView(this);

    m_document->loadFromFile(filePath);
    m_view->setDocument(m_document);

    // File-card paths in a .canvas file are vault-relative (Obsidian spec),
    // not relative to the canvas file's own directory. Fall back to the
    // canvas file's directory when no vault root is supplied.
    QString resolveBase = vaultRoot.isEmpty() ? QFileInfo(filePath).absolutePath() : vaultRoot;
    m_view->canvasScene()->setFileResolver([resolveBase](const QString &path) -> QString {
        QString fullPath = resolveBase + QLatin1Char('/') + path;
        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return QString::fromUtf8(file.readAll());
    });

    m_view->canvasScene()->setFileSaver([resolveBase](const QString &path, const QString &content) {
        QString fullPath = resolveBase + QLatin1Char('/') + path;
        QFile file(fullPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(content.toUtf8());
        }
    });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    connect(m_document, &Canvas::CanvasDocument::modificationChanged,
            this, &CanvasViewTab::modificationChanged);
}

CanvasViewTab::~CanvasViewTab() = default;

void CanvasViewTab::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_view->canvasScene()->setRenderEngine(engine);
}

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

Canvas::CanvasScene *CanvasViewTab::canvasScene() const
{
    return m_view ? m_view->canvasScene() : nullptr;
}

} // namespace Corbomite
