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
    /// @param filePath Absolute path of the .canvas file being opened.
    /// @param vaultRoot Absolute path of the vault root. File-card paths
    ///        embedded in the canvas are resolved/saved relative to this
    ///        root (per Obsidian's `.canvas` spec), not the canvas file's
    ///        own directory. Falls back to the canvas file's directory
    ///        when left empty (e.g. ad-hoc/out-of-vault usage).
    explicit CanvasViewTab(const QString &filePath, const QString &vaultRoot = QString(),
                           QWidget *parent = nullptr);
    ~CanvasViewTab() override;

    void setRenderEngine(MarkdownRenderEngine *engine);

    QString filePath() const;
    bool save();
    bool isModified() const;

    /// Cluster R Task 3.6 — access to the underlying CanvasScene so
    /// CanvasFileView can drive its render-to-image / render-to-svg
    /// export pipeline.
    Canvas::CanvasScene *canvasScene() const;

    /// Cluster O Phase O1.T3 — access to the underlying CanvasView so
    /// CanvasFileView can dispatch the polymorphic View::zoomIn/Out/Reset
    /// virtuals onto the real viewport transform. (O4.T1 will want this
    /// same accessor for the canvas toolbar; landing it here is the
    /// minimal plumbing O1.T3 needs, not scope creep into O4.)
    Canvas::CanvasView *canvasView() const;

Q_SIGNALS:
    void modificationChanged(bool modified);

private:
    Canvas::CanvasDocument *m_document;
    Canvas::CanvasView *m_view;
    QString m_filePath;
};

} // namespace Corbomite
