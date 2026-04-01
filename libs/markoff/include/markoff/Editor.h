// SPDX-License-Identifier: GPL-3.0-or-later
// Forked from Qt's QPlainTextEdit / QWidgetTextControl
// Original: Copyright (C) The Qt Company Ltd. (GPL-2.0-only OR GPL-3.0-only)

#ifndef MARKOFF_EDITOR_H
#define MARKOFF_EDITOR_H

#include <QAbstractScrollArea>
#include <memory>

class QTextDocument;

namespace Markoff {

class Editor : public QAbstractScrollArea {
    Q_OBJECT
public:
    enum class Mode { Source, LivePreview };

    explicit Editor(QWidget *parent = nullptr);
    ~Editor() override;

    void setPlainText(const QString &text);
    QString toPlainText() const;

    QTextDocument *document() const;

    void setMode(Mode mode);
    Mode mode() const;

    void setFontSize(int pointSize);

    void ensureCursorVisible();
    QRect cursorRect() const;

    // Forward-declared here, defined in Editor_p.h
    struct Private;

Q_SIGNALS:
    void textChanged();

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
    void inputMethodEvent(QInputMethodEvent *e) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void scrollContentsBy(int dx, int dy) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    bool event(QEvent *e) override;

private:
    std::unique_ptr<Private> d;
};

} // namespace Markoff

#endif // MARKOFF_EDITOR_H
