// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>

class QLineEdit;
class QLabel;
class QPushButton;
class QToolButton;
class QWidget;

namespace Markoff { class FindController; }

namespace Corbomite {

/// Horizontal in-document Find bar docked at the bottom of NoteEditorWidget.
/// Binds to a Markoff::FindController via setController(). Search-as-you-type:
/// every keystroke updates controller.needle; Return / Shift+Return drive
/// navigation; Esc emits closeRequested. No LineEdit color feedback — count
/// label carries all "no matches" messaging.
class FindBar : public QFrame {
    Q_OBJECT
public:
    explicit FindBar(QWidget *parent = nullptr);
    ~FindBar() override;

    /// Bind to a controller. Passing nullptr detaches. Safe to call repeatedly.
    void setController(Markoff::FindController *controller);
    Markoff::FindController *controller() const;

    /// Focus the line edit. Called by NoteEditorWidget::showFindBar.
    void focusLineEdit();

    /// Toggle the replace row (a second row with a replacement field + Replace
    /// / Replace All). Hidden by default; find-only behavior is unchanged.
    void setReplaceMode(bool on);
    bool isReplaceMode() const { return m_replaceMode; }
    QString replacementText() const;

Q_SIGNALS:
    void closeRequested();
    void replaceRequested();
    void replaceAllRequested();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void onLineEditTextChanged(const QString &text);
    void onNeedleChanged();
    void onMatchesChanged();
    void onCurrentMatchChanged();
    void refreshCountLabel();
    void refreshButtonEnableState();

    QLineEdit   *m_lineEdit   = nullptr;
    QLabel      *m_countLabel = nullptr;
    QPushButton *m_prevButton = nullptr;
    QPushButton *m_nextButton = nullptr;
    QToolButton *m_closeButton = nullptr;
    Markoff::FindController *m_controller = nullptr;
    bool m_applyingControllerNeedle = false;

    QWidget     *m_replaceRow       = nullptr;
    QLineEdit   *m_replaceLineEdit  = nullptr;
    QPushButton *m_replaceButton    = nullptr;
    QPushButton *m_replaceAllButton = nullptr;
    bool         m_replaceMode      = false;
};

} // namespace Corbomite
