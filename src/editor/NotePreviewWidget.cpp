// SPDX-License-Identifier: GPL-3.0-or-later
#include "NotePreviewWidget.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/MarkdownRenderEngine.h"

#include <QDesktopServices>
#include <QTextDocument>

namespace Corbomite {

NotePreviewWidget::NotePreviewWidget(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setReadOnly(true);

    connect(this, &QTextBrowser::anchorClicked, this, &NotePreviewWidget::onAnchorClicked);
}

void NotePreviewWidget::setRenderEngine(MarkdownRenderEngine *engine)
{
    m_engine = engine;
}

void NotePreviewWidget::renderDocument(NoteDocument *doc)
{
    if (!doc || !m_engine) {
        clear();
        return;
    }

    auto rendered = m_engine->render(doc->markdown());
    setHtml(rendered->toQTextDocument()->toHtml());
}

void NotePreviewWidget::onAnchorClicked(const QUrl &url)
{
    QString scheme = url.scheme();
    QString path = url.path();

    if (scheme.isEmpty() || scheme == QStringLiteral("file")) {
        // Internal link — likely a wikilink
        if (path.endsWith(QStringLiteral(".md"))) {
            Q_EMIT internalLinkClicked(path);
        } else {
            Q_EMIT internalLinkClicked(path + QStringLiteral(".md"));
        }
    } else {
        // External link — open in browser
        QDesktopServices::openUrl(url);
    }
}

} // namespace Corbomite
