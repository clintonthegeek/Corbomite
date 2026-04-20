# Completion Popups & Link Navigation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add wikilink autocomplete (`[[`), tag autocomplete (`#`), and Ctrl+Click wikilink navigation to the editor.

**Architecture:** Shared `CompletionPopup` widget with fuzzy filtering (KFuzzyMatcher). NoteEditorWidget detects triggers via keyPressEvent, creates popup with appropriate model, and handles insertion. Ctrl+Click uses the highlighter's InlineRange data to detect wikilinks under cursor.

**Tech Stack:** C++20, Qt6 Widgets, KFuzzyMatcher (KCoreAddons), QMarkdownTextEdit

**Spec:** `docs/superpowers/specs/2026-03-31-completion-and-link-navigation-design.md`

**Current state:** 14 tests passing. Obsidian syntax highlighting complete. QuickSwitcherModel available for reuse.

---

### Task 1: VaultModel::allTags()

**Files:**
- Modify: `libs/models/include/corbomite/models/VaultModel.h`
- Modify: `libs/models/src/VaultModel.cpp`
- Modify: `tests/models/tst_vaultmodel.cpp`

- [ ] **Step 1: Write allTags tests**

Add to `tests/models/tst_vaultmodel.cpp`:

```cpp
    void testAllTags()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note1.md", QStringLiteral("# Title\n\nHello #project world #status/active\n"));
        createFile(tmp.path() + "/note2.md", QStringLiteral("Text with #project and #idea\n"));

        Corbomite::VaultModel model;
        model.open(tmp.path());

        auto tags = model.allTags();

        QVERIFY(tags.contains(QStringLiteral("project")));
        QVERIFY(tags.contains(QStringLiteral("status/active")));
        QVERIFY(tags.contains(QStringLiteral("idea")));
        // "project" appears in both files but only listed once
        QCOMPARE(tags.count(QStringLiteral("project")), 1);
        // Sorted alphabetically
        QVERIFY(std::is_sorted(tags.begin(), tags.end()));
    }

    void testAllTagsExcludesCodeBlocks()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createFile(tmp.path() + "/note.md", QStringLiteral("Real #tag here\n\n```\n#not-a-tag\n```\n"));

        Corbomite::VaultModel model;
        model.open(tmp.path());

        auto tags = model.allTags();

        QVERIFY(tags.contains(QStringLiteral("tag")));
        QVERIFY(!tags.contains(QStringLiteral("not-a-tag")));
    }

    void testAllTagsEmptyVault()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Corbomite::VaultModel model;
        model.open(tmp.path());

        QVERIFY(model.allTags().isEmpty());
    }
```

- [ ] **Step 2: Add allTags() declaration to VaultModel.h**

In `libs/models/include/corbomite/models/VaultModel.h`, add in the public section after `allNotes()`:

```cpp
    QStringList allTags() const;
    void invalidateTagCache();
```

Add private member:

```cpp
    mutable QStringList m_cachedTags;
    mutable bool m_tagCacheDirty = true;
```

- [ ] **Step 3: Implement allTags()**

In `libs/models/src/VaultModel.cpp`, add:

```cpp
QStringList VaultModel::allTags() const
{
    if (!m_tagCacheDirty) {
        return m_cachedTags;
    }

    QSet<QString> tagSet;
    static const QRegularExpression tagPattern(
        QStringLiteral(R"((?<![&\w])#([a-zA-Z_][a-zA-Z0-9_/-]*))"));

    // Simple code block exclusion: track ``` state per line
    static const QRegularExpression codeFencePattern(QStringLiteral(R"(^```)"));

    for (const auto &meta : m_notes) {
        auto content = m_fs.readFile(m_vaultPath + QLatin1Char('/') + meta.relativePath);
        if (!content.has_value()) continue;

        bool inCodeBlock = false;
        const auto lines = content.value().split(QLatin1Char('\n'));
        for (const auto &line : lines) {
            if (codeFencePattern.match(line).hasMatch()) {
                inCodeBlock = !inCodeBlock;
                continue;
            }
            if (inCodeBlock) continue;

            auto it = tagPattern.globalMatch(line);
            while (it.hasNext()) {
                auto match = it.next();
                tagSet.insert(match.captured(1));
            }
        }
    }

    m_cachedTags = tagSet.values();
    std::sort(m_cachedTags.begin(), m_cachedTags.end());
    m_tagCacheDirty = false;
    return m_cachedTags;
}

