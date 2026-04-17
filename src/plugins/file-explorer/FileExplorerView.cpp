// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileExplorerView.h"

#include "corbomite/core/proxies/WorkspaceController.h"
#include "corbomite/models/NotesTreeModel.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"
#include "corbomite/vault/proxies/FileManagerProxy.h"

#include <KLocalizedString>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>

namespace Corbomite {

FileExplorerView::FileExplorerView(Vault *vault,
                                    FileManagerProxy *fileManager,
                                    WorkspaceController *workspace,
                                    QWidget *parent)
    : QWidget(parent)
    , m_vault(vault)
    , m_fmProxy(fileManager)
    , m_workspace(workspace)
    , m_treeView(new QTreeView(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_treeView);

    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setAnimated(true);
    m_treeView->installEventFilter(this);

    if (m_vault) {
        m_model = new NotesTreeModel(m_vault, this);
        m_treeView->setModel(m_model);
    }

    connect(m_treeView, &QTreeView::doubleClicked, this,
            &FileExplorerView::onDoubleClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this,
            &FileExplorerView::showContextMenu);
}

FileExplorerView::~FileExplorerView() = default;

void FileExplorerView::onDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    if (index.data(NotesTreeModel::IsDirectoryRole).toBool()) return;
    onNoteActivated(index.data(NotesTreeModel::PathRole).toString());
}

void FileExplorerView::onNoteActivated(const QString &path)
{
    if (m_workspace && !path.isEmpty()) m_workspace->openFile(path);
}

void FileExplorerView::onNewNoteIn(const QString &folder)
{
    if (!m_fmProxy || !m_vault) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, i18n("New Note"),
        i18n("Note name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty()) return;
    TFolder *parent = nullptr;
    if (!folder.isEmpty()) parent = m_vault->getFolderByPath(folder);
    auto *tf = m_fmProxy->createNewMarkdownFile(parent, name);
    if (tf) onNoteActivated(tf->path);
}

void FileExplorerView::onDeleteNote(const QString &path)
{
    if (!m_fmProxy || !m_vault) return;
    if (QMessageBox::question(this, i18n("Delete Note"),
        i18n("Delete \"%1\"?", path)) != QMessageBox::Yes) return;
    auto *file = m_vault->getAbstractFileByPath(path);
    if (file) m_fmProxy->trashFile(file);
}

void FileExplorerView::onRenameNote(const QString &path)
{
    if (!m_fmProxy || !m_vault) return;
    QString oldName = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
    if (oldName.endsWith(QStringLiteral(".md"))) oldName.chop(3);
    bool ok = false;
    const QString newName = QInputDialog::getText(this, i18n("Rename Note"),
        i18n("New name:"), QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.isEmpty() || newName == oldName) return;
    QString folder;
    const int lastSlash = path.lastIndexOf(QLatin1Char('/'));
    if (lastSlash > 0) folder = path.left(lastSlash);
    const QString newPath = folder.isEmpty()
        ? newName + QStringLiteral(".md")
        : folder + QLatin1Char('/') + newName + QStringLiteral(".md");
    auto *file = m_vault->getAbstractFileByPath(path);
    if (file) m_fmProxy->renameFile(file, newPath);
}

void FileExplorerView::showContextMenu(const QPoint &pos)
{
    auto index = m_treeView->indexAt(pos);
    QMenu menu(this);
    QString contextPath;

    if (index.isValid()) {
        const bool isDir = index.data(NotesTreeModel::IsDirectoryRole).toBool();
        contextPath = index.data(NotesTreeModel::PathRole).toString();
        if (isDir) {
            auto *newNote = menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")),
                                           i18n("New Note Here"));
            connect(newNote, &QAction::triggered, this,
                    [this, p = contextPath]() { onNewNoteIn(p); });
        } else {
            auto *open = menu.addAction(i18n("Open"));
            connect(open, &QAction::triggered, this,
                    [this, p = contextPath]() { onNoteActivated(p); });
            auto *rename = menu.addAction(i18n("Rename"));
            connect(rename, &QAction::triggered, this,
                    [this, p = contextPath]() { onRenameNote(p); });
            auto *del = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                                       i18n("Delete"));
            connect(del, &QAction::triggered, this,
                    [this, p = contextPath]() { onDeleteNote(p); });
        }
    } else {
        auto *newNote = menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")),
                                       i18n("New Note"));
        connect(newNote, &QAction::triggered, this,
                [this]() { onNewNoteIn(QString()); });
    }
    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

bool FileExplorerView::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_treeView && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const auto idx = m_treeView->currentIndex();
        if (idx.isValid() && !idx.data(NotesTreeModel::IsDirectoryRole).toBool()) {
            const QString path = idx.data(NotesTreeModel::PathRole).toString();
            if (keyEvent->key() == Qt::Key_F2) { onRenameNote(path); return true; }
            if (keyEvent->key() == Qt::Key_Delete) { onDeleteNote(path); return true; }
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace Corbomite
