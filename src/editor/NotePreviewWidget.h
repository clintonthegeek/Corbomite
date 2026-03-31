// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTextBrowser>
#include "corbomite/core/MarkdownRenderer.h"

namespace Corbomite {

class NoteDocument;

class NotePreviewWidget : public QTextBrowser {
    Q_OBJECT

public:
    explicit NotePreviewWidget(QWidget *parent = nullptr);

    void renderDocument(NoteDocument *doc);

Q_SIGNALS:
    void internalLinkClicked(const QString &targetPath);

private:
    void onAnchorClicked(const QUrl &url);

    MarkdownRenderer m_renderer;
};

} // namespace Corbomite