void VaultModel::invalidateTagCache()
{
    m_tagCacheDirty = true;
}
```

Also, in `addNote()`, `removeNote()`, and `updateNoteMeta()`, add `m_tagCacheDirty = true;` to invalidate.

In `close()`, add `m_cachedTags.clear(); m_tagCacheDirty = true;`.

- [ ] **Step 4: Build and run tests**

Run:
```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_vaultmodel --output-on-failure
```

Expected: All VaultModel tests pass including new tag tests.

- [ ] **Step 5: Commit**

```bash
git add libs/models/ tests/models/tst_vaultmodel.cpp
git commit -m "feat: add VaultModel::allTags() with code block exclusion and caching"
```

---

### Task 2: CompletionPopup and CompletionDelegate

**Files:**
- Create: `src/editor/CompletionPopup.h`
- Create: `src/editor/CompletionPopup.cpp`
- Create: `src/editor/CompletionDelegate.h`
- Create: `src/editor/CompletionDelegate.cpp`
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: Implement CompletionDelegate**

`src/editor/CompletionDelegate.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QStyledItemDelegate>

namespace Corbomite {

class CompletionDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit CompletionDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    void setFilterPattern(const QString &pattern);

private:
    QString m_pattern;
};

} // namespace Corbomite
```

`src/editor/CompletionDelegate.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompletionDelegate.h"

#include <KFuzzyMatcher>
#include <QPainter>
#include <QApplication>

namespace Corbomite {

CompletionDelegate::CompletionDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void CompletionDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    painter->save();

    QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    const QString text = index.data(Qt::DisplayRole).toString();
    const int padding = 4;
    QRect textRect = option.rect.adjusted(padding, 0, -padding, 0);

    if (!m_pattern.isEmpty()) {
        auto ranges = KFuzzyMatcher::matchedRanges(m_pattern, text);

        int x = textRect.left();
        int y = textRect.center().y() + option.fontMetrics.ascent() / 2 - 1;

        for (int i = 0; i < text.length(); ++i) {
            bool highlighted = false;
            for (const auto &range : ranges) {
                if (i >= range.start && i < range.start + range.length) {
                    highlighted = true;
                    break;
                }
            }

            QFont charFont = option.font;
            if (highlighted) {
                charFont.setBold(true);
                painter->setPen(option.palette.color(QPalette::Link));
            } else {
                painter->setPen(option.palette.color(QPalette::Text));
            }
            painter->setFont(charFont);
            QString ch = text.mid(i, 1);
            painter->drawText(x, y, ch);
            x += QFontMetrics(charFont).horizontalAdvance(ch);
        }
    } else {
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
    }

    painter->restore();
}

QSize CompletionDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QSize(option.rect.width(), option.fontMetrics.height() + 8);
}

void CompletionDelegate::setFilterPattern(const QString &pattern)
{
    m_pattern = pattern;
}

} // namespace Corbomite
```

- [ ] **Step 2: Implement CompletionPopup**

`src/editor/CompletionPopup.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFrame>
#include <QListView>
#include <QSortFilterProxyModel>

class QAbstractItemModel;

namespace Corbomite {

class CompletionDelegate;

class CompletionPopup : public QFrame {
    Q_OBJECT

public:
    explicit CompletionPopup(QAbstractItemModel *sourceModel, QWidget *parent = nullptr);

    void setFilterText(const QString &text);
    void selectNext();
    void selectPrevious();
    QString selectedText() const;
    QString selectedData() const;
    bool hasSelection() const;

Q_SIGNALS:
    void itemSelected(const QString &text, const QString &data);
    void dismissed();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void onActivated(const QModelIndex &index);

    QListView *m_listView;
    QSortFilterProxyModel *m_proxyModel;
    CompletionDelegate *m_delegate;
    QString m_filterText;
};

} // namespace Corbomite
```

`src/editor/CompletionPopup.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CompletionPopup.h"
#include "CompletionDelegate.h"

#include <KFuzzyMatcher>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QApplication>

