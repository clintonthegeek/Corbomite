// src/editor/MarkdownFileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MarkdownFileView.h"
#include "NoteEditorWidget.h"
#include <markoff/core/MarkdownView.h>
#include <markoff/source/Editor.h>
#include "corbomite/core/EditableFileView.h"
#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/WorkspaceLeaf.h"
// Port history: Reading::ReadingView retired
// Port history: old Markoff::Editor retired
// include <markoff/Editor.h>

#include <KLocalizedString>

#include <QAction>
#include <QIcon>
#include <QList>
#include <QString>
#include <QVBoxLayout>

namespace Corbomite {

MarkdownFileView::MarkdownFileView(WorkspaceLeaf *leaf, QWidget *parent)
    : TextFileView(leaf, parent)
    , m_editorWidget(new NoteEditorWidget(contentWidget()))
{
    auto *layout = new QVBoxLayout(contentWidget());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_editorWidget);

    // Forward the editor's own context signals
    // onto the generic View::contextChanged() so ActionContextController
    // can listen the same way it listens to every other view type,
    // instead of special-casing markdown's NoteEditorWidget signals.
    connect(m_editorWidget, &NoteEditorWidget::editorContextChanged,
            this, &View::contextChanged);
    connect(m_editorWidget, &NoteEditorWidget::viewModeChanged,
            this, &View::contextChanged);
}

View *MarkdownFileView::factory(WorkspaceLeaf *leaf)
{
    return new MarkdownFileView(leaf);
}

QString MarkdownFileView::getViewType() const
{
    return QStringLiteral("markdown");
}

QString MarkdownFileView::getDisplayText() const
{
    if (m_file)
        return m_file->name();
    return QStringLiteral("Markdown");
}

QString MarkdownFileView::getIcon() const
{
    return QStringLiteral("text-markdown");
}

QString MarkdownFileView::getViewData() const
{
    if (!m_editorWidget || !m_editorWidget->noteDocument())
        return {};
    return m_editorWidget->noteDocument()->markdown();
}

void MarkdownFileView::setViewData(const QString &data, bool clear)
{
    if (!m_editorWidget || !m_editorWidget->noteDocument())
        return;
    m_editorWidget->noteDocument()->setMarkdown(data);
    Q_UNUSED(clear)
}

void MarkdownFileView::clear()
{
    if (m_editorWidget && m_editorWidget->noteDocument())
        m_editorWidget->noteDocument()->setMarkdown(QString());
}

bool MarkdownFileView::canAcceptExtension(const QString &ext) const
{
    return ext.compare(QStringLiteral("md"), Qt::CaseInsensitive) == 0;
}

bool MarkdownFileView::setCursorLine(int line)
{
    if (!m_editorWidget) return false;
    return m_editorWidget->goToLine(line);
}

namespace {
// Zoom step lives here rather than in MainWindow.cpp's onZoomIn/Out/Reset,
// which used to bypass the View::zoomIn/Out/Reset virtuals entirely and
// apply this directly to activeEditor()->activeLeaf(). Matches the live
// leaf's own kFontScaleStep so menu zoom and the editor's Ctrl+=/Ctrl+-
// shortcuts step identically.
constexpr qreal kZoomStep = 1.10;
}

void MarkdownFileView::zoomIn()
{
    if (auto *leaf = m_editorWidget->activeLeaf())
        leaf->setFontScale(leaf->fontScale() * kZoomStep);   // base clamps to [0.25, 4.0]
}

void MarkdownFileView::zoomOut()
{
    if (auto *leaf = m_editorWidget->activeLeaf())
        leaf->setFontScale(leaf->fontScale() / kZoomStep);
}

void MarkdownFileView::zoomReset()
{
    if (auto *leaf = m_editorWidget->activeLeaf())
        leaf->setFontScale(1.0);
}

bool MarkdownFileView::canEdit() const
{
    auto *leaf = m_editorWidget->activeLeaf();
    return leaf && leaf->hasEditing();
}

bool MarkdownFileView::canSave() const { return true; }
bool MarkdownFileView::canFind() const { return true; }

bool MarkdownFileView::canUndo() const
{
    // Markoff::MarkdownView's contract exposes undo()/redo() actions but
    // no undo-stack-depth query — hasEditing() (editable at all) is the
    // best available proxy until an upstream API addition. Same
    // approximation O1.T8 shipped, now routed through the virtual.
    return canEdit();
}
bool MarkdownFileView::canRedo() const { return canEdit(); }

