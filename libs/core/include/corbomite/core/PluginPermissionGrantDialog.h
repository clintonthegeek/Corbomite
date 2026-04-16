// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QHash>
#include <QSet>
#include <QStringList>

class QCheckBox;

namespace Corbomite {

/// Modal dialog shown when an untrusted plugin is being enabled for the
/// first time, or after a metadata update introduces new permissions.
///
/// Lists declared permissions with human-readable descriptions; the user
/// checks which to grant. UX language reflects spec §5.3 — the prompt
/// reads "This plugin DECLARES it needs ...", not "This plugin CAN ...".
///
/// `grantedIfAccepted()` returns the currently-checked subset; check
/// `wasCancelled()` to distinguish "user clicked Cancel" (empty set,
/// cancelled=true) from "user accepted but unchecked everything"
/// (empty set, cancelled=false).
///
/// Test hooks: `setCheckedForTest()` and `cancelForTest()` allow unit
/// tests to drive the dialog state without invoking exec(). Production
/// callers go through exec() + accept/reject as normal.
class PluginPermissionGrantDialog : public QDialog
{
    Q_OBJECT
public:
    PluginPermissionGrantDialog(const QString &pluginName,
                                 const QString &pluginDescription,
                                 const QStringList &requestedPermissions,
                                 QWidget *parent = nullptr);

    QSet<QString> grantedIfAccepted() const;
    bool wasCancelled() const { return m_cancelled; }

    // Test-facing hooks (avoid opening a real modal in tests)
    void setCheckedForTest(const QString &token, bool checked);
    void cancelForTest();

    /// Human-readable one-line description of a capability token.
    /// Falls back to the token itself when unknown.
    static QString describe(const QString &token);

private Q_SLOTS:
    void onAccepted();
    void onRejected();

private:
    QHash<QString, QCheckBox *> m_boxes;
    bool m_cancelled = false;
};

} // namespace Corbomite