namespace Corbomite {

// Fuzzy proxy for completion
class CompletionFilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setFilterPattern(const QString &pattern)
    {
        m_pattern = pattern;
        beginFilterChange();
        endFilterChange();
        sort(0);
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        Q_UNUSED(sourceParent)
        if (m_pattern.isEmpty()) return true;

        auto idx = sourceModel()->index(sourceRow, 0);
        QString text = idx.data(Qt::DisplayRole).toString();
        return KFuzzyMatcher::matchSimple(m_pattern, text);
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        if (m_pattern.isEmpty()) {
            return left.data(Qt::DisplayRole).toString()
                       .compare(right.data(Qt::DisplayRole).toString(), Qt::CaseInsensitive) < 0;
        }
        QString leftText = left.data(Qt::DisplayRole).toString();
        QString rightText = right.data(Qt::DisplayRole).toString();
        return KFuzzyMatcher::match(m_pattern, leftText).score
             > KFuzzyMatcher::match(m_pattern, rightText).score;
    }

private:
    QString m_pattern;
};

CompletionPopup::CompletionPopup(QAbstractItemModel *sourceModel, QWidget *parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedWidth(300);
    setMaximumHeight(200);
    setFrameShape(QFrame::StyledPanel);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(12);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(0, 0, 0, 60));
    setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);

    m_listView = new QListView(this);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_listView);

    m_proxyModel = new CompletionFilterProxy(this);
    m_proxyModel->setSourceModel(sourceModel);
    m_proxyModel->sort(0);
    m_listView->setModel(m_proxyModel);

    m_delegate = new CompletionDelegate(this);
    m_listView->setItemDelegate(m_delegate);

    connect(m_listView, &QListView::activated, this, &CompletionPopup::onActivated);
}

void CompletionPopup::setFilterText(const QString &text)
{
    m_filterText = text;
    static_cast<CompletionFilterProxy *>(m_proxyModel)->setFilterPattern(text);
    m_delegate->setFilterPattern(text);

    if (m_proxyModel->rowCount() > 0) {
        m_listView->setCurrentIndex(m_proxyModel->index(0, 0));
    }

    m_listView->viewport()->update();

    // Auto-dismiss if no matches
    if (m_proxyModel->rowCount() == 0 && !text.isEmpty()) {
        // Keep open — user might still be typing
    }
}

void CompletionPopup::selectNext()
{
    auto current = m_listView->currentIndex();
    int next = current.isValid() ? current.row() + 1 : 0;
    if (next < m_proxyModel->rowCount()) {
        m_listView->setCurrentIndex(m_proxyModel->index(next, 0));
    }
}

void CompletionPopup::selectPrevious()
{
    auto current = m_listView->currentIndex();
    int prev = current.isValid() ? current.row() - 1 : 0;
    if (prev >= 0) {
        m_listView->setCurrentIndex(m_proxyModel->index(prev, 0));
    }
}

QString CompletionPopup::selectedText() const
{
    auto current = m_listView->currentIndex();
    if (!current.isValid()) return {};
    return current.data(Qt::DisplayRole).toString();
}

QString CompletionPopup::selectedData() const
{
    auto current = m_listView->currentIndex();
    if (!current.isValid()) return {};
    return current.data(Qt::UserRole + 1).toString();
}

bool CompletionPopup::hasSelection() const
{
    return m_listView->currentIndex().isValid();
}

void CompletionPopup::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    if (m_proxyModel->rowCount() > 0) {
        m_listView->setCurrentIndex(m_proxyModel->index(0, 0));
    }
}

void CompletionPopup::onActivated(const QModelIndex &index)
{
    if (!index.isValid()) return;
    Q_EMIT itemSelected(index.data(Qt::DisplayRole).toString(),
                        index.data(Qt::UserRole + 1).toString());
    close();
}

} // namespace Corbomite
```

- [ ] **Step 3: Add to src/CMakeLists.txt**

Add `editor/CompletionPopup.cpp` and `editor/CompletionDelegate.cpp` to CorbomiteApp sources.

- [ ] **Step 4: Build**

Run: `cmake --build build`

Expected: Builds cleanly.

- [ ] **Step 5: Commit**

```bash
git add src/editor/CompletionPopup.h src/editor/CompletionPopup.cpp \
        src/editor/CompletionDelegate.h src/editor/CompletionDelegate.cpp \
        src/CMakeLists.txt
git commit -m "feat: add CompletionPopup and CompletionDelegate widgets

