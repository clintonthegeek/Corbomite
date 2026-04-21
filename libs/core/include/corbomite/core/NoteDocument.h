// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace Markoff { class MarkoffDocument; class ParsePool; }

namespace Corbomite {

class NoteDocument : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool modified READ isModified NOTIFY modificationChanged)

public:
    explicit NoteDocument(const QString &vaultRoot, const QString &relativePath,
                          Markoff::ParsePool *pool = nullptr,
                          QObject *parent = nullptr);
    ~NoteDocument() override;

    QString filePath() const;
    QString relativePath() const;
    QString name() const;

    QString markdown() const;
    void    setMarkdown(const QString &text);

    bool isModified() const;
    void setModified(bool modified);

    int wordCount() const;
    int characterCount() const;

    // New: leaves bind via note->markoff().
    Markoff::MarkoffDocument       *markoff();
    const Markoff::MarkoffDocument *markoff() const;

Q_SIGNALS:
    void textChanged();
    void modificationChanged(bool modified);
    void saved();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace Corbomite
