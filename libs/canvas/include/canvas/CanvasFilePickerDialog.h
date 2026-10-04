// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QStringList>
#include <QVector>

class QLineEdit;
class QListWidget;

namespace Canvas {

/// M2.2 — modal file picker for the canvas "New file card…" context-menu
/// action. Takes a flat candidate list (vault-relative markdown +
/// attachment paths, per the app-side wiring) and fuzzy-filters/-ranks it
/// live via Corbomite::FuzzyMatcher as the user types — the same matcher
/// used by CompletionPopup/QuickSwitcher for `[[` wikilink and Ctrl+O
/// candidates.
///
/// M5.5 reuses it for "Jump to group" over group labels, hence the
/// display/payload split: two groups may legitimately carry the same label,
/// and the caller needs the chosen group's node id, not its text.
class CanvasFilePickerDialog : public QDialog {
    Q_OBJECT

public:
    struct Entry {
        QString display;
        QString payload;
    };

    explicit CanvasFilePickerDialog(const QVector<Entry> &entries, QWidget *parent = nullptr,
                                    const QString &title = QString(),
                                    const QString &placeholder = QString());

    /// The vault-relative path of the accepted selection, or an empty
    /// string if the dialog was cancelled / closed without a selection.
    QString selectedPath() const;

    /// The accepted entry's payload, or empty if cancelled. Identical to
    /// selectedPath() when every payload was set to its display string.
    QString selectedPayload() const;

    /// Convenience: exec() the dialog and return the picked path, or an
    /// empty string on cancel. Owns and destroys the dialog itself.
    static QString pickFile(QWidget *parent, const QStringList &candidatePaths);

    /// As pickFile(), over caller-supplied entries, returning the payload.
    static QString pickPayload(QWidget *parent, const QVector<Entry> &entries,
                               const QString &title, const QString &placeholder);

private Q_SLOTS:
    void updateResults(const QString &filterText);
    void acceptCurrentSelection();

private:
    QVector<Entry> m_entries;
    QString m_selectedPayload;
    QLineEdit *m_filterEdit = nullptr;
    QListWidget *m_resultsList = nullptr;
};

} // namespace Canvas