Reusable inline completion popup with KFuzzyMatcher filtering,
match highlighting, arrow key navigation, and auto-sizing.
Used by both wikilink and tag autocomplete."
```

---

### Task 3: Wikilink Autocomplete in NoteEditorWidget

**Files:**
- Modify: `src/editor/NoteEditorWidget.h`
- Modify: `src/editor/NoteEditorWidget.cpp`

- [ ] **Step 1: Add completion support to NoteEditorWidget header**

Update `src/editor/NoteEditorWidget.h`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qmarkdowntextedit.h>

namespace Corbomite {

class NoteDocument;
class VaultModel;
class CompletionPopup;

class NoteEditorWidget : public QMarkdownTextEdit {
    Q_OBJECT

public:
    explicit NoteEditorWidget(QWidget *parent = nullptr);

    void setNoteDocument(NoteDocument *doc);
    NoteDocument *noteDocument() const;
    void setVaultModel(VaultModel *vault);

    int currentLine() const;
    int currentColumn() const;

Q_SIGNALS:
    void cursorInfoChanged(int line, int column, int wordCount);
    void linkActivated(const QString &targetPath);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void onTextChanged();
    void onCursorPositionChanged();
    void syncFromDocument();

    // Completion
    void triggerWikiLinkCompletion();
    void triggerTagCompletion();
    void dismissCompletion();
    void onCompletionAccepted(const QString &text, const QString &data);
    void updateCompletionFilter();
    int completionTriggerPos() const;
    QString textFromTrigger() const;

    // Link navigation
    QString wikiLinkTargetAtCursor(const QPoint &pos) const;
    QString resolveWikiLinkTarget(const QString &rawTarget) const;

    NoteDocument *m_doc = nullptr;
    VaultModel *m_vault = nullptr;
    bool m_updatingFromDoc = false;

    // Completion state
    CompletionPopup *m_completionPopup = nullptr;
    int m_completionTriggerPos = -1;
    enum class CompletionMode { None, WikiLink, Tag };
    CompletionMode m_completionMode = CompletionMode::None;
    // Future: support [[Note|Display]] alias insertion mode
    // Future: respect "use markdown links" setting
};

} // namespace Corbomite
```

- [ ] **Step 2: Implement the full NoteEditorWidget with completion and Ctrl+Click**

Replace `src/editor/NoteEditorWidget.cpp` entirely:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoteEditorWidget.h"
#include "CompletionPopup.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/models/VaultModel.h"
#include "dialogs/QuickSwitcherModel.h"
#include "markdownhighlighter.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QTextCursor>
#include <QStringListModel>
#include <QApplication>
#include <QRegularExpression>

namespace Corbomite {

NoteEditorWidget::NoteEditorWidget(QWidget *parent)
    : QMarkdownTextEdit(parent)
{
    connect(this, &QPlainTextEdit::textChanged, this, &NoteEditorWidget::onTextChanged);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &NoteEditorWidget::onCursorPositionChanged);
}

void NoteEditorWidget::setNoteDocument(NoteDocument *doc)
{
    m_doc = doc;
    if (m_doc) {
        syncFromDocument();
    } else {
        clear();
    }
}

NoteDocument *NoteEditorWidget::noteDocument() const
{
    return m_doc;
}

void NoteEditorWidget::setVaultModel(VaultModel *vault)
{
    m_vault = vault;
}

int NoteEditorWidget::currentLine() const
{
    return textCursor().blockNumber() + 1;
}

int NoteEditorWidget::currentColumn() const
{
    return textCursor().columnNumber() + 1;
}

