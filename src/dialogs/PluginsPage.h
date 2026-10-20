// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

class QDialog;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QVBoxLayout;

namespace Corbomite {

class PluginManager;

/// Settings page that lists every discovered plugin alongside its enable
/// state, declared permissions, and a per-plugin "Configure…" button when
/// the plugin exposes config pages.
///
/// The button appears only for a plugin that is currently loaded (a
/// disabled plugin has no instance and no `PluginContext` to build a page
/// against) and reports `configPages() > 0`. Clicking it opens a modal
/// dialog hosting the widgets from `Plugin::configPage(i, parent)`; the
/// plugin persists its own changes through
/// `PluginContext::saveData()` / `config()`.
///
/// Trusted plugins (built-ins) auto-grant their declared permissions, so
/// the per-permission checkboxes are read-only — there's nothing to
/// revoke. Untrusted plugins surface the checkboxes as live toggles
/// (placeholder for the granular grant UI; the permission grant dialog
/// grant dialog still drives first-enable prompts).
class PluginsPage : public QWidget
{
    Q_OBJECT
public:
    explicit PluginsPage(PluginManager *mgr, QWidget *parent = nullptr);

    /// Number of plugin rows in the list. Test-facing.
    int rowCount() const;

    /// Selects the row by index. Test-facing.
    void selectRow(int index);

    /// True if the per-plugin enable checkbox for the given plugin id is
    /// currently checked. Test-facing.
    bool isPluginChecked(const QString &pluginId) const;

    /// Programmatically toggle a plugin's enable state, exactly as the
    /// user clicking the row's checkbox would. Test-facing.
    void setPluginChecked(const QString &pluginId, bool checked);

    /// True if the per-permission checkboxes in the detail pane are
    /// editable for the currently selected plugin. Test-facing.
    bool detailPermissionsEditable() const;

private Q_SLOTS:
    void onSelectionChanged();
    void onItemChanged(QListWidgetItem *item);
    void onConfigureClicked();

private:
    void rebuild();
    void refreshDetail(int row);

    PluginManager *m_mgr;
    QListWidget   *m_list = nullptr;
    QWidget       *m_detail = nullptr;
    QLabel        *m_detailHeading = nullptr;
    QVBoxLayout   *m_detailLayout = nullptr;
    bool m_settingChecked = false; // re-entry guard for itemChanged
    bool m_currentEditable = false;

    /// Open plugin settings dialog, or nullptr. Owned by this page and
    /// destroyed on close (`Qt::WA_DeleteOnClose`), so it never outlives
    /// the plugin instance whose widget it hosts.
    QDialog *m_configDialog = nullptr;
    QString  m_configPluginId;     // plugin m_configDialog belongs to
};

} // namespace Corbomite
