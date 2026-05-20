// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkoffDocument.h>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace Corbomite {

struct NoteDocument::Private {
    QString vaultRoot;
    QString relativePath;
    std::unique_ptr<Markoff::MarkoffDocument> markoff;
    bool modified = false;
    mutable int cachedWordCount = -1;
};

NoteDocument::NoteDocument(const QString &vaultRoot, const QString &relativePath,
                           Markoff::ParsePool *pool, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
    d->vaultRoot    = vaultRoot;
    d->relativePath = relativePath;
    d->markoff = std::make_unique<Markoff::MarkoffDocument>(
        /* buffer */ nullptr, pool, this);

    connect(d->markoff.get(), &Markoff::MarkoffDocument::contentsChanged, this,
            [this](qsizetype /*offset*/, qsizetype /*removed*/, qsizetype /*inserted*/) {
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
    d->markoff->resetContent(text, Markoff::Origin::TestFixture);
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
    return int(d->markoff->length());
}

Markoff::MarkoffDocument       *NoteDocument::markoff()       { return d->markoff.get(); }
const Markoff::MarkoffDocument *NoteDocument::markoff() const { return d->markoff.get(); }

} // namespace Corbomite