QJsonObject MarkdownFileView::getState() const
{
    QJsonObject state = FileView::getState();
    auto mode = m_editorWidget->viewMode();
    if (mode == NoteEditorWidget::ViewMode::Reading) {
        state[QStringLiteral("mode")] = QStringLiteral("preview");
    } else {
        state[QStringLiteral("mode")] = QStringLiteral("source");
        state[QStringLiteral("source")] = (mode == NoteEditorWidget::ViewMode::Source);
    }
    return state;
}

void MarkdownFileView::setState(const QJsonObject &state)
{
    FileView::setState(state);
    QString mode = state[QStringLiteral("mode")].toString();
    if (mode == QStringLiteral("preview")) {
        m_editorWidget->setViewMode(NoteEditorWidget::ViewMode::Reading);
    } else if (mode == QStringLiteral("source")) {
        bool source = state[QStringLiteral("source")].toBool(false);
        m_editorWidget->setViewMode(source ? NoteEditorWidget::ViewMode::Source
                                           : NoteEditorWidget::ViewMode::LivePreview);
    }
}

QJsonObject MarkdownFileView::getEphemeralState() const
{
    // Delegate to the EphemeralState serialization
    return {};
}

void MarkdownFileView::setEphemeralState(const QJsonObject &state)
{
    Q_UNUSED(state)
}

NoteEditorWidget *MarkdownFileView::editorWidget() const { return m_editorWidget; }

int findClosingFrontmatterFence(const QString &body)
{
    for (int i = 3; i + 3 <= body.size(); ++i) {
        if (body[i] != QLatin1Char('\n'))
            continue;
        if (body.mid(i + 1, 3) != QLatin1String("---"))
            continue;
        const int after = i + 4;
        if (after >= body.size())
            return i;                                  // fence at EOF
        const QChar next = body.at(after);
        if (next == QLatin1Char('\n') || next == QLatin1Char('\r'))
            return i;                                  // fence on its own line
        // Longer dash run, or a line prefixed with dashes: not a fence.
    }
    return -1;
}

void MarkdownFileView::insertFrontmatterProperty()
{
    if (!m_editorWidget) return;

    // If currently in Reading, flip to LivePreview so the new row is visible.
    if (m_editorWidget->viewMode() == NoteEditorWidget::ViewMode::Reading)
        m_editorWidget->setViewMode(NoteEditorWidget::ViewMode::LivePreview);

    auto *doc = m_editorWidget->noteDocument();
    if (!doc) return;

    QString body = doc->markdown();

    // Obsidian's convention: a frontmatter block opens with `---` on the
    // very first line and closes with `---` on its own line. If the opening
    // fence is missing, prepend a minimal 3-line block with one empty row.
    if (!body.startsWith(QStringLiteral("---\n"))
        && !body.startsWith(QStringLiteral("---\r\n"))
        && body != QStringLiteral("---")) {
        // Prepend a new block containing one blank key.
        const QString fm = QStringLiteral("---\n: \n---\n");
        doc->setMarkdown(fm + body);
        return;
    }

    // Locate the closing fence and append a blank key before it.
    const int closeIdx = findClosingFrontmatterFence(body);
    if (closeIdx < 0) {
        // Opening fence present but no closing fence — treat as malformed;
        // append a fresh block at the top.
        const QString fm = QStringLiteral("---\n: \n---\n");
        doc->setMarkdown(fm + body);
        return;
    }

    // Insert a blank property row directly before the closing `---`.
    // Preserve a trailing newline if the closing fence is the last line.
    const QString insert = QStringLiteral(": \n");
    QString out = body;
    out.insert(closeIdx + 1, insert);  // +1 skips the leading '\n'
    doc->setMarkdown(out);
}

void MarkdownFileView::setMarkdownCommandDispatcher(CommandDispatch dispatcher)
{
    m_markdownCommandDispatcher = std::move(dispatcher);
}

void MarkdownFileView::setPdfExportTrigger(PdfExportTrigger trigger)
{
    m_pdfExportTrigger = std::move(trigger);
}

void MarkdownFileView::setFindTrigger(FindTrigger trigger)
{
    m_findTrigger = std::move(trigger);
}

void MarkdownFileView::setReplaceTrigger(FindTrigger trigger)
{
    m_replaceTrigger = std::move(trigger);
}

void MarkdownFileView::setMergeFileTrigger(MergeTrigger trigger)
{
    m_mergeFileTrigger = std::move(trigger);
}

