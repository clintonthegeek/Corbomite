// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "markoff/Editor.h"
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QSpinBox>
#include <QLabel>
#include <QStatusBar>
#include <QRegularExpression>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(900, 700);

    m_editor = new Markoff::Editor(this);
    setCentralWidget(m_editor);

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
    fontSpin->setRange(6, 48);
    fontSpin->setValue(14);
    toolbar->addWidget(fontSpin);

    toolbar->addSeparator();
    auto *modeAction = toolbar->addAction(tr("Live Preview"));
    modeAction->setCheckable(true);
    modeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(modeAction, &QAction::toggled, this, &MainWindow::onModeToggle);

    connect(fontSpin, &QSpinBox::valueChanged, m_editor, &Markoff::Editor::setFontSize);

    m_editor->setFontSize(14);

    connect(m_editor, &Markoff::Editor::textChanged, this, &MainWindow::updateStatusBar);

    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);

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
}

void MainWindow::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open Markdown File"), QString(),
        QStringLiteral("Markdown Files (*.md *.markdown);;All Files (*)"));
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::onSave()
{
    if (m_filePath.isEmpty()) {
        m_filePath = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save Markdown File"), QString(),
            QStringLiteral("Markdown Files (*.md *.markdown);;All Files (*)"));
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

void MainWindow::onModeToggle()
{
    m_editor->setReadOnly(!m_editor->isReadOnly());
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
    m_statusLabel->setText(QStringLiteral("%1 lines, %2 words, %3 chars")
        .arg(lines).arg(words).arg(text.size()));
}

void MainWindow::updateTitle()
{
    const QString name = m_filePath.isEmpty()
        ? QStringLiteral("[untitled]")
        : QFileInfo(m_filePath).fileName();
    setWindowTitle(QStringLiteral("Markoff \u2014 ") + name);
}
