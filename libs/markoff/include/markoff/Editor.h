// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_EDITOR_H
#define MARKOFF_EDITOR_H

#include <QGraphicsView>
#include <QTextDocument>
#include <markoff/Theme.h>
#include <markoff/EditorSettings.h>
#include <markoff/RenderSettings.h>
#include <markoff-parser/Document.h>

class QTimer;

namespace Markoff {

class SelectionScene;
class SceneCoordinator;
class MarkdownTextItem;
class ResourceProvider;

class Editor : public QGraphicsView {
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    enum class Mode { Source, LivePreview };
    Q_ENUM(Mode)

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

    void setRenderSettings(const RenderSettings &settings);
    RenderSettings renderSettings() const;

    void setResourceProvider(ResourceProvider *provider);

    // --- Mode ---
    void setMode(Mode mode);
    Mode mode() const;

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

Q_SIGNALS:
    void textChanged();
    void modeChanged(Markoff::Editor::Mode mode);
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

    SelectionScene *m_scene = nullptr;
    SceneCoordinator *m_coordinator = nullptr;
    Mode m_mode = Mode::Source;
    QString m_sourceText;
    int m_fontSize = 14;
    QTimer *m_autoScrollTimer = nullptr;
    int m_autoScrollDelta = 0;
    bool m_autoScrollActive = false;

    Theme m_theme;
    EditorSettings m_editorSettings;
    RenderSettings m_renderSettings;
    ResourceProvider *m_resourceProvider = nullptr;
    std::unique_ptr<Document> m_document;
};

} // namespace Markoff

#endif // MARKOFF_EDITOR_H
