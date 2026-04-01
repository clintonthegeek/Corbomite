// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Canvas {
class CanvasDocument;
class CanvasView;
}

namespace Corbomite {

class MarkdownRenderEngine;

class CanvasViewTab : public QWidget {
    Q_OBJECT

public:
    explicit CanvasViewTab(const QString &filePath, QWidget *parent = nullptr);
    ~CanvasViewTab() override;

    void setRenderEngine(MarkdownRenderEngine *engine);

    QString filePath() const;
    bool save();
    bool isModified() const;

Q_SIGNALS:
    void modificationChanged(bool modified);

private:
    Canvas::CanvasDocument *m_document;
    Canvas::CanvasView *m_view;
    QString m_filePath;
};

} // namespace Corbomite