void NoteEditorWidget::keyPressEvent(QKeyEvent *event)
{
    // If completion popup is visible, handle navigation keys
    if (m_completionPopup) {
        switch (event->key()) {
        case Qt::Key_Down:
            m_completionPopup->selectNext();
            return;
        case Qt::Key_Up:
            m_completionPopup->selectPrevious();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
            if (m_completionPopup->hasSelection()) {
                onCompletionAccepted(m_completionPopup->selectedText(),
                                     m_completionPopup->selectedData());
                return;
            }
            break;
        case Qt::Key_Escape:
            dismissCompletion();
            return;
        default:
            break;
        }
    }

    // Let the base class handle the key first (inserts the character)
    QMarkdownTextEdit::keyPressEvent(event);

    // Check for completion triggers after the character is inserted
    if (event->text().isEmpty()) return;

    QChar typed = event->text().at(0);

    if (typed == QLatin1Char('[')) {
        // Check if we just typed the second [
        QTextCursor cursor = textCursor();
        int pos = cursor.positionInBlock();
        QString blockText = cursor.block().text();
        if (pos >= 2 && blockText.mid(pos - 2, 2) == QStringLiteral("[[")) {
            triggerWikiLinkCompletion();
            return;
        }
    }

    if (typed == QLatin1Char('#') && !m_completionPopup) {
        // Check it's not a heading (# at line start followed by space)
        QTextCursor cursor = textCursor();
        int pos = cursor.positionInBlock();
        if (pos == 1) {
            // Just # at start of line — wait for next char to determine
            // Don't trigger yet; if next char is space it's a heading
        } else if (pos > 1) {
            // # in the middle of a line — trigger tag completion
            triggerTagCompletion();
            return;
        }
    }

    // Update filter if completion is active
    if (m_completionPopup) {
        updateCompletionFilter();

        // Dismiss if user typed ]] (closed the wikilink manually)
        if (m_completionMode == CompletionMode::WikiLink && typed == QLatin1Char(']')) {
            QTextCursor cursor = textCursor();
            int pos = cursor.positionInBlock();
            QString blockText = cursor.block().text();
            if (pos >= 2 && blockText.mid(pos - 2, 2) == QStringLiteral("]]")) {
                dismissCompletion();
            }
        }

        // Dismiss tag completion on space or punctuation
        if (m_completionMode == CompletionMode::Tag &&
            (typed.isSpace() || typed == QLatin1Char(',') || typed == QLatin1Char('.'))) {
            dismissCompletion();
        }
    }
}

void NoteEditorWidget::mousePressEvent(QMouseEvent *event)
{
    // Ctrl+Click to follow wikilink
    if (event->button() == Qt::LeftButton &&
        event->modifiers() & Qt::ControlModifier) {
        QString target = wikiLinkTargetAtCursor(event->pos());
        if (!target.isEmpty()) {
            QString resolved = resolveWikiLinkTarget(target);
            Q_EMIT linkActivated(resolved);
            return;
        }
    }

    QMarkdownTextEdit::mousePressEvent(event);
}

void NoteEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Show pointing hand cursor when Ctrl+hovering over wikilink
    if (event->modifiers() & Qt::ControlModifier) {
        QString target = wikiLinkTargetAtCursor(event->pos());
        if (!target.isEmpty()) {
            viewport()->setCursor(Qt::PointingHandCursor);
            QMarkdownTextEdit::mouseMoveEvent(event);
            return;
        }
    }
    viewport()->setCursor(Qt::IBeamCursor);
    QMarkdownTextEdit::mouseMoveEvent(event);
}

void NoteEditorWidget::onTextChanged()
{
    if (m_updatingFromDoc || !m_doc) return;
    m_doc->setMarkdown(toPlainText());
}

void NoteEditorWidget::onCursorPositionChanged()
{
    if (!m_doc) return;
    Q_EMIT cursorInfoChanged(currentLine(), currentColumn(), m_doc->wordCount());
}

void NoteEditorWidget::syncFromDocument()
{
    if (!m_doc) return;
    m_updatingFromDoc = true;
    setPlainText(m_doc->markdown());
    m_doc->setModified(false);
    m_updatingFromDoc = false;
}

// --- Completion ---

void NoteEditorWidget::triggerWikiLinkCompletion()
{
    if (!m_vault) return;
    dismissCompletion();

    m_completionMode = CompletionMode::WikiLink;
    m_completionTriggerPos = textCursor().position();

    // Reuse QuickSwitcherModel for note list
    auto *model = new QuickSwitcherModel(this);
    model->setNotes(m_vault->allNotes());
    // Future: match against frontmatter aliases

    m_completionPopup = new CompletionPopup(model, this);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() { m_completionPopup = nullptr; m_completionMode = CompletionMode::None; });

    // Position below cursor
    QRect cr = cursorRect();
    QPoint pos = mapToGlobal(QPoint(cr.left(), cr.bottom() + 2));
    m_completionPopup->move(pos);
    m_completionPopup->show();
}

