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

    // Save watermark: the document edit-sequence values captured the last time
    // the document was marked clean (load / save / reload). MarkoffDocument
    // schedules d2DocumentChanged via QTimer::singleShot(0), so a clean→save
    // sequence can leave a *stale* change notification queued; when it fires
    // after the save it must NOT re-dirty already-saved content. Gating the
    // re-dirty on actual sequence advancement past this watermark fixes that.
    // Both sequences are tracked because legacy edits bump editSequence() while
    // D2 edits bump d2EditSequence().
    quint64 savedEditSeq = 0;
    quint64 savedD2Seq = 0;
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
        if (!d->modified && hasUnsavedEdits())
            setModified(true);
        Q_EMIT textChanged();
    });
    connect(d->markoff.get(), &Markoff::MarkoffDocument::documentReloaded, this,
            [this]() {
        d->cachedWordCount = -1;
        if (!d->modified && hasUnsavedEdits())
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
    // Route through serializeForSave() — toMarkdown() reads MarkoffDocument's
    // legacy buffer which loadFromMarkdown() and D2 edits do not update.
    // See Markoff docs/handoff/2026-05-21-save-path-data-loss.md.
    return QString::fromUtf8(d->markoff->serializeForSave());
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

bool NoteDocument::hasUnsavedEdits() const
{
    return d->markoff->editSequence() != d->savedEditSeq
        || d->markoff->d2EditSequence() != d->savedD2Seq;
}

void NoteDocument::setModified(bool modified)
{
    if (!modified) {
        // Marking clean (load / save / reload): record the current edit
        // watermark so a deferred d2DocumentChanged queued before this point
        // — representing content that is now on disk — does not re-dirty the
        // document when it finally fires. See Private::savedEditSeq.
        d->savedEditSeq = d->markoff->editSequence();
        d->savedD2Seq   = d->markoff->d2EditSequence();
    }
    if (d->modified != modified) {
        d->modified = modified;
        Q_EMIT modificationChanged(modified);
    }
}

int NoteDocument::wordCount() const
{
    if (d->cachedWordCount >= 0)
        return d->cachedWordCount;

    // Route through markdown() — d->markoff->toMarkdown() reads MarkoffDocument's
    // legacy buffer which D2 edits do not update.
    const QString text = markdown();
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
