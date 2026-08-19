// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QListWidget;

namespace Canvas {

/// M2.2 — modal file picker for the canvas "New file card…" context-menu
/// action. Takes a flat candidate list (vault-relative markdown +
/// attachment paths, per the app-side wiring) and fuzzy-filters/-ranks it
/// live via Corbomite::FuzzyMatcher as the user types — the same matcher
/// used by CompletionPopup/QuickSwitcher for `[[` wikilink and Ctrl+O
/// candidates.
class CanvasFilePickerDialog : public QDialog {
    Q_OBJECT

public:
    explicit CanvasFilePickerDialog(const QStringList &candidatePaths, QWidget *parent = nullptr);

    /// The vault-relative path of the accepted selection, or an empty
    /// string if the dialog was cancelled / closed without a selection.
    QString selectedPath() const;

    /// Convenience: exec() the dialog and return the picked path, or an
    /// empty string on cancel. Owns and destroys the dialog itself.
    static QString pickFile(QWidget *parent, const QStringList &candidatePaths);

private Q_SLOTS:
    void updateResults(const QString &filterText);
    void acceptCurrentSelection();

private:
    QStringList m_candidates;
    QString m_selectedPath;
    QLineEdit *m_filterEdit = nullptr;
    QListWidget *m_resultsList = nullptr;
};

} // namespace Canvas
