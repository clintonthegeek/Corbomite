// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/NoteDocument.h"

#include "corbomite/core/EditingSession.h"

#include <markoff/core/BlockEdit.h>
#include <markoff/core/BlockId.h>
#include <markoff/core/MarkoffDocument.h>
#include <markoff/core/FindController.h>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace Corbomite {

struct NoteDocument::Private {
    QString vaultRoot;
    QString relativePath;
    EditingBinding editing;
    std::unique_ptr<Markoff::MarkoffDocument> markoff;
    Markoff::FindController *findController = nullptr;
    bool modified = false;
    bool imported = false;
    mutable int cachedWordCount = -1;

    // Save watermark. d2EditSequence() does not move for a pristine import
    // or a whole-document replace, so hostEditSeq counts those publications
    // too. Both are captured when the document is marked clean. A deferred
    // d2DocumentChanged that fires after a save must not re-dirty content
    // that is already on disk.
    quint64 hostEditSeq = 0;
    quint64 savedD2Seq = 0;
    quint64 savedHostSeq = 0;
};

NoteDocument::NoteDocument(const QString &vaultRoot, const QString &relativePath,
                           QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
    d->vaultRoot    = vaultRoot;
    d->relativePath = relativePath;
    // The session source is the plugin seam. The default source is an
    // ephemeral provider per document, so Markdown on disk stays the
    // Obsidian file and no writer identity is stored. A collaboration
    // plugin replaces the source before notes are opened.
    EditingRequest request;
    request.keyHint = noteDocumentKeyHint(vaultRoot, relativePath);
    d->editing = editingSessionSource()->bindingFor(request);
    d->markoff = openMarkoffDocument(d->editing, this);

    if (d->markoff) {
        connect(d->markoff.get(), &Markoff::MarkoffDocument::d2DocumentChanged, this,
                [this]() {
            d->cachedWordCount = -1;
            if (!d->modified && hasUnsavedEdits())
                setModified(true);
            Q_EMIT textChanged();
        });
    }
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

QString NoteDocument::vaultRoot() const
{
    return d->vaultRoot;
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
    if (!d->markoff)
        return {};
    return QString::fromUtf8(d->markoff->serializeForSave());
}

void NoteDocument::setMarkdown(const QString &text)
{
    publishMarkdown(text.toUtf8());
}

bool NoteDocument::publishMarkdown(const QByteArray &utf8)
{
    if (!d->markoff)
        return false;
    const bool wasModified = d->modified;
    const quint64 previousHostSeq = d->hostEditSeq;
    ++d->hostEditSeq;
    const auto published = d->imported
        ? d->markoff->replaceMarkdown(utf8)
        : d->markoff->importInitialMarkdown(utf8);
    if (!publicationCommitted(published)) {
        d->hostEditSeq = previousHostSeq;
        if (!wasModified && d->modified && !hasUnsavedEdits())
            setModified(false);
        return false;
    }
    d->imported = true;
    if (hasUnsavedEdits())
        setModified(true);
    return true;
}

bool NoteDocument::insertAtFlatOffset(uint32_t byteOffset, const QByteArray &utf8)
{
    if (!d->markoff)
        return false;
    if (!d->imported) {
        if (byteOffset != 0)
            return false;
        return publishMarkdown(utf8);
    }
    const auto ids = d->markoff->iterateBlocks();
    if (ids.empty()) {
        if (byteOffset != 0)
            return false;
        return publishMarkdown(utf8);
    }

    Markoff::BlockId target = ids.back();
    uint32_t local = uint32_t(d->markoff->blockText(target).size());
    uint32_t remain = byteOffset;
    for (const Markoff::BlockId id : ids) {
        const auto text = d->markoff->blockText(id);
        const auto size = uint32_t(text.size());
        if (remain <= size) {
            target = id;
            local = remain;
            break;
        }
        remain -= size;
    }
    // A blank line is a Markdown block boundary. pasteMarkdownAt trims the
    // payload and leaves the caret's head as its own block, so splice the
    // bytes into this block and replace the whole block range. Anything
    // else is a literal block edit, which keeps a leading space.
    const bool wasModified = d->modified;
    const quint64 previousHostSeq = d->hostEditSeq;
    ++d->hostEditSeq;
    Markoff::VNext::ActionPublicationResult published;
    if (utf8.contains("\n\n")) {
        const QByteArray text = d->markoff->blockText(target);
        const QByteArray spliced = text.left(qsizetype(local)) + utf8 + text.mid(qsizetype(local));
        published = d->markoff->pasteMarkdownReplacingRange(
            target, 0, target, uint32_t(text.size()), spliced).publication;
    } else {
        published = d->markoff->applyBlockEdit({target, local, 0, utf8});
    }
    if (!publicationCommitted(published)) {
        d->hostEditSeq = previousHostSeq;
        if (!wasModified && d->modified && !hasUnsavedEdits())
            setModified(false);
        return false;
    }
    if (hasUnsavedEdits())
        setModified(true);
    return true;
}

bool NoteDocument::isModified() const
{
    return d->modified;
}

bool NoteDocument::hasUnsavedEdits() const
{
    if (!d->markoff)
        return false;
    return d->markoff->d2EditSequence() != d->savedD2Seq
        || d->hostEditSeq != d->savedHostSeq;
}

void NoteDocument::setModified(bool modified)
{
    if (!modified && d->markoff) {
        // Marking clean (load / save / reload): record the current edit
        // watermark so a deferred d2DocumentChanged queued before this point
        // does not re-dirty content that is now on disk.
        d->savedD2Seq = d->markoff->d2EditSequence();
        d->savedHostSeq = d->hostEditSeq;
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
    if (!d->markoff)
        return 0;
    return int(d->markoff->serializeForSave().size());
}

Markoff::MarkoffDocument       *NoteDocument::markoff()       { return d->markoff.get(); }
const Markoff::MarkoffDocument *NoteDocument::markoff() const { return d->markoff.get(); }

Markoff::FindController *NoteDocument::findController()
{
    if (!d->findController && markoff()) {
        d->findController = new Markoff::FindController(markoff(), this);
    }
    return d->findController;
}

} // namespace Corbomite
