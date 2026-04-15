// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QGraphicsView>
#include <QString>

class QGraphicsScene;

namespace Corbomite::ReadingView {

/// Obsidian-compatible Reading-mode widget. Cluster E Phase 3 fills in the
/// real pipeline (tree-sitter AST → section split → section recycling +
/// virtualization + async parse + frame budget). Cluster E Phase 2 creates
/// this shell so `NoteEditorWidget::saveEphemeralState` can route the Reading
/// mode's visual-line float scroll through a stable API surface.
class ReadingView : public QGraphicsView {
    Q_OBJECT

public:
    explicit ReadingView(QWidget *parent = nullptr);
    ~ReadingView() override;

    /// Set the markdown source. Phase 2 stub — no-op. Phase 3 parses this
    /// through `libs/markoff-parser` and populates the section pool.
    void setPlainText(const QString &text);

    /// Visual-line float scroll. Stubbed at `0.0f` in Phase 2; Phase 3
    /// computes it from mounted-section geometry plus the pre-layout line
    /// offsets of unmounted sections above.
    float scrollPositionVisualLine() const;
    void setScrollPositionVisualLine(float visualLine);

Q_SIGNALS:
    /// Fired when the scroll position changes. Phase 2 stub never emits this;
    /// Phase 3 wires it through the virtual-scroll controller.
    void scrollPositionVisualLineChanged(float visualLine);
};

} // namespace Corbomite::ReadingView
