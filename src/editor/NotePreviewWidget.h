// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTextBrowser>

namespace Corbomite {

class MarkdownRenderEngine;
class NoteDocument;

class NotePreviewWidget : public QTextBrowser {
    Q_OBJECT

public:
    explicit NotePreviewWidget(QWidget *parent = nullptr);

    void setRenderEngine(MarkdownRenderEngine *engine);
    void renderDocument(NoteDocument *doc);

Q_SIGNALS:
    void internalLinkClicked(const QString &targetPath);

private:
    void onAnchorClicked(const QUrl &url);

    MarkdownRenderEngine *m_engine = nullptr;
};

} // namespace Corbomite
