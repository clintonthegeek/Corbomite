// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFont>
#include <QMetaType>
#include <QVector>
#include <QWidget>

class QPlainTextEdit;

// Port history: Qutepart::Qutepart was a public class in the old Markoff
// layout. The standalone qutepart library was retired (markoff-source's
// QPlainTextEdit-based widget now subsumes that role).
// This shim is stubbed to a plain QPlainTextEdit; the real port-feature is to
// replace Corbomite::SourceEditor with Markoff::Source::Editor at the
// NoteEditorWidget seam. Until then, visual-line scroll is approximated by
// scrollbar-value passthrough.

namespace Corbomite {

/**
 * App-facing Source-mode editor.
 *
 * Composes a private `Qutepart::Qutepart` child and exposes the Corbomite
 * public contract: plain-text content, (line, column) cursor, visual-line
 * float scroll, and a fold-state round-trip stub.
 *
 * Second stage (2026-04-15) of the qutepart-corbomite fork plan. Find / replace
 * and markdown-specific wiki-link awareness are deliberately not implemented yet
 * — this shim carries only the API surface needed by the ViewMode encoding
 * and EphemeralState scroll persistence.
 */
class SourceEditor : public QWidget {
    Q_OBJECT

public:
    explicit SourceEditor(QWidget *parent = nullptr);
    ~SourceEditor() override;

    // Content
    void setPlainText(const QString &text);
    QString toPlainText() const;

    // Cursor
    struct CursorPos {
        int line = 0;
        int column = 0;

        friend bool operator==(const CursorPos &a, const CursorPos &b) {
            return a.line == b.line && a.column == b.column;
        }
    };
    CursorPos cursorPosition() const;
    void setCursorPosition(CursorPos pos);

    // Scroll — visual-line float (see scrollPositionVisualLine / set*).
    float scrollPosition() const;
    void setScrollPosition(float visualLine);

    // Fold — scaffold only. Round-trips an empty vector; real fold
    // serialization lands with FoldCalculator and the markdown-heading
    // section fold.
    QVector<int> foldedHeadings() const;
    void setFoldedHeadings(const QVector<int> &foldedLines);

    // Read-only
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;

    // Zoom — delegates to the underlying Qutepart/QPlainTextEdit font zoom.
    // `resetZoom()` restores the font captured at construction time.
    void zoomIn();
    void zoomOut();
    void resetZoom();

    // Internal-use accessor for tests and future phase-3/phase-7 wiring.
    // NOT part of the app-facing contract.
    // Port history: this originally returned Qutepart::Qutepart *. It is
    // stubbed to QPlainTextEdit * pending the source-editor swap.
    QPlainTextEdit *qutepart() const { return m_qutepart; }

Q_SIGNALS:
    void textChanged();
    void cursorPositionChanged(CursorPos pos);
    void scrollPositionChanged(float visualLine);

private:
    QPlainTextEdit *m_qutepart = nullptr;  // formerly Qutepart::Qutepart *
    QVector<int> m_pendingFoldedHeadings; // Pending: no fold engine in the shim
    QFont m_defaultFont; // Captured at ctor for resetZoom().
};

} // namespace Corbomite

Q_DECLARE_METATYPE(Corbomite::SourceEditor::CursorPos)
