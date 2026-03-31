// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutlinePanel.h"
#include "corbomite/core/NoteDocument.h"

#include <KLocalizedString>
#include <QVBoxLayout>
#include <QFont>
#include <QRegularExpression>

namespace Corbomite {

OutlinePanel::OutlinePanel(QWidget *parent)
    : QWidget(parent)
    , m_headerLabel(new QLabel(i18n("Outline"), this))
    , m_tree(new QTreeWidget(this))
    , m_emptyLabel(new QLabel(i18n("No headings"), this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    QFont headerFont = m_headerLabel->font();
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    layout->addWidget(m_headerLabel);

    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_tree);

    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: gray;"));
    layout->addWidget(m_emptyLabel);
    m_emptyLabel->setVisible(true);
    m_tree->setVisible(false);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(500);
    connect(&m_debounceTimer, &QTimer::timeout, this, &OutlinePanel::refresh);

    connect(m_tree, &QTreeWidget::itemClicked, this, &OutlinePanel::onItemClicked);
}

void OutlinePanel::setCurrentNote(NoteDocument *doc)
{
    // Disconnect from previous document
    if (m_currentDoc) {
        disconnect(m_currentDoc, &NoteDocument::textChanged, &m_debounceTimer, nullptr);
    }

    m_currentDoc = doc;

    if (m_currentDoc) {
        // Connect for live updates
        connect(m_currentDoc, &NoteDocument::textChanged,
                &m_debounceTimer, qOverload<>(&QTimer::start));
    }

    refresh();
}

void OutlinePanel::refresh()
{
    m_tree->clear();

    if (!m_currentDoc || m_currentDoc->markdown().isEmpty()) {
        m_headerLabel->setText(i18n("Outline"));
        m_emptyLabel->setVisible(true);
        m_tree->setVisible(false);
        return;
    }

    static const QRegularExpression headingPattern(QStringLiteral(R"(^(#{1,6})\s+(.+)$)"));

    const auto lines = m_currentDoc->markdown().split(QLatin1Char('\n'));

    // Stack to track parent items at each heading level
    // Index 0 = H1 parent, Index 1 = H2 parent, etc.
    QTreeWidgetItem *parents[6] = {nullptr};
    int headingCount = 0;

    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        auto match = headingPattern.match(lines[lineNum]);
        if (!match.hasMatch()) continue;

        int level = match.captured(1).length(); // 1-6
        QString text = match.captured(2).trimmed();

        auto *item = new QTreeWidgetItem();
        item->setText(0, text);
        item->setData(0, Qt::UserRole, lineNum + 1); // 1-based line number

        // Find parent: the most recent heading with a lower level
        QTreeWidgetItem *parent = nullptr;
        for (int i = level - 2; i >= 0; --i) {
            if (parents[i]) {
                parent = parents[i];
                break;
            }
        }

        if (parent) {
            parent->addChild(item);
        } else {
            m_tree->addTopLevelItem(item);
        }

        // Register this item as the parent for deeper levels
        parents[level - 1] = item;
        // Clear deeper level parents (a new H2 resets H3-H6 parents)
        for (int i = level; i < 6; ++i) {
            parents[i] = nullptr;
        }

        ++headingCount;
    }

    m_headerLabel->setText(i18n("Outline (%1)", headingCount));

    if (headingCount == 0) {
        m_emptyLabel->setVisible(true);
        m_tree->setVisible(false);
    } else {
        m_emptyLabel->setVisible(false);
        m_tree->setVisible(true);
        m_tree->expandAll();
    }
}

void OutlinePanel::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    int lineNumber = item->data(0, Qt::UserRole).toInt();
    if (lineNumber > 0) {
        Q_EMIT scrollToLine(lineNumber);
    }
}

} // namespace Corbomite
