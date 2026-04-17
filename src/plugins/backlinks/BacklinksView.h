// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QListWidget>
#include <QPointer>
#include <QWidget>

class QLabel;

namespace Corbomite {

class MetadataCacheReader;
class WorkspaceController;

/// Backlinks side panel — moved from src/sidebar/BacklinksPanel.{h,cpp}
/// during the Cluster Q migration to InternalPlugin form.
///
/// All plugin-side state (current file path) lives here; refresh is
/// triggered by MetadataCacheReader::cacheChanged and
/// WorkspaceController::activeFileChanged. Clicking a row routes
/// through WorkspaceController::openFile — no signals exposed back to
/// the host.
class BacklinksView : public QWidget
{
    Q_OBJECT
public:
    BacklinksView(MetadataCacheReader *metadata,
                  WorkspaceController *workspace,
                  QWidget *parent = nullptr);

private Q_SLOTS:
    void onItemClicked(QListWidgetItem *item);
    void onActiveFileChanged(const QString &path);
    void onCacheTouched(const QString &);

private:
    void refresh();

    QPointer<MetadataCacheReader> m_metadata;
    QPointer<WorkspaceController> m_workspace;

    QLabel *m_headerLabel;
    QListWidget *m_list;
    QLabel *m_emptyLabel;

    QString m_currentPath;
};

} // namespace Corbomite
