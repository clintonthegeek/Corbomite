// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_SCENECOORDINATOR_H
#define MARKOFF_SCENECOORDINATOR_H

#include <markoff/Theme.h>
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
class ResourceProvider;

/// Manages the ordered list of scene items, their vertical positioning,
/// splitting/merging on reparse, and serialization back to markdown.
class SceneCoordinator : public QObject {
    Q_OBJECT
public:
    explicit SceneCoordinator(SelectionScene *scene, QObject *parent = nullptr);
    ~SceneCoordinator() override;

    /// Load markdown: split at block boundaries, apply live-preview formatting.
    void loadMarkdown(const QString &markdown);

    /// Serialize all items back to flat markdown.
    QString toMarkdown() const;

    /// Set item width (for viewport resize).
    void setItemWidth(qreal width);

    /// Set font for all text items.
    void setFont(const QFont &font);

    /// Set theme for all text item highlighters.
    void setTheme(const Theme &theme);

    /// Set a non-owning resource provider used to resolve relative
    /// resource paths (images, embeds, links) in the editor pipeline.
    /// Used by ImageBlockItem for image resolution.
    void setResourceProvider(ResourceProvider *provider);
    ResourceProvider *resourceProvider() const { return m_resourceProvider; }

    /// Get ordered items (for external use).
    const QList<SelectableItem *> &items() const { return m_items; }

    /// Transfer focus to an adjacent item. Returns true if successful.
    bool moveFocusTo(MarkdownTextItem *from, Qt::Edge edge);

Q_SIGNALS:
    void textChanged();
    void reparsed();

private:
    MarkdownTextItem *createTextItem(const QString &text);
    void handleBoundary(MarkdownTextItem *from, Qt::Edge edge);
    void clearItems();
    void repositionItems();
    void onItemTextChanged();
    void reparse();

    SelectionScene *m_scene = nullptr;
    QList<SelectableItem *> m_items;
    TreeSitterParser *m_parser = nullptr;
    QTimer *m_reparseTimer = nullptr;
    ResourceProvider *m_resourceProvider = nullptr;
    qreal m_itemWidth = 600.0;
    qreal m_spacing = 8.0;
    qreal m_leftMargin = 16.0;
    qreal m_topMargin = 12.0;
    QFont m_font;
    bool m_inReparse = false;
    int m_keyboardCurrentIdx = -1;
    int m_keyboardAnchorIdx = -1;
    int m_keyboardAnchorPos = -1;
};

} // namespace Markoff

#endif // MARKOFF_SCENECOORDINATOR_H
