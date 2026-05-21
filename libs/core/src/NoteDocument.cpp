// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkoffDocument.h>
#include <markoff/core/FindController.h>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace Corbomite {

struct NoteDocument::Private {
    QString vaultRoot;
    QString relativePath;
    std::unique_ptr<Markoff::MarkoffDocument> markoff;
    Markoff::FindController *findController = nullptr;
    bool modified = false;
    mutable int cachedWordCount = -1;
};

NoteDocument::NoteDocument(const QString &vaultRoot, const QString &relativePath,
                           QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
    d->vaultRoot    = vaultRoot;
    d->relativePath = relativePath;
    // TODO(port-foundation-exploration): new MarkoffDocument ctor takes
    // (replicaId, registry?, parent?) — no buffer/pool args. Using random
    // replicaId 1 here is the same workaround markoff-live-app used pre-
    // CollabText perf fix; revisit when collab use cases land.
    d->markoff = std::make_unique<Markoff::MarkoffDocument>(
        quint16{1}, /* registry */ nullptr, this);

    // TODO(port-foundation-exploration): contentsChanged(qsizetype, qsizetype,
    // qsizetype) replaced by d2DocumentChanged() (no args). Word-count
    // invalidation logic preserved; offset/removed/inserted info no longer
    // available to listeners.
    connect(d->markoff.get(), &Markoff::MarkoffDocument::d2DocumentChanged, this,
            [this]() {
        d->cachedWordCount = -1;
        if (!d->modified)
            setModified(true);
        Q_EMIT textChanged();
    });
    connect(d->markoff.get(), &Markoff::MarkoffDocument::documentReloaded, this,
            [this]() {
        d->cachedWordCount = -1;
        if (!d->modified)
            setModified(true);
        Q_EMIT textChanged();
    });
}

NoteDocument::~NoteDocument() = default;

QString NoteDocument::filePath() const
{
    return d->vaultRoot + QLatin1Char('/') + d->relativePath;
}

QString NoteDocument::relativePath() const
{
    return d->relativePath;
}

void NoteDocument::setRelativePath(const QString &relativePath)
{
    if (d->relativePath == relativePath)
        return;
    const QString oldPath = d->relativePath;
    d->relativePath = relativePath;
    Q_EMIT pathChanged(oldPath);
}

void NoteDocument::markDeleted()
{
    Q_EMIT deleted();
}

QString NoteDocument::name() const
{
    // Strip leading path, strip .md suffix. Preserve existing semantics.
    QString fileName = d->relativePath.mid(d->relativePath.lastIndexOf(QLatin1Char('/')) + 1);
    int dotPos = fileName.lastIndexOf(QLatin1Char('.'));
    if (dotPos > 0)
        return fileName.left(dotPos);
    return fileName;
}

QString NoteDocument::markdown() const
{
    return d->markoff->toMarkdown();  // toMarkdown() returns const QString& — copy on return
}

void NoteDocument::setMarkdown(const QString &text)
{
    // Generic-purpose setter — callers who know their use-case should prefer
    // markoff()->resetContent(text, Origin::*) directly.
    // TODO(port-foundation-exploration): new resetContent takes QByteArray.
    d->markoff->resetContent(text.toUtf8(), Markoff::Origin::TestFixture);
}

bool NoteDocument::isModified() const
{
    return d->modified;
}

void NoteDocument::setModified(bool modified)
{
    if (d->modified != modified) {
        d->modified = modified;
        Q_EMIT modificationChanged(modified);
    }
}

int NoteDocument::wordCount() const
{
    if (d->cachedWordCount >= 0)
        return d->cachedWordCount;

    const QString text = d->markoff->toMarkdown();
    if (text.isEmpty()) {
        d->cachedWordCount = 0;
        return 0;
    }

    // Strip markdown syntax characters, then count word-like tokens.
    static const QRegularExpression wordPattern(
        QStringLiteral(R"(\b[a-zA-Z0-9]+(?:[-'][a-zA-Z0-9]+)*\b)"));

    int count = 0;
    auto it = wordPattern.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    d->cachedWordCount = count;
    return count;
}

int NoteDocument::characterCount() const
{
    // TODO(port-foundation-exploration): length() was a method on the old
    // MarkoffDocument; new equivalent is visibleLength() returning UTF-8
    // byte count. Note: this changes the "character count" semantic from
    // QString-char to UTF-8-byte. For ASCII-heavy markdown the values
    // match; for multi-byte content this overcounts. Revisit if word/
    // character-count UX surfaces this discrepancy.
    return int(d->markoff->visibleLength());
}

Markoff::MarkoffDocument       *NoteDocument::markoff()       { return d->markoff.get(); }
const Markoff::MarkoffDocument *NoteDocument::markoff() const { return d->markoff.get(); }

Markoff::FindController *NoteDocument::findController()
{
    if (!d->findController) {
        d->findController = new Markoff::FindController(markoff(), this);
    }
    return d->findController;
}

} // namespace Corbomite
