// SPDX-License-Identifier: GPL-3.0-or-later
#include "CanvasViewTab.h"

#include <canvas/CanvasDocument.h>
#include <canvas/CanvasFilePickerDialog.h>
#include <canvas/CanvasScene.h>
#include <canvas/CanvasView.h>
#include <corbomite/core/MarkdownRenderEngine.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
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

    // M2.2 — "New file card…" candidate source. No Vault object is
    // reachable at this layer (CanvasViewTab only ever gets a vaultRoot
    // path string), so this scans the vault tree directly for markdown +
    // common attachment files rather than reusing Vault::getFiles(); a
    // real Vault-backed source is app-side follow-up work (see plan M2.2
    // note). CanvasFilePickerDialog does the actual fuzzy filter/rank.
    QPointer<Canvas::CanvasView> viewPtr(m_view);
    m_view->canvasScene()->setFilePickerRequestor([resolveBase, viewPtr]() -> QString {
        static const QStringList kAttachmentExts = {
            QStringLiteral("md"), QStringLiteral("png"), QStringLiteral("jpg"),
            QStringLiteral("jpeg"), QStringLiteral("gif"), QStringLiteral("svg"),
            QStringLiteral("pdf"), QStringLiteral("canvas"),
        };

        QStringList candidates;
        if (!resolveBase.isEmpty()) {
            const QDir root(resolveBase);
            QDirIterator it(resolveBase, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString abs = it.next();
                const QFileInfo fi(abs);
                if (!kAttachmentExts.contains(fi.suffix().toLower()))
                    continue;
                candidates << root.relativeFilePath(abs);
            }
            candidates.sort(Qt::CaseInsensitive);
        }

        if (!viewPtr)
            return {};
        return Canvas::CanvasFilePickerDialog::pickFile(viewPtr, candidates);
    });

    // M2.3 — drag-drop candidate paths arrive as absolute filesystem paths
    // (text/uri-list); resolve to vault-relative, rejecting anything
    // outside resolveBase (no copy-into-vault in M1/M2 scope).
    m_view->canvasScene()->setVaultPathResolver([resolveBase](const QString &absoluteFilePath) -> QString {
        if (resolveBase.isEmpty())
            return {};
        const QDir root(resolveBase);
        const QString rel = root.relativeFilePath(absoluteFilePath);
        if (rel.startsWith(QStringLiteral("..")) || QDir::isAbsolutePath(rel))
            return {}; // outside the vault
        return rel;
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
