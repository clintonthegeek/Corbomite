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
class FoldingModel;

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

    /// Subscribe to fold-state changes and apply item visibility.
    void setFoldingModel(FoldingModel *model);

    /// Return the index of the item whose scene bounding rect contains sceneY.
    /// Returns -1 if none.
    int itemIndexAt(qreal sceneY) const;

    /// Return the heading path enclosing itemIndex (i.e. the most-recent
    /// heading at or before itemIndex). Returns empty list for items before
    /// the first heading. PUBLIC — used by Editor (Task 8) for auto-unfold.
    QStringList enclosingHeadingPath(int itemIndex) const;

    /// Return the heading index in FoldingModel::headings() if itemIndex
    /// itself is a heading item, otherwise -1. Used by Task 10 gutter click
    /// dispatch.
    int headingIndexForItem(int itemIndex) const;

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

    FoldingModel *m_foldingModel = nullptr;
    void applyFoldVisibility();

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
