// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QListView>
#include <QSortFilterProxyModel>

class QAbstractItemModel;

namespace Corbomite {

class CompletionDelegate;

/// Non-focus-stealing completion popup, modeled on KDevelop's
/// `KateCompletionWidget` (`~/src/kde/src/ktexteditor/src/completion/`).
///
/// Spec: `docs/superpowers/specs/2026-04-15-completion-popup-rewrite.md`.
///
/// Construction contract:
///   - The `parent` argument MUST be a real widget (typically the
///     editor's viewport). The popup parents itself there as a regular
///     child widget — *not* a top-level `Qt::Popup` window. This is
///     what keeps keystrokes flowing into the editor.
///   - The popup itself + every child has `Qt::NoFocus`. Keyboard
///     navigation is driven externally via `selectNext` /
///     `selectPrevious` / `acceptCurrent`, called by the editor's
///     event filter.
///
/// Lifetime: parent owns. Caller deletes (or `deleteLater`s) when the
/// owning trigger ends.
class CompletionPopup : public QFrame {
    Q_OBJECT

public:
    explicit CompletionPopup(QAbstractItemModel *sourceModel, QWidget *parent);

    void setFilterText(const QString &text);
    void selectNext();
    void selectPrevious();
    QString selectedText() const;
    QString selectedData() const;
    bool hasSelection() const;
    int visibleRowCount() const;

    /// Content-driven size: width is fixed; height grows with the number
    /// of visible rows (capped, then the list scrolls). The controller
    /// anchors against this hint, so it must reflect the real shown size.
    QSize sizeHint() const override;

    /// Emit `itemSelected` for the currently-highlighted row. Returns
    /// false if no row is selected.
    bool acceptCurrent();

Q_SIGNALS:
    void itemSelected(const QString &text, const QString &data);
    void dismissed();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void onActivated(const QModelIndex &index);
    int contentHeight() const;       ///< pixel height for the current row count
    int contentWidth() const;        ///< pixel width to fit display + detail
    void resizeToContents();         ///< apply content width/height to the widget

    QListView *m_listView;
    QSortFilterProxyModel *m_proxyModel;
    CompletionDelegate *m_delegate;
    QString m_filterText;
};

} // namespace Corbomite