void NoteEditorWidget::triggerTagCompletion()
{
    if (!m_vault) return;
    dismissCompletion();

    m_completionMode = CompletionMode::Tag;
    m_completionTriggerPos = textCursor().position();

    auto tags = m_vault->allTags();
    auto *model = new QStringListModel(tags, this);
    // Future: include tags from frontmatter properties
    // Future: show tag usage count alongside name

    m_completionPopup = new CompletionPopup(model, this);
    connect(m_completionPopup, &CompletionPopup::itemSelected,
            this, &NoteEditorWidget::onCompletionAccepted);
    connect(m_completionPopup, &CompletionPopup::dismissed,
            this, [this]() { m_completionPopup = nullptr; m_completionMode = CompletionMode::None; });

    QRect cr = cursorRect();
    QPoint pos = mapToGlobal(QPoint(cr.left(), cr.bottom() + 2));
    m_completionPopup->move(pos);
    m_completionPopup->show();
}

void NoteEditorWidget::dismissCompletion()
{
    if (m_completionPopup) {
        m_completionPopup->close();
        m_completionPopup = nullptr;
    }
    m_completionMode = CompletionMode::None;
    m_completionTriggerPos = -1;
}

void NoteEditorWidget::onCompletionAccepted(const QString &text, const QString &data)
{
    Q_UNUSED(data)

    QTextCursor cursor = textCursor();

    if (m_completionMode == CompletionMode::WikiLink) {
        // Delete text typed after [[ trigger
        int currentPos = cursor.position();
        int deleteCount = currentPos - m_completionTriggerPos;
        if (deleteCount > 0) {
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, deleteCount);
            cursor.removeSelectedText();
        }
        // Insert NoteName]] — the [[ is already typed
        // Future: respect "use markdown links" setting — insert [text](path.md) instead
        cursor.insertText(text + QStringLiteral("]]"));
    } else if (m_completionMode == CompletionMode::Tag) {
        // Delete text typed after # trigger
        int currentPos = cursor.position();
        int deleteCount = currentPos - m_completionTriggerPos;
        if (deleteCount > 0) {
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, deleteCount);
            cursor.removeSelectedText();
        }
        // Insert tag name — the # is already typed
        cursor.insertText(text);
    }

    setTextCursor(cursor);
    dismissCompletion();
}

void NoteEditorWidget::updateCompletionFilter()
{
    if (!m_completionPopup || m_completionTriggerPos < 0) return;
    QString filterText = textFromTrigger();
    m_completionPopup->setFilterText(filterText);
}

int NoteEditorWidget::completionTriggerPos() const
{
    return m_completionTriggerPos;
}

QString NoteEditorWidget::textFromTrigger() const
{
    if (m_completionTriggerPos < 0) return {};
    QTextCursor cursor = textCursor();
    int length = cursor.position() - m_completionTriggerPos;
    if (length <= 0) return {};

    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, length);
    return cursor.selectedText();
}

// --- Link Navigation ---

QString NoteEditorWidget::wikiLinkTargetAtCursor(const QPoint &pos) const
{
    QTextCursor cursor = cursorForPosition(pos);
    int blockNum = cursor.blockNumber();
    int posInBlock = cursor.positionInBlock();
    QString blockText = cursor.block().text();

    // Find surrounding [[ and ]]
    int openPos = blockText.lastIndexOf(QStringLiteral("[["), posInBlock);
    if (openPos == -1) return {};

    int closePos = blockText.indexOf(QStringLiteral("]]"), openPos + 2);
    if (closePos == -1 || posInBlock > closePos + 1) return {};

    // Extract content between [[ and ]]
    QString content = blockText.mid(openPos + 2, closePos - openPos - 2);
    if (content.isEmpty()) return {};

    // Handle [[target|display]] — take the target part
    int pipePos = content.indexOf(QLatin1Char('|'));
    if (pipePos >= 0) {
        content = content.left(pipePos);
    }

    // Handle [[target#heading]] — take just the note path
    // Future: scroll to heading after navigation
    int hashPos = content.indexOf(QLatin1Char('#'));
    if (hashPos >= 0) {
        content = content.left(hashPos);
    }

    return content.trimmed();
}

