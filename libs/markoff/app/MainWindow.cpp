// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "markoff/Editor.h"
#include "markoff/Theme.h"
#include <markoff-parser/Document.h>

#include <QAction>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStatusBar>
#include <QTextStream>
#include <QToolBar>
#include <QTreeWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1000, 750);

    m_editor = new Markoff::Editor(this);
    setCentralWidget(m_editor);

    // --- Menu bar ---
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open..."), QKeySequence::Open, this, &MainWindow::onOpen);
    fileMenu->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::onSave);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Undo"), QKeySequence::Undo, m_editor, &Markoff::Editor::undo);
    editMenu->addAction(tr("&Redo"), QKeySequence::Redo, m_editor, &Markoff::Editor::redo);
    editMenu->addSeparator();
    editMenu->addAction(tr("Cu&t"), QKeySequence::Cut, m_editor, &Markoff::Editor::cut);
    editMenu->addAction(tr("&Copy"), QKeySequence::Copy, m_editor, &Markoff::Editor::copy);
    editMenu->addAction(tr("&Paste"), QKeySequence::Paste, m_editor, &Markoff::Editor::paste);
    editMenu->addAction(tr("Select &All"), QKeySequence::SelectAll, m_editor, &Markoff::Editor::selectAll);

    auto *formatMenu = menuBar()->addMenu(tr("F&ormat"));
    formatMenu->addAction(tr("&Bold"), QKeySequence::Bold, m_editor, &Markoff::Editor::toggleBold);
    formatMenu->addAction(tr("&Italic"), QKeySequence::Italic, m_editor, &Markoff::Editor::toggleItalic);
    formatMenu->addAction(tr("&Strikethrough"), m_editor, &Markoff::Editor::toggleStrikethrough);
    formatMenu->addAction(tr("Inline &Code"), m_editor, &Markoff::Editor::toggleInlineCode);
    formatMenu->addSeparator();
    formatMenu->addAction(tr("Increase &Heading"), m_editor, &Markoff::Editor::increaseHeadingLevel);
    formatMenu->addAction(tr("&Decrease Heading"), m_editor, &Markoff::Editor::decreaseHeadingLevel);
    formatMenu->addSeparator();
    formatMenu->addAction(tr("Insert &Link"), m_editor, &Markoff::Editor::insertLink);
    formatMenu->addAction(tr("Insert &Wiki Link"), m_editor, &Markoff::Editor::insertWikiLink);
    formatMenu->addAction(tr("Insert I&mage"), m_editor, &Markoff::Editor::insertImage);
    formatMenu->addSeparator();
    formatMenu->addAction(tr("Code Bloc&k"), m_editor, &Markoff::Editor::insertCodeBlock);
    formatMenu->addAction(tr("Block &Quote"), m_editor, &Markoff::Editor::insertBlockQuote);
    formatMenu->addAction(tr("Horizontal &Rule"), m_editor, &Markoff::Editor::insertHorizontalRule);
    formatMenu->addAction(tr("&Toggle Checkbox"), m_editor, &Markoff::Editor::toggleCheckbox);
    formatMenu->addSeparator();
    formatMenu->addAction(tr("Insert &Table (3x3)"), m_editor, [this]() {
        m_editor->insertTable(3, 3);
    });

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    m_readOnlyAction = viewMenu->addAction(tr("&Read Only"));
    m_readOnlyAction->setCheckable(true);
    m_readOnlyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(m_readOnlyAction, &QAction::toggled, this, &MainWindow::onToggleReadOnly);

    m_themeAction = viewMenu->addAction(tr("&Dark Theme"));
    m_themeAction->setCheckable(true);
    m_themeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(m_themeAction, &QAction::toggled, this, &MainWindow::onToggleTheme);

    viewMenu->addSeparator();
    m_sidebarAction = viewMenu->addAction(tr("&Document Info"));
    m_sidebarAction->setCheckable(true);
    m_sidebarAction->setChecked(true);
    m_sidebarAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(m_sidebarAction, &QAction::toggled, this, &MainWindow::onToggleSidebar);

    // --- Toolbar ---
    auto *toolbar = addToolBar(tr("Formatting"));
    toolbar->setMovable(false);

    toolbar->addAction(tr("B"), m_editor, &Markoff::Editor::toggleBold);
    toolbar->addAction(tr("I"), m_editor, &Markoff::Editor::toggleItalic);
    toolbar->addAction(tr("S"), m_editor, &Markoff::Editor::toggleStrikethrough);
    toolbar->addAction(tr("`"), m_editor, &Markoff::Editor::toggleInlineCode);
    toolbar->addSeparator();
    toolbar->addAction(tr("H+"), m_editor, &Markoff::Editor::increaseHeadingLevel);
    toolbar->addAction(tr("H-"), m_editor, &Markoff::Editor::decreaseHeadingLevel);
    toolbar->addSeparator();

    auto *fontLabel = new QLabel(tr(" Font: "), toolbar);
    toolbar->addWidget(fontLabel);
    auto *fontSpin = new QSpinBox(toolbar);
    fontSpin->setRange(6, 48);
    fontSpin->setValue(14);
    toolbar->addWidget(fontSpin);
    connect(fontSpin, &QSpinBox::valueChanged, m_editor, &Markoff::Editor::setFontSize);

    // --- Sidebar: document metadata ---
    m_metadataTree = new QTreeWidget;
    m_metadataTree->setHeaderHidden(true);
    m_metadataTree->setRootIsDecorated(true);
    m_metadataTree->setMinimumWidth(200);

    m_sidebarDock = new QDockWidget(tr("Document Info"), this);
    m_sidebarDock->setWidget(m_metadataTree);
    m_sidebarDock->setFeatures(QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::RightDockWidgetArea, m_sidebarDock);

    connect(m_sidebarDock, &QDockWidget::visibilityChanged,
            m_sidebarAction, &QAction::setChecked);

    // --- Status bar ---
    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);

    // --- Signals ---
    m_editor->setFontSize(14);

    connect(m_editor, &Markoff::Editor::textChanged, this, &MainWindow::updateStatusBar);
    connect(m_editor, &Markoff::Editor::cursorPositionChanged,
            this, [this](int line, int col) {
        Q_UNUSED(line) Q_UNUSED(col)
        updateStatusBar();
    });
    connect(m_editor, &Markoff::Editor::headingsChanged, this, [this]() { updateMetadata(); });
    connect(m_editor, &Markoff::Editor::linksChanged, this, [this]() { updateMetadata(); });
    connect(m_editor, &Markoff::Editor::tagsChanged, this, [this]() { updateMetadata(); });
    connect(m_editor, &Markoff::Editor::wordCountChanged, this, [this]() { updateStatusBar(); });

    connect(m_editor, &Markoff::Editor::linkClicked, this, [](const QString &target) {
        qDebug("Link clicked: %s", qPrintable(target));
    });

    updateTitle();
}

