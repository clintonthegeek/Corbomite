// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_EDITOR_H
#define MARKOFF_EDITOR_H

#include <QGraphicsView>
#include <QJsonObject>
#include <QTextDocument>
#include <markoff/Theme.h>
#include <markoff/EditorSettings.h>
#include <markoff-parser/Document.h>

class QTimer;

namespace Markoff {

class SelectionScene;
class SceneCoordinator;
class MarkdownTextItem;
class ResourceProvider;
class SearchBar;
class FoldingModel;
class FoldGutter;

class Editor : public QGraphicsView {
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    ~Editor() override;

    // --- Content ---
    void setPlainText(const QString &text);
    void clear();
    QString toPlainText() const;
    const Document *document() const;

    // --- Configuration ---
    void setTheme(const Theme &theme);
    Theme theme() const;

    void setEditorSettings(const EditorSettings &settings);
    EditorSettings editorSettings() const;

    void setResourceProvider(ResourceProvider *provider);

    /// Enable or disable editing. When read-only, the editor displays
    /// live-preview-formatted markdown but does not accept input.
    ///
    /// NOTE: Read-only mode does NOT prevent all user interaction with
    /// non-text block items. Specifically, TableBlockItem (and future
    /// interactive block items) may allow non-destructive display
    /// adjustments — such as column width resizing — that affect only
    /// the visual presentation and do not modify the underlying markdown.
    /// These are ephemeral viewport affordances for readability, not
    /// editing operations, and are not persisted or serialized.
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;

    // --- Editing actions ---
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();

    // --- Formatting actions ---
    void toggleBold();
    void toggleItalic();
    void toggleStrikethrough();
    void toggleInlineCode();
    void insertLink();
    void insertWikiLink();
    void insertImage();
    void insertCodeBlock();
    void insertBlockQuote();
    void insertHorizontalRule();
    void insertTable(int rows, int cols);
    void increaseHeadingLevel();
    void decreaseHeadingLevel();
    void toggleCheckbox();
    void insertCallout(const QString &type);

    // --- Cursor & navigation ---
    int cursorLine() const;
    int cursorColumn() const;
    QRect cursorScreenRect() const;
    void goToLine(int line);
    void scrollToHeading(const HeadingInfo &heading);

    // --- Font size (kept for test-app use) ---
    void setFontSize(int pointSize);

    // --- Search ---
    bool findText(const QString &text, QTextDocument::FindFlags flags = {});
    bool replaceText(const QString &find, const QString &replace,
                     QTextDocument::FindFlags flags = {});
    int replaceAll(const QString &find, const QString &replace,
                   QTextDocument::FindFlags flags = {});

    /// Show the embedded find bar (Ctrl+F).
    void showSearchBar();
    /// Show the embedded find+replace bar (Ctrl+H).
    void showReplaceBar();
    /// Hide the search bar and clear highlights.
    void hideSearchBar();

    // --- Folding ---
    QList<QStringList> headingPaths() const;
    bool isFolded(const QStringList &path) const;
    QList<QStringList> foldedPaths() const;

    void fold(const QStringList &path);
    void unfold(const QStringList &path);
    void toggleFold(const QStringList &path);
    void toggleFoldAtCursor();

    void foldAll();
    void unfoldAll();
    void foldAllAtLevel(int level);
    void unfoldAllAtLevel(int level);
    void foldLevel(int n);
    void unfoldLevel(int n);

    QJsonObject serializeFoldState() const;
    void restoreFoldState(const QJsonObject &state);

    void setGutterVisible(bool visible);
    bool isGutterVisible() const;

    /// Test-only accessor. Do not use from host code.
    SceneCoordinator *coordinatorForTesting() const { return m_coordinator; }

Q_SIGNALS:
    void textChanged();
    void cursorPositionChanged(int line, int column);
    void undoAvailable(bool available);
    void redoAvailable(bool available);
    void modificationChanged(bool modified);
    void linkClicked(const QString &target);
    void linkHovered(const QString &target);
    void wikiLinkTrigger(int cursorPosition);
    void tagTrigger(int cursorPosition);
    void completionDismissHint();
    void headingsChanged(const QList<Markoff::HeadingInfo> &headings);
    void linksChanged(const QList<Markoff::LinkInfo> &links);
    void tagsChanged(const QList<Markoff::TagInfo> &tags);
    void wordCountChanged(int count);
    void foldStateChanged();
    void foldsAutoExpanded(const QList<QStringList> &paths);

protected:
    void resizeEvent(QResizeEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void rebuildScene();
    void ensureFocusedCursorVisible();
    void startAutoScroll(int mouseY);
    void stopAutoScroll();
    void doAutoScroll();
    void jumpToDocumentEdge(bool toStart, bool select);
    void pageUpDown(bool up, bool select);
    MarkdownTextItem *focusedTextItem() const;
    void onDocumentReparsed();
    void detectCompletionTriggers(const QString &insertedText);
    void wrapSelection(const QString &before, const QString &after);
    void insertAtCursor(const QString &text);
    void highlightAllMatches(const QString &text);
    void clearSearchHighlights();
    void updateMatchCount();
    void repositionSearchBar();
    void repositionFoldGutter();
    QTextDocument::FindFlags searchFlags() const;

    SelectionScene *m_scene = nullptr;
    SceneCoordinator *m_coordinator = nullptr;
    QString m_sourceText;
    int m_fontSize = 14;
    QTimer *m_autoScrollTimer = nullptr;
    int m_autoScrollDelta = 0;
    bool m_autoScrollActive = false;
    bool m_readOnly = false;

    Theme m_theme;
    EditorSettings m_editorSettings;
    ResourceProvider *m_resourceProvider = nullptr;
    std::unique_ptr<Document> m_document;

    SearchBar *m_searchBar = nullptr;
    QString m_lastSearchText;
    int m_currentMatchIndex = -1;
    int m_totalMatchCount = 0;

    FoldingModel *m_foldingModel = nullptr;
    FoldGutter *m_foldGutter = nullptr;
    bool m_gutterVisible = true;
};

} // namespace Markoff

Q_DECLARE_METATYPE(QList<QStringList>)

#endif // MARKOFF_EDITOR_H
