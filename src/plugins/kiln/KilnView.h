// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "KilnSettings.h"

#include <QJsonObject>
#include <QTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace Corbomite {

class PluginContext;
class KilnRunner;

/// The Kiln panel: which notes become a site, and what the engine said about it.
///
/// A view rather than a settings page. Settings → Plugins hosts plugin settings
/// pages via `Plugin::configPages()` / `configPage()` (wired 2026-10-18, see
/// `PluginsPage`), but a view is the right shape here: `createView()` is what every
/// bundled plugin uses, and this panel also shows the engine's report, which is not
/// a setting.
class KilnView : public QWidget
{
    Q_OBJECT
public:
    KilnView(PluginContext *context, KilnRunner *runner, QWidget *parent = nullptr);

    /// What would be handed to the engine right now, after folding the form back
    /// into the settings. Exposed so a test can assert the shape without a process.
    QJsonObject configForRun() const;

    /// The panel's text. Also exposed for tests.
    QString reportText() const;

    /// Reload the stored settings when they changed on disk under us — a hand edit, or
    /// another writer. Does nothing when the file already agrees with the panel, which
    /// is the case for the panel's own saves.
    void reloadFromStoreIfChanged();

    const KilnSettings &settings() const { return m_settings; }

public Q_SLOTS:
    void runCheck();
    void runProject();
    void runScaffold();
    void addProjection();
    void removeProjection();

private Q_SLOTS:
    void onProjectionSelected(int row);
    void onFieldChanged();
    void onBrowseEngine();
    void onBrowseOutput();
    void onRevealOutput();
    void onCopyConfig();
    void onRunFinished(int exitCode, const QJsonObject &report, const QString &standardError);

private:
    void loadSettings();
    void saveSettings();
    void rebuildProjectionList();
    void loadProjectionIntoForm(int index);
    void storeFormIntoProjection();
    void setBusy(bool busy);
    void setReport(const QString &text);
    void appendReport(const QString &text);
    void run(const QString &mode);
    QString emptyStateText() const;

    PluginContext *m_context = nullptr;
    KilnRunner *m_runner = nullptr;

    KilnSettings m_settings;
    QString m_vaultPath;
    int m_current = -1;
    /// Guards the field-changed handler while the form is being filled
    /// programmatically, so loading a projection does not look like an edit.
    bool m_loading = false;

    QTimer m_saveTimer;

    QLineEdit *m_enginePath = nullptr;
    QLineEdit *m_output = nullptr;
    QListWidget *m_projectionList = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_folders = nullptr;
    QLineEdit *m_deny = nullptr;
    QLineEdit *m_orderFrom = nullptr;
    QLineEdit *m_section = nullptr;
    QComboBox *m_shape = nullptr;
    QComboBox *m_frontMatter = nullptr;
    QLineEdit *m_fieldOrder = nullptr;
    QLineEdit *m_linkBase = nullptr;
    QLineEdit *m_taxonomies = nullptr;
    QCheckBox *m_related = nullptr;
    QCheckBox *m_tagParents = nullptr;
    QCheckBox *m_fragments = nullptr;
    QCheckBox *m_scaffold = nullptr;
    QPushButton *m_checkButton = nullptr;
    QPushButton *m_projectButton = nullptr;
    QPushButton *m_scaffoldButton = nullptr;
    QPlainTextEdit *m_report = nullptr;
};

} // namespace Corbomite