MainWindow::~MainWindow() = default;

void MainWindow::openFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream stream(&file);
    m_editor->setPlainText(stream.readAll());
    m_filePath = path;
    updateTitle();
    updateMetadata();
}

void MainWindow::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Markdown File"), QString(),
        tr("Markdown Files (*.md *.markdown);;All Files (*)"));
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::onSave()
{
    if (m_filePath.isEmpty()) {
        m_filePath = QFileDialog::getSaveFileName(
            this, tr("Save Markdown File"), QString(),
            tr("Markdown Files (*.md *.markdown);;All Files (*)"));
        if (m_filePath.isEmpty())
            return;
    }
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream stream(&file);
    stream << m_editor->toPlainText();
    updateTitle();
}

void MainWindow::onToggleReadOnly()
{
    m_editor->setReadOnly(m_readOnlyAction->isChecked());
}

void MainWindow::onToggleTheme()
{
    m_darkTheme = m_themeAction->isChecked();
    m_editor->setTheme(m_darkTheme ? Markoff::Theme::defaultDark()
                                   : Markoff::Theme::defaultLight());
}

void MainWindow::onToggleSidebar()
{
    m_sidebarDock->setVisible(m_sidebarAction->isChecked());
}

void MainWindow::updateStatusBar()
{
    int line = m_editor->cursorLine();
    int col = m_editor->cursorColumn();

    const auto *doc = m_editor->document();
    int words = doc ? doc->wordCount() : 0;

    const QString text = m_editor->toPlainText();
    int lines = text.count(QLatin1Char('\n')) + (text.isEmpty() ? 0 : 1);

    QString status = QStringLiteral("Ln %1, Col %2  |  %3 lines, %4 words, %5 chars")
        .arg(line).arg(col).arg(lines).arg(words).arg(text.size());

    if (m_editor->isReadOnly())
        status += QStringLiteral("  |  READ ONLY");

    m_statusLabel->setText(status);
}

