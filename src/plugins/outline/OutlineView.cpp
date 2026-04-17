// SPDX-License-Identifier: GPL-3.0-or-later
#include "OutlineView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/storage/proxies/MetadataCacheReader.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/proxies/VaultProxy.h"

#include <KLocalizedString>
#include <QFont>
#include <QLabel>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace Corbomite {

OutlineView::OutlineView(MetadataCacheReader *metadata,
                          VaultProxy *vault,
                          WorkspaceController *workspace,
                          QWidget *parent)
    : QWidget(parent)
    , m_metadata(metadata)
    , m_vaultProxy(vault)
    , m_workspace(workspace)
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
    connect(&m_debounceTimer, &QTimer::timeout, this, &OutlineView::refresh);
    connect(m_tree, &QTreeWidget::itemClicked, this, &OutlineView::onItemClicked);

    if (m_metadata) {
        // Cache change for the active file → re-derive outline.
        connect(m_metadata, &MetadataCacheReader::cacheChanged, this,
                [this](const QString &p) {
                    if (p == m_currentPath) scheduleRefresh();
                });
    }
    if (m_workspace) {
        connect(m_workspace, &WorkspaceController::activeFileChanged, this,
                &OutlineView::onActiveFileChanged);
        m_currentPath = m_workspace->activeFilePath();
    }
    refresh();
}

void OutlineView::onActiveFileChanged(const QString &path)
{
    if (m_currentPath == path) return;
    m_currentPath = path;
    refresh();
}

void OutlineView::scheduleRefresh()
{
    m_debounceTimer.start();
}

void OutlineView::refresh()
{
    m_tree->clear();
    m_headerLabel->setText(i18n("Outline"));

    if (!m_vaultProxy || m_currentPath.isEmpty()) {
        m_emptyLabel->setVisible(true);
        m_tree->setVisible(false);
        return;
    }
    auto *tf = m_vaultProxy->getFileByPath(m_currentPath);
    if (!tf) {
        m_emptyLabel->setVisible(true);
        m_tree->setVisible(false);
        return;
    }
    const QByteArray bytes = m_vaultProxy->cachedRead(tf);
    if (bytes.isEmpty()) {
        m_emptyLabel->setVisible(true);
        m_tree->setVisible(false);
        return;
    }
    const QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'));
    static const QRegularExpression heading(
        QStringLiteral(R"(^(#{1,6})\s+(.+)$)"));

    QTreeWidgetItem *parents[6] = {nullptr};
    int headingCount = 0;
    for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
        const auto match = heading.match(lines.at(lineNum));
        if (!match.hasMatch()) continue;
        const int level = match.captured(1).length();
        const QString text = match.captured(2).trimmed();

        auto *item = new QTreeWidgetItem();
        item->setText(0, text);
        item->setData(0, Qt::UserRole, lineNum + 1);

        QTreeWidgetItem *parent = nullptr;
        for (int i = level - 2; i >= 0; --i) {
            if (parents[i]) { parent = parents[i]; break; }
        }
        if (parent) parent->addChild(item);
        else m_tree->addTopLevelItem(item);

        parents[level - 1] = item;
        for (int i = level; i < 6; ++i) parents[i] = nullptr;
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

void OutlineView::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    Q_UNUSED(item);
    // Scroll-to-line in active editor is deferred — WorkspaceController
    // doesn't expose an editor handle yet. Follow-up: extend the
    // workspace proxy with goToLine(activeLeaf, line).
}

} // namespace Corbomite
