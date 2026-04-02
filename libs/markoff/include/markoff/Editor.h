// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_EDITOR_H
#define MARKOFF_EDITOR_H

#include <QGraphicsView>

namespace Markoff {

class SelectionScene;
class SceneCoordinator;

/// QGraphicsView-based markdown editor.
/// Splits markdown at block boundaries (tables, code blocks) into
/// independent editable text items and non-text block items.
/// Supports cross-boundary selection with markdown clipboard.
class Editor : public QGraphicsView {
    Q_OBJECT
public:
    enum class Mode { Source, LivePreview };

    explicit Editor(QWidget *parent = nullptr);
    ~Editor() override;

    void setPlainText(const QString &text);
    QString toPlainText() const;

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setFontSize(int pointSize);

Q_SIGNALS:
    void textChanged();

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    void rebuildScene();

    SelectionScene *m_scene = nullptr;
    SceneCoordinator *m_coordinator = nullptr;
    Mode m_mode = Mode::Source;
    QString m_sourceText;
    int m_fontSize = 14;
};

} // namespace Markoff

#endif // MARKOFF_EDITOR_H
