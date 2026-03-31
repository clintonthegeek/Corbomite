// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace Corbomite {

class NoteDocument : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool modified READ isModified NOTIFY modificationChanged)

public:
    explicit NoteDocument(const QString &vaultRoot, const QString &relativePath,
                          QObject *parent = nullptr);

    QString filePath() const;
    QString relativePath() const;
    QString name() const;

    QString markdown() const;
    void setMarkdown(const QString &text);

    bool isModified() const;
    void setModified(bool modified);

    int wordCount() const;
    int characterCount() const;

Q_SIGNALS:
    void textChanged();
    void modificationChanged(bool modified);
    void saved();

private:
    QString m_vaultRoot;
    QString m_relativePath;
    QString m_markdown;
    bool m_modified = false;
    mutable int m_cachedWordCount = -1;
};

} // namespace Corbomite
