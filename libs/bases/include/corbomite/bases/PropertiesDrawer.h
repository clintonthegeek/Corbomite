// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>

namespace Corbomite {
class FileManager;
class TFile;
}

class QFormLayout;
class QLabel;

namespace Corbomite::Bases {

class BasesEntry;

/// Right-pane editor for the selected entry's frontmatter. Renders a form of
/// label + type-appropriate editor; emits frontMatterEditRequested on commit
/// (BasesView routes it through the undo stack).
class PropertiesDrawer : public QWidget
{
    Q_OBJECT
public:
    explicit PropertiesDrawer(QWidget *parent = nullptr);

    void setFileManager(FileManager *fm) { m_fm = fm; }
    /// Populate from `entry` (its file + frontmatter). Null clears the form.
    void showEntry(BasesEntry *entry);

Q_SIGNALS:
    void frontMatterEditRequested(Corbomite::TFile *file, const QString &key,
                                  const QVariant &value);

private:
    void clearForm();
    void commit(const QString &key, const QVariant &value);

    FileManager *m_fm = nullptr;
    TFile *m_file = nullptr;
    QLabel *m_title = nullptr;
    QFormLayout *m_form = nullptr;
};

}  // namespace Corbomite::Bases
