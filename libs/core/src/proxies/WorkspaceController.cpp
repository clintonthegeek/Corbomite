// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/WorkspaceController.h"

#include "corbomite/core/FileView.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceTabs.h"

#include <QFileInfo>
#include <QJsonObject>

namespace Corbomite {

namespace {
QString fileForLeaf(WorkspaceLeaf *leaf)
{
    if (!leaf) return {};
    if (auto *fv = qobject_cast<FileView *>(leaf->view())) {
        if (fv->file()) return fv->file()->relativePath();
    }
    const QJsonObject vs = leaf->getViewState();
    return vs.value(QStringLiteral("state")).toObject()
        .value(QStringLiteral("file")).toString();
}
} // namespace

WorkspaceController::WorkspaceController(Workspace *workspace, QObject *parent)
    : QObject(parent), m_workspace(workspace)
{
    if (!m_workspace) return;
    connect(m_workspace, &Workspace::activeLeafChanged, this,
            [this](WorkspaceLeaf *leaf) {
                Q_EMIT activeFileChanged(fileForLeaf(leaf));
            });
}

WorkspaceController::~WorkspaceController() = default;

QString WorkspaceController::activeFilePath() const
{
    if (!m_workspace) return {};
    return fileForLeaf(m_workspace->activeLeaf());
}

bool WorkspaceController::openFile(const QString &relativePath)
{
    if (!m_workspace || relativePath.isEmpty()) return false;

    // Reuse an existing leaf if one already holds this file.
    for (auto *leaf : m_workspace->allLeaves()) {
        const QJsonObject vs = leaf->getViewState();
        const QJsonObject state = vs.value(QStringLiteral("state")).toObject();
        if (state.value(QStringLiteral("file")).toString() == relativePath) {
            if (leaf->isDeferred()) leaf->loadIfDeferred();
            m_workspace->setActiveLeaf(leaf);
            return true;
        }
        if (auto *fv = qobject_cast<FileView *>(leaf->view())) {
            if (fv->file() && fv->file()->relativePath() == relativePath) {
                m_workspace->setActiveLeaf(leaf);
                return true;
            }
        }
    }

    auto *tabs = m_workspace->activeTabs();
    if (!tabs) return false;
    auto *leaf = m_workspace->createLeafInTabs(tabs);
    if (!leaf) return false;

    QString type;
    if (auto *registry = m_workspace->viewRegistry()) {
        const QString ext = QFileInfo(relativePath).suffix().toLower();
        type = registry->getTypeByExtension(ext);
    }
    if (type.isEmpty()) type = QStringLiteral("markdown");

    QJsonObject viewState;
    viewState[QStringLiteral("type")] = type;
    viewState[QStringLiteral("state")] = QJsonObject{
        {QStringLiteral("file"), relativePath}
    };
    leaf->setViewState(viewState);
    m_workspace->setActiveLeaf(leaf);
    m_workspace->pushLastOpenFile(relativePath);
    return true;
}

QString WorkspaceController::activeLeafId() const
{
    if (!m_workspace) return {};
    auto *leaf = m_workspace->activeLeaf();
    return leaf ? leaf->id() : QString();
}

bool WorkspaceController::splitLeaf(const QString &leafId, Qt::Orientation orientation)
{
    if (!m_workspace) return false;
    auto *leaf = m_workspace->findLeafById(leafId);
    if (!leaf) return false;
    return m_workspace->splitLeaf(leaf, orientation) != nullptr;
}

bool WorkspaceController::closeLeaf(const QString &leafId)
{
    if (!m_workspace) return false;
    auto *leaf = m_workspace->findLeafById(leafId);
    if (!leaf) return false;
    m_workspace->closeLeaf(leaf);
    return true;
}

bool WorkspaceController::popoutLeaf(const QString &leafId)
{
    if (!m_workspace) return false;
    auto *leaf = m_workspace->findLeafById(leafId);
    if (!leaf) return false;
    return m_workspace->popoutLeaf(leaf) != nullptr;
}

} // namespace Corbomite
