// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

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
/// Trusted plugins (built-ins) auto-grant their declared permissions, so
/// the per-permission checkboxes are read-only — there's nothing to
/// revoke. Untrusted plugins surface the checkboxes as live toggles
/// (placeholder for the granular grant UI; the permission grant dialog
/// from Task 4 still drives first-enable prompts).
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
};

} // namespace Corbomite
