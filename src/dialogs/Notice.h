// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QFrame>
#include <QString>
#include <QTimer>

class QHBoxLayout;
class QLabel;
class QPushButton;

namespace Corbomite {

// Cluster H Phase 5 — transient toast widget mirroring Obsidian's Notice.
//
// Floating Qt::ToolTip frame anchored at the bottom-right of the active
// screen. Auto-dismisses after `durationMs` (default 4000ms — Obsidian's
// shipped default; KMessageWidget was rejected as a backing because it's
// designed for inline embedding, not floating stacked toasts).
//
// Optional action button: `setAction(label, callback)` adds a clickable
// button next to the message; the callback fires before the notice
// dismisses. Action is suitable for undo/retry/dismiss-link semantics.
//
// Lifetime: the notice deletes itself when dismissed (WA_DeleteOnClose).
// Callers should `show()` and forget; do not keep a long-lived pointer.
class Notice : public QFrame {
    Q_OBJECT

public:
    static constexpr int kDefaultDurationMs = 4000;

    explicit Notice(const QString &message, int durationMs = kDefaultDurationMs,
                     QWidget *parent = nullptr);
    ~Notice() override;

    void setAction(const QString &label, std::function<void()> callback);

    // Position at the bottom-right of `screen`'s available geometry with a
    // small margin. Called automatically from showEvent.
    void anchorBottomRight();

    QString message() const;

protected:
    void showEvent(QShowEvent *event) override;

private:
    QLabel *m_label = nullptr;
    QPushButton *m_actionButton = nullptr;
    QHBoxLayout *m_layout = nullptr;
    QTimer m_dismissTimer;
};

} // namespace Corbomite
