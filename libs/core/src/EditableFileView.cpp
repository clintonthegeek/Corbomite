// libs/core/src/EditableFileView.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/EditableFileView.h"

#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/PathUtils.h"
#include "corbomite/core/Platform.h"

#include <KLocalizedString>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QIcon>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>

namespace Corbomite {

EditableFileView::EditableFileView(WorkspaceLeaf *leaf, QWidget *parent)
    : FileView(leaf, parent)
{
}

bool EditableFileView::setCursorLine(int /*line*/)
{
    return false;
}

void EditableFileView::setRenameCallback(NoteDocumentCallback cb)
{
    m_renameCallback = std::move(cb);
}

void EditableFileView::setMoveCallback(NoteDocumentCallback cb)
{
    m_moveCallback = std::move(cb);
}

void EditableFileView::setDeleteCallback(NoteDocumentCallback cb)
{
    m_deleteCallback = std::move(cb);
}

void EditableFileView::setVaultAbsolutePathResolver(
    std::function<QString(NoteDocument *)> resolver)
{
    m_absolutePathResolver = std::move(resolver);
}

void EditableFileView::setVaultNameResolver(std::function<QString()> resolver)
{
    m_vaultNameResolver = std::move(resolver);
}

void EditableFileView::setCommandDispatcher(CommandDispatch dispatcher)
{
    m_commandDispatcher = std::move(dispatcher);
}

void EditableFileView::onOpen()
{
    FileView::onOpen();
}

void EditableFileView::onPaneMenu(QMenu *menu)
{
    FileView::onPaneMenu(menu);
    // Keep the tab-header right-click Rename… item working against the
    // same codepath as the hamburger rename entry.
    if (m_file) {
        menu->addAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                        i18n("Rename..."),
                        this, &EditableFileView::startRename);
    }
}