void MainWindow::updateMetadata()
{
    m_metadataTree->clear();
    const auto *doc = m_editor->document();
    if (!doc) return;

    // Headings
    auto headings = doc->headings();
    if (!headings.isEmpty()) {
        auto *headingRoot = new QTreeWidgetItem(m_metadataTree,
            {QStringLiteral("Headings (%1)").arg(headings.size())});
        for (const auto &h : headings) {
            QString prefix = QString(h.level, QLatin1Char('#'));
            new QTreeWidgetItem(headingRoot,
                {QStringLiteral("%1 %2").arg(prefix, h.text)});
        }
        headingRoot->setExpanded(true);
    }

    // Links
    auto links = doc->links();
    if (!links.isEmpty()) {
        auto *linkRoot = new QTreeWidgetItem(m_metadataTree,
            {QStringLiteral("Links (%1)").arg(links.size())});
        for (const auto &l : links) {
            QString typeStr;
            switch (l.type) {
            case Markoff::LinkInfo::Standard: typeStr = QStringLiteral("link"); break;
            case Markoff::LinkInfo::Wiki: typeStr = QStringLiteral("wiki"); break;
            case Markoff::LinkInfo::Image: typeStr = QStringLiteral("image"); break;
            case Markoff::LinkInfo::Embed: typeStr = QStringLiteral("embed"); break;
            }
            new QTreeWidgetItem(linkRoot,
                {QStringLiteral("[%1] %2").arg(typeStr, l.target)});
        }
        linkRoot->setExpanded(true);
    }

    // Tags
    auto tags = doc->tags();
    if (!tags.isEmpty()) {
        auto *tagRoot = new QTreeWidgetItem(m_metadataTree,
            {QStringLiteral("Tags (%1)").arg(tags.size())});
        for (const auto &t : tags) {
            new QTreeWidgetItem(tagRoot,
                {QStringLiteral("#%1").arg(t.name)});
        }
        tagRoot->setExpanded(true);
    }

    // Footnotes
    auto footnotes = doc->footnotes();
    if (!footnotes.isEmpty()) {
        auto *fnRoot = new QTreeWidgetItem(m_metadataTree,
            {QStringLiteral("Footnotes (%1)").arg(footnotes.size())});
        for (const auto &fn : footnotes) {
            new QTreeWidgetItem(fnRoot,
                {QStringLiteral("[^%1] %2").arg(fn.label, fn.content)});
        }
        fnRoot->setExpanded(true);
    }

    // Frontmatter
    QString fm = doc->frontmatter();
    if (!fm.isEmpty()) {
        auto *fmRoot = new QTreeWidgetItem(m_metadataTree,
            {QStringLiteral("Frontmatter")});
        for (const auto &line : fm.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            new QTreeWidgetItem(fmRoot, {line.trimmed()});
        }
        fmRoot->setExpanded(true);
    }
}

void MainWindow::updateTitle()
{
    const QString name = m_filePath.isEmpty()
        ? QStringLiteral("[untitled]")
        : QFileInfo(m_filePath).fileName();
    setWindowTitle(QStringLiteral("Markoff \u2014 ") + name);
}
