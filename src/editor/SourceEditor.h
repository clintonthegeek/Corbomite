// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaType>
#include <QVector>
#include <QWidget>

namespace Qutepart {
class Qutepart;
}

namespace Corbomite {

/**
 * App-facing Source-mode editor.
 *
 * Composes a private `Qutepart::Qutepart` child and exposes the Corbomite
 * public contract: plain-text content, (line, column) cursor, visual-line
 * float scroll, and a fold-state round-trip stub.
 *
 * Phase 2 (2026-04-15) of the qutepart-corbomite fork plan. Find / replace
 * (Phase 3) and markdown-specific wiki-link awareness (Phase 7) deliberately
 * not implemented yet — this shim carries only the API surface needed by
 * Cluster E Phase 1 (ViewMode encoding + EphemeralState scroll persistence).
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

    // Fold — Phase 2 scaffold only. Round-trips an empty vector; real fold
    // serialization lands in Phase 4 (FoldCalculator) / Phase 7
    // (markdown-heading section fold).
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
    Qutepart::Qutepart *qutepart() const { return m_qutepart; }

Q_SIGNALS:
    void textChanged();
    void cursorPositionChanged(CursorPos pos);
    void scrollPositionChanged(float visualLine);

private:
    Qutepart::Qutepart *m_qutepart = nullptr;
    QVector<int> m_pendingFoldedHeadings; // TODO(phase-4/phase-7): honour
    QFont m_defaultFont; // Captured at ctor for resetZoom().
};

} // namespace Corbomite

Q_DECLARE_METATYPE(Corbomite::SourceEditor::CursorPos)
