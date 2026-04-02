// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_SCENECOORDINATOR_H
#define MARKOFF_SCENECOORDINATOR_H

#include "MarkdownHighlighter.h"
#include <QObject>
#include <QList>
#include <QFont>

class QTimer;

namespace Markoff {

class SelectionScene;
class SelectableItem;
class MarkdownTextItem;
class MarkdownHighlighter;
class TreeSitterParser;

/// Manages the ordered list of scene items, their vertical positioning,
/// splitting/merging on reparse, and serialization back to markdown.
class SceneCoordinator : public QObject {
    Q_OBJECT
public:
    explicit SceneCoordinator(SelectionScene *scene, QObject *parent = nullptr);
    ~SceneCoordinator() override;

    /// Load markdown in live preview mode: split at block boundaries.
    void loadMarkdown(const QString &markdown);

    /// Load markdown in source mode: single text item, no splitting.
    void loadSource(const QString &markdown);

    /// Serialize all items back to flat markdown.
    QString toMarkdown() const;

    /// Set item width (for viewport resize).
    void setItemWidth(qreal width);

    /// Set font for all text items.
    void setFont(const QFont &font);

    /// Get ordered items (for external use).
    const QList<SelectableItem *> &items() const { return m_items; }

    /// Transfer focus to an adjacent item. Returns true if successful.
    bool moveFocusTo(MarkdownTextItem *from, Qt::Edge edge);

Q_SIGNALS:
    void textChanged();

private:
    MarkdownTextItem *createTextItem(const QString &text,
                                      MarkdownHighlighter::Mode hlMode);
    void clearItems();
    void repositionItems();
    void onItemTextChanged();
    void reparse();

    SelectionScene *m_scene = nullptr;
    QList<SelectableItem *> m_items;
    TreeSitterParser *m_parser = nullptr;
    QTimer *m_reparseTimer = nullptr;
    qreal m_itemWidth = 600.0;
    qreal m_spacing = 8.0;
    QFont m_font;
    bool m_inReparse = false;
};

} // namespace Markoff

#endif // MARKOFF_SCENECOORDINATOR_H