void MarkdownFileView::onMoreOptionsMenu(MenuSectionHelper &helper)
{
    // ---- pane: Split right / Split down ----
    auto dispatch = [this](const QString &cmd) {
        if (m_markdownCommandDispatcher) m_markdownCommandDispatcher(cmd);
    };

    auto *splitR = new QAction(
        QIcon::fromTheme(QStringLiteral("view-split-left-right")),
        i18n("Split right"), this);
    connect(splitR, &QAction::triggered, this,
            [dispatch] { dispatch(QStringLiteral("split_right")); });
    helper.addToSection(splitR, QStringLiteral("pane"));

    auto *splitD = new QAction(
        QIcon::fromTheme(QStringLiteral("view-split-top-bottom")),
        i18n("Split down"), this);
    connect(splitD, &QAction::triggered, this,
            [dispatch] { dispatch(QStringLiteral("split_down")); });
    helper.addToSection(splitD, QStringLiteral("pane"));

    // ---- view: Backlinks-in-document toggle ----
    auto *backlinksInDoc = new QAction(i18n("Backlinks in document"), this);
    backlinksInDoc->setCheckable(true);
    if (m_leaf) {
        const QJsonObject vs = m_leaf->getViewState();
        const QJsonObject st = vs.value(QStringLiteral("state")).toObject();
        backlinksInDoc->setChecked(
            st.value(QStringLiteral("backlinksInDocument")).toBool());
    }
    connect(backlinksInDoc, &QAction::triggered, this, [this](bool on) {
        if (!m_leaf) return;
        QJsonObject vs = m_leaf->getViewState();
        QJsonObject st = vs.value(QStringLiteral("state")).toObject();
        st[QStringLiteral("backlinksInDocument")] = on;
        vs[QStringLiteral("state")] = st;
        m_leaf->setViewState(vs);
        // The flag is kept in the view state, but nothing reads it: the
        // backlinks section used to be appended by BacklinksPostProcessor,
        // which went with the retired ReadingView. The reading leaf is the
        // styled leaf now, and it does not read this key.
    });
    helper.addToSection(backlinksInDoc, QStringLiteral("view"));

    // ---- view: Reading / Source toggles ----
    auto *readingAct = new QAction(i18n("Reading view"), this);
    readingAct->setCheckable(true);
    readingAct->setChecked(
        m_editorWidget
        && m_editorWidget->viewMode() == NoteEditorWidget::ViewMode::Reading);
    connect(readingAct, &QAction::triggered, this, [this] {
        if (!m_editorWidget) return;
        const bool toReading =
            m_editorWidget->viewMode() != NoteEditorWidget::ViewMode::Reading;
        m_editorWidget->setViewMode(toReading
            ? NoteEditorWidget::ViewMode::Reading
            : NoteEditorWidget::ViewMode::LivePreview);
    });
    helper.addToSection(readingAct, QStringLiteral("view"));

    auto *sourceAct = new QAction(i18n("Source mode"), this);
    sourceAct->setCheckable(true);
    sourceAct->setChecked(
        m_editorWidget
        && m_editorWidget->viewMode() == NoteEditorWidget::ViewMode::Source);
    connect(sourceAct, &QAction::triggered, this, [this] {
        if (!m_editorWidget) return;
        const bool toSource =
            m_editorWidget->viewMode() != NoteEditorWidget::ViewMode::Source;
        m_editorWidget->setViewMode(toSource
            ? NoteEditorWidget::ViewMode::Source
            : NoteEditorWidget::ViewMode::LivePreview);
    });
    helper.addToSection(sourceAct, QStringLiteral("view"));

    // ---- action: Bookmark (placeholder — the bookmarks panel exists, but the
    // editor's "add a bookmark here" action is not wired to it yet) ----
    auto *bookmarkAct = new QAction(
        QIcon::fromTheme(QStringLiteral("bookmark-new")),
        i18n("Bookmark..."), this);
    bookmarkAct->setEnabled(false);
    bookmarkAct->setToolTip(
        i18n("Adding a bookmark is not available yet"));
    helper.addToSection(bookmarkAct, QStringLiteral("action"));

    // ---- action: Add file property ----
    auto *addPropAct = new QAction(
        QIcon::fromTheme(QStringLiteral("list-add")),
        i18n("Add file property"), this);
    connect(addPropAct, &QAction::triggered, this,
            [dispatch] {
                dispatch(QStringLiteral("markdown:add-metadata-property"));
            });
    helper.addToSection(addPropAct, QStringLiteral("action"));

    // ---- action: Export to PDF ----
    auto *exportPdfAct = new QAction(
        QIcon::fromTheme(QStringLiteral("document-export")),
        i18n("Export to PDF..."), this);
    connect(exportPdfAct, &QAction::triggered, this, [this] {
        if (m_pdfExportTrigger) m_pdfExportTrigger(this);
    });
    helper.addToSection(exportPdfAct, QStringLiteral("action"));

    auto *findAct = new QAction(
        QIcon::fromTheme(QStringLiteral("edit-find")),
        i18n("Find..."), this);
    connect(findAct, &QAction::triggered, this, [this] {
        if (m_findTrigger) m_findTrigger(this);
    });
    helper.addToSection(findAct, QStringLiteral("find"));

    auto *replaceAct = new QAction(
        QIcon::fromTheme(QStringLiteral("edit-find-replace")),
        i18n("Replace..."), this);
    connect(replaceAct, &QAction::triggered, this, [this] {
        if (m_replaceTrigger) m_replaceTrigger(this);
    });
    helper.addToSection(replaceAct, QStringLiteral("find"));

    // ---- view.linked submenu: open the five sidebar dock panels ----
    auto *linkedSub = helper.addSubmenu(
        QStringLiteral("view.linked"),
        i18n("Open linked view"),
        QIcon::fromTheme(QStringLiteral("tab-detach")));

    struct LinkedEntry {
        QString label;
        QString commandId;
        QString icon;
    };
    const QList<LinkedEntry> entries = {
        // Corbomite-native ids, which are now *aliases* of the Obsidian ids
        // these commands are canonically registered under
        // (`backlink:open`, `outgoing-links:open`, …) — see
        // docs/obsidian-command-ids.md. Dispatch works under either name.
        {i18n("Open local graph"),
            QStringLiteral("corbomite-local-graph:open-local"),
            QStringLiteral("preferences-system-network")},
        {i18n("Open backlinks"),
            QStringLiteral("corbomite-backlinks:open"),
            QStringLiteral("go-previous")},
        {i18n("Open outgoing links"),
            QStringLiteral("corbomite-outlinks:open"),
            QStringLiteral("go-next")},
        {i18n("Open file properties"),
            QStringLiteral("corbomite-properties:open"),
            QStringLiteral("document-properties")},
        {i18n("Open outline"),
            QStringLiteral("corbomite-outline:open"),
            QStringLiteral("view-list-tree")},
    };
    for (const auto &e : entries) {
        auto *act = new QAction(QIcon::fromTheme(e.icon), e.label, this);
        const QString cmdId = e.commandId;
        connect(act, &QAction::triggered, this,
                [dispatch, cmdId] { dispatch(cmdId); });
        linkedSub->addToSection(act, QStringLiteral("action"));
    }

    // ---- action: Merge entire file with… ----
    auto *mergeAct = new QAction(
        QIcon::fromTheme(QStringLiteral("document-merge")),
        i18n("Merge entire file with…"), this);
    if (m_mergeFileTrigger) {
        connect(mergeAct, &QAction::triggered, this, [this] {
            if (m_mergeFileTrigger) m_mergeFileTrigger(this);
        });
    } else {
        mergeAct->setEnabled(false);
    }
    helper.addToSection(mergeAct, QStringLiteral("action"));

    // ---- Chain to EditableFileView for universal file-menu items ----
    // The chain currently resolves to View::onMoreOptionsMenu (empty body);
    // EditableFileView adds rename/move/delete
    // + copy-path submenu + open-in-default-app + reveal-in-navigation +
    // danger-zone Delete. MarkdownFileView's items appear BEFORE EditableFileView's
    // within each canonical section.
    EditableFileView::onMoreOptionsMenu(helper);
}

void MarkdownFileView::setVault(Vault *vault)
{
    m_editorWidget->setVault(vault);
}

void MarkdownFileView::setHoverPopover(HoverPopover *popover)
{
    m_editorWidget->setHoverPopover(popover);
}

void MarkdownFileView::setEditorSuggestManager(EditorSuggestManager *manager)
{
    m_editorWidget->setEditorSuggestManager(manager);
}

void MarkdownFileView::onOpen()
{
    TextFileView::onOpen();
}

void MarkdownFileView::onClose()
{
    TextFileView::onClose();
}

void MarkdownFileView::onLoadFile(NoteDocument *file)
{
    m_editorWidget->setNoteDocument(file);
    TextFileView::onLoadFile(file);
}

} // namespace Corbomite
