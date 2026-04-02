// SPDX-License-Identifier: GPL-3.0-or-later
#include "markoff/Editor.h"
#include "SelectionScene.h"
#include "SceneCoordinator.h"
#include "MarkdownTextItem.h"
#include "TextControl.h"

#include <QResizeEvent>
#include <QScrollBar>
#include <QTextDocument>

namespace Markoff {

Editor::Editor(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new SelectionScene(this))
    , m_coordinator(new SceneCoordinator(m_scene, this))
{
    setScene(m_scene);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setDragMode(QGraphicsView::NoDrag);
    setRenderHint(QPainter::Antialiasing);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);

    viewport()->setBackgroundRole(QPalette::Base);
    viewport()->setCursor(Qt::IBeamCursor);

    verticalScrollBar()->setSingleStep(20);

    connect(m_coordinator, &SceneCoordinator::textChanged,
            this, &Editor::textChanged);
}

Editor::~Editor() = default;

void Editor::setPlainText(const QString &text)
{
    m_sourceText = text;
    rebuildScene();
}

QString Editor::toPlainText() const
{
    if (m_mode == Mode::Source) {
        // In source mode there's a single text item
        if (!m_coordinator->items().isEmpty() && m_coordinator->items().first()->isTextItem()) {
            return m_coordinator->items().first()->allMarkdown();
        }
        return m_sourceText;
    }
    return m_coordinator->toMarkdown();
}

void Editor::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    // Serialize current state before switching
    m_sourceText = toPlainText();
    m_mode = mode;
    rebuildScene();
}

void Editor::setFontSize(int pointSize)
{
    m_fontSize = pointSize;
    QFont font = this->font();
    font.setPointSize(pointSize);
    m_coordinator->setFont(font);
}

void Editor::resizeEvent(QResizeEvent *e)
{
    QGraphicsView::resizeEvent(e);
    qreal width = viewport()->width() - 32; // 16px scene margin each side
    if (width > 100)
        m_coordinator->setItemWidth(width);
}

void Editor::rebuildScene()
{
    if (m_mode == Mode::Source)
        m_coordinator->loadSource(m_sourceText);
    else
        m_coordinator->loadMarkdown(m_sourceText);

    // Set width and font
    qreal width = viewport()->width() - 32; // 16px scene margin each side
    if (width > 100)
        m_coordinator->setItemWidth(width);
    if (m_fontSize > 0) {
        QFont font = this->font();
        font.setPointSize(m_fontSize);
        m_coordinator->setFont(font);
    }

    // Focus the first text item
    for (auto *item : m_coordinator->items()) {
        if (item->isTextItem()) {
            item->asGraphicsItem()->setFocus();
            break;
        }
    }
}

} // namespace Markoff
