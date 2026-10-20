// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QHash>
#include <QTimer>

namespace Corbomite {

class NoteDocument;
class Vault;

class AutosaveReactor : public QObject {
    Q_OBJECT

public:
    explicit AutosaveReactor(Vault *vault, QObject *parent = nullptr);

    void watchDocument(NoteDocument *doc);
    void unwatchDocument(NoteDocument *doc);
    void setDelayMs(int ms);

Q_SIGNALS:
    void noteSaved(const QString &relativePath);

private:
    void onModificationChanged(NoteDocument *doc, bool modified);

    Vault *m_vault;
    QHash<NoteDocument *, QTimer *> m_timers;
    int m_delayMs = 2000;
};

} // namespace Corbomite