QString NoteEditorWidget::resolveWikiLinkTarget(const QString &rawTarget) const
{
    if (rawTarget.isEmpty()) return {};

    // If it already has an extension, use as-is
    if (rawTarget.endsWith(QStringLiteral(".md")) || rawTarget.endsWith(QStringLiteral(".canvas"))) {
        return rawTarget;
    }

    // Append .md
    return rawTarget + QStringLiteral(".md");
}

} // namespace Corbomite
```

- [ ] **Step 3: Wire VaultModel into editors from MainWindow**

In `src/app/MainWindow.cpp`, in `onVaultOpened()`, after creating reactors, add:

```cpp
    // Pass vault model to editors for completion
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        if (editor && m_vaultService->vault()) {
            editor->setVaultModel(m_vaultService->vault());
        }
    }, Qt::UniqueConnection);
```

Also, connect `linkActivated` from the EditorViewManager level. In `EditorViewManager.h`, add a new signal:

```cpp
Q_SIGNALS:
    void activeEditorChanged(NoteEditorWidget *editor);
    void cursorInfoChanged(int line, int column, int wordCount);
    void linkActivated(const QString &targetPath);
```

In `EditorViewManager.cpp`, update the constructor to forward the signal. In `EditorViewSpace.cpp`, when a new editor is created in `openNote()`, connect `linkActivated`:

In `EditorViewSpace::openNote()`, after creating the editor, add:

```cpp
    connect(editor, &NoteEditorWidget::linkActivated,
            this, [this](const QString &path) {
        // Forward up — MainWindow will handle opening the note
        // Find parent EditorViewManager and emit from there
        if (auto *manager = qobject_cast<EditorViewManager *>(parentWidget())) {
            Q_EMIT manager->linkActivated(path);
        }
    });
```

Wait — `linkActivated` isn't a signal on EditorViewSpace. Let me simplify: just connect in MainWindow after `activeEditorChanged`.

In MainWindow's `onVaultOpened()`, replace the `activeEditorChanged` connection with:

```cpp
    // Set vault on editors and connect link navigation
    connect(m_editorManager, &EditorViewManager::activeEditorChanged,
            this, [this](NoteEditorWidget *editor) {
        if (!editor) return;
        if (m_vaultService->vault()) {
            editor->setVaultModel(m_vaultService->vault());
        }
        // Connect link navigation (disconnect previous to avoid duplicates)
        connect(editor, &NoteEditorWidget::linkActivated,
                this, &MainWindow::onNoteActivated, Qt::UniqueConnection);
    }, Qt::UniqueConnection);
```

This is cleaner — each time a new editor becomes active, we ensure it has the vault model and its `linkActivated` signal is connected.

- [ ] **Step 4: Build and verify**

Run:
```bash
cmake --build build && cd build && ctest --output-on-failure
```

Expected: All tests pass, builds cleanly.

- [ ] **Step 5: Commit**

```bash
git add src/editor/NoteEditorWidget.h src/editor/NoteEditorWidget.cpp \
        src/app/MainWindow.cpp
git commit -m "feat: add wikilink/tag autocomplete and Ctrl+Click navigation

Wikilink autocomplete: type [[ to trigger fuzzy note search popup.
Tag autocomplete: type # to trigger tag completion from vault.
Ctrl+Click: opens wikilink target note in new tab.
Ctrl+Hover: shows pointing hand cursor on wikilinks.
Uses shared CompletionPopup with KFuzzyMatcher."
```

---

Self-review:

1. **Spec coverage:** VaultModel::allTags() ✓. CompletionPopup reusable widget ✓. CompletionDelegate with match highlighting ✓. Wikilink autocomplete on `[[` ✓. Tag autocomplete on `#` ✓. Ctrl+Click navigation ✓. Ctrl+Hover cursor change ✓. `[[target|display]]` pipe handling ✓. `[[target#heading]]` hash handling ✓. Breadcrumb comments for future ✓. Dismissal on `]]`/space ✓. Code block tag exclusion ✓.

2. **Placeholder scan:** All code complete. No TBDs. Future items in breadcrumb comments.

3. **Type consistency:** `CompletionPopup::itemSelected(text, data)` matches `onCompletionAccepted(text, data)`. `QuickSwitcherModel` reused with `setNotes()`. `VaultModel::allTags()` returns `QStringList`. `linkActivated(QString)` signal matches `onNoteActivated(QString)` slot.
