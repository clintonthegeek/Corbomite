// SPDX-License-Identifier: GPL-3.0-or-later
#include "NotePreviewWidget.h"
#include "corbomite/core/NoteDocument.h"

#include <QDesktopServices>

namespace Corbomite {

NotePreviewWidget::NotePreviewWidget(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setReadOnly(true);

    connect(this, &QTextBrowser::anchorClicked, this, &NotePreviewWidget::onAnchorClicked);
}

void NotePreviewWidget::renderDocument(NoteDocument *doc)
{
    if (!doc) {
        clear();
        return;
    }

    QString html = m_renderer.renderToHtml(doc->markdown());
    setHtml(html);
}

void NotePreviewWidget::onAnchorClicked(const QUrl &url)
{
    QString scheme = url.scheme();
    QString path = url.path();

    if (scheme.isEmpty() || scheme == QStringLiteral("file")) {
        // Internal link — likely a wikilink
        // Remove .md extension for the signal
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