void EditableFileView::onMoreOptionsMenu(MenuSectionHelper &helper)
{
    View::onMoreOptionsMenu(helper);

    // Everything below targets an editable file. Skip when the view has
    // no document (new-tab placeholder, for example).
    if (!m_file) return;

    // ---- pane: disabled placeholders ----
    auto *openInNewWindow = new QAction(
        QIcon::fromTheme(QStringLiteral("window-new")),
        i18n("Open in new window"), this);
    openInNewWindow->setEnabled(false);
    openInNewWindow->setToolTip(
        i18n("Requires WorkspaceWindow popout (Cluster G follow-up #6)"));
    helper.addToSection(openInNewWindow, QStringLiteral("pane"));

    auto *bookmarkAct = new QAction(
        QIcon::fromTheme(QStringLiteral("bookmark-new")),
        i18n("Bookmark"), this);
    bookmarkAct->setEnabled(false);
    bookmarkAct->setToolTip(
        i18n("Requires Cluster S: Bookmarks plugin"));
    helper.addToSection(bookmarkAct, QStringLiteral("pane"));

    // ---- action: Rename / Move / Version history (disabled) ----
    auto *renameAct = new QAction(
        QIcon::fromTheme(QStringLiteral("edit-rename")),
        i18n("Rename..."), this);
    connect(renameAct, &QAction::triggered, this, [this] {
        if (m_renameCallback && m_file) m_renameCallback(m_file, this);
    });
    helper.addToSection(renameAct, QStringLiteral("action"));

    auto *moveAct = new QAction(
        QIcon::fromTheme(QStringLiteral("folder-open")),
        i18n("Move file to..."), this);
    connect(moveAct, &QAction::triggered, this, [this] {
        if (m_moveCallback && m_file) m_moveCallback(m_file, this);
    });
    helper.addToSection(moveAct, QStringLiteral("action"));

    auto *versionAct = new QAction(
        QIcon::fromTheme(QStringLiteral("chronometer")),
        i18n("Version history"), this);
    versionAct->setEnabled(false);
    versionAct->setToolTip(
        i18n("Requires Cluster T: File Recovery plugin"));
    helper.addToSection(versionAct, QStringLiteral("action"));

    // ---- info.copy: "Copy path" submenu ----
    auto *copySub = helper.addSubmenu(
        QStringLiteral("info.copy"),
        i18n("Copy path"),
        QIcon::fromTheme(QStringLiteral("edit-copy")));

    auto *copyObsidian = new QAction(i18n("Copy Obsidian URL"), this);
    connect(copyObsidian, &QAction::triggered, this, [this] {
        if (!m_file) return;
        const QString vaultName = m_vaultNameResolver
            ? m_vaultNameResolver() : QString();
        const QString url = PathUtils::obsidianUrlFor(
            vaultName, m_file->relativePath());
        QApplication::clipboard()->setText(url);
    });
    copySub->addToSection(copyObsidian, QStringLiteral("action"));

    auto *copyCorbomite = new QAction(i18n("Copy Corbomite URL"), this);
    connect(copyCorbomite, &QAction::triggered, this, [this] {
        if (!m_file) return;
        const QString vaultName = m_vaultNameResolver
            ? m_vaultNameResolver() : QString();
        const QString url = PathUtils::corbomiteUrlFor(
            vaultName, m_file->relativePath());
        QApplication::clipboard()->setText(url);
    });
    copySub->addToSection(copyCorbomite, QStringLiteral("action"));

    auto *copyRel = new QAction(i18n("Copy vault-relative path"), this);
    connect(copyRel, &QAction::triggered, this, [this] {
        if (!m_file) return;
        QApplication::clipboard()->setText(m_file->relativePath());
    });
    copySub->addToSection(copyRel, QStringLiteral("action"));

    // ---- system: Open in default app / Show in folder / Reveal in nav ----
    auto *openDefault = new QAction(
        QIcon::fromTheme(QStringLiteral("document-open")),
        i18n("Open in default app"), this);
    connect(openDefault, &QAction::triggered, this, [this] {
        if (!m_file) return;
        QString abs;
        if (m_absolutePathResolver)
            abs = m_absolutePathResolver(m_file);
        if (abs.isEmpty())
            abs = m_file->filePath();  // best-effort fallback
        Platform::openWithDefaultApp(abs);
    });
    helper.addToSection(openDefault, QStringLiteral("system"));

    auto *showInFs = new QAction(
        QIcon::fromTheme(QStringLiteral("folder-open")),
        i18n("Show in folder"), this);
    connect(showInFs, &QAction::triggered, this, [this] {
        if (!m_file) return;
        QString abs;
        if (m_absolutePathResolver)
            abs = m_absolutePathResolver(m_file);
        if (abs.isEmpty())
            abs = m_file->filePath();
        Platform::showInFolder(abs);
    });
    helper.addToSection(showInFs, QStringLiteral("system"));

    auto *revealAct = new QAction(
        QIcon::fromTheme(QStringLiteral("mark-location")),
        i18n("Reveal file in navigation"), this);
    connect(revealAct, &QAction::triggered, this, [this] {
        if (m_commandDispatcher)
            m_commandDispatcher(QStringLiteral("file-explorer:reveal-file"));
    });
    helper.addToSection(revealAct, QStringLiteral("system"));

    // ---- danger: Delete ----
    auto *deleteAct = new QAction(
        QIcon::fromTheme(QStringLiteral("edit-delete")),
        i18n("Delete"), this);
    connect(deleteAct, &QAction::triggered, this, [this] {
        if (m_deleteCallback && m_file) m_deleteCallback(m_file, this);
    });
    helper.addToSection(deleteAct, QStringLiteral("danger"));
}

void EditableFileView::startRename()
{
    if (!m_file || m_renaming) return;
    m_renaming = true;
    m_originalName = m_file->name();
    if (m_renameCallback) m_renameCallback(m_file, this);
    m_renaming = false;
}

void EditableFileView::finishRename()
{
    m_renaming = false;
}

void EditableFileView::cancelRename()
{
    m_renaming = false;
}

} // namespace Corbomite
