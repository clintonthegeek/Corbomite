// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Canvas {
class CanvasDocument;
class CanvasScene;
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

    /// Cluster R Task 3.6 — access to the underlying CanvasScene so
    /// CanvasFileView can drive its render-to-image / render-to-svg
    /// export pipeline.
    Canvas::CanvasScene *canvasScene() const;

Q_SIGNALS:
    void modificationChanged(bool modified);

private:
    Canvas::CanvasDocument *m_document;
    Canvas::CanvasView *m_view;
    QString m_filePath;
};

} // namespace Corbomite
