// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "markoff/Editor.h"
#include "markoff/ReadingView.h"
#include "markoff/Document.h"
#include "markoff/RenderSettings.h"
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QSpinBox>
#include <QLabel>
#include <QScrollBar>
#include <QStatusBar>
#include <QRegularExpression>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1200, 700);

    // Central widget: horizontal splitter
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    m_editor = new Markoff::Editor(splitter);
    m_readingView = new Markoff::ReadingView(splitter);

    splitter->addWidget(m_editor);
    splitter->addWidget(m_readingView);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(splitter);

    // Toolbar
    auto *toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->setMovable(false);

    auto *openAction = new QAction(QStringLiteral("Open"), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpen);
    toolbar->addAction(openAction);

    auto *saveAction = new QAction(QStringLiteral("Save"), this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSave);
    toolbar->addAction(saveAction);

    toolbar->addSeparator();

    auto *fontLabel = new QLabel(QStringLiteral("Font size:"), toolbar);
    toolbar->addWidget(fontLabel);

    auto *fontSpin = new QSpinBox(toolbar);
    fontSpin->setRange(8, 32);
    fontSpin->setValue(14);
    toolbar->addWidget(fontSpin);

    toolbar->addSeparator();
    auto *modeAction = toolbar->addAction(tr("Live Preview"));
    modeAction->setCheckable(true);
    modeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(modeAction, &QAction::toggled, this, &MainWindow::onModeToggle);

    connect(fontSpin, &QSpinBox::valueChanged, this, [this](int size) {
        // Update reading view
        Markoff::RenderSettings settings;
        settings.baseFontSizePt = size;
        m_readingView->setSettings(settings);

        // Update editor (source mode + live preview rendered blocks)
        m_editor->setFontSize(size);

        // Re-render
        onTextChanged();
    });

    // Apply initial font size
    m_editor->setFontSize(14);

    // Connect editor text changes
    connect(m_editor, &Markoff::Editor::textChanged, this, &MainWindow::onTextChanged);

    // Status bar
    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);

    // Scroll sync: editor → reading view
    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        auto *sb = m_editor->verticalScrollBar();
        if (sb->maximum() > 0) {
            qreal fraction = static_cast<qreal>(sb->value()) / sb->maximum();
            m_readingView->setScrollFraction(fraction);
        }
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

    // Set basePath for image resolution
    Markoff::RenderSettings settings;
    settings.basePath = QFileInfo(path).absolutePath();
    m_readingView->setSettings(settings);

    updateTitle();
}

void MainWindow::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Markdown File"),
        QString(),
        QStringLiteral("Markdown Files (*.md *.markdown);;All Files (*)")
    );
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::onSave()
{
    if (m_filePath.isEmpty()) {
        m_filePath = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Save Markdown File"),
            QString(),
            QStringLiteral("Markdown Files (*.md *.markdown);;All Files (*)")
        );
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

void MainWindow::onTextChanged()
{
    auto doc = Markoff::Document::fromMarkdown(m_editor->toPlainText());
    m_readingView->setDocument(*doc);
    updateStatusBar();
}

void MainWindow::updateStatusBar()
{
    const QString text = m_editor->toPlainText();
    int lines = text.count(QLatin1Char('\n')) + (text.isEmpty() ? 0 : 1);
    int words = 0;
    if (!text.isEmpty()) {
        static const QRegularExpression wordRe(QStringLiteral("\\S+"));
        auto it = wordRe.globalMatch(text);
        while (it.hasNext()) { it.next(); ++words; }
    }
    int chars = text.size();
    m_statusLabel->setText(QStringLiteral("%1 lines, %2 words, %3 chars").arg(lines).arg(words).arg(chars));
}

void MainWindow::onModeToggle()
{
    if (m_editor->mode() == Markoff::Editor::Mode::Source)
        m_editor->setMode(Markoff::Editor::Mode::LivePreview);
    else
        m_editor->setMode(Markoff::Editor::Mode::Source);
}

void MainWindow::updateTitle()
{
    const QString name = m_filePath.isEmpty()
        ? QStringLiteral("[untitled]")
        : QFileInfo(m_filePath).fileName();
    setWindowTitle(QStringLiteral("Markoff \u2014 ") + name);
}
