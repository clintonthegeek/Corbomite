// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/NoteDocument.h"
#include <QRegularExpression>

namespace Corbomite {

NoteDocument::NoteDocument(const QString &vaultRoot, const QString &relativePath,
                           QObject *parent)
    : QObject(parent)
    , m_vaultRoot(vaultRoot)
    , m_relativePath(relativePath)
{
}

QString NoteDocument::filePath() const
{
    return m_vaultRoot + QLatin1Char('/') + m_relativePath;
}

QString NoteDocument::relativePath() const
{
    return m_relativePath;
}

QString NoteDocument::name() const
{
    QString fileName = m_relativePath.mid(m_relativePath.lastIndexOf(QLatin1Char('/')) + 1);
    int dotPos = fileName.lastIndexOf(QLatin1Char('.'));
    if (dotPos > 0) {
        return fileName.left(dotPos);
    }
    return fileName;
}

QString NoteDocument::markdown() const
{
    return m_markdown;
}

void NoteDocument::setMarkdown(const QString &text)
{
    if (m_markdown == text) {
        return;
    }
    m_markdown = text;
    m_cachedWordCount = -1; // Invalidate cache

    if (!m_modified) {
        m_modified = true;
        Q_EMIT modificationChanged(true);
    }

    Q_EMIT textChanged();
}

bool NoteDocument::isModified() const
{
    return m_modified;
}

void NoteDocument::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        Q_EMIT modificationChanged(modified);
    }
}

int NoteDocument::wordCount() const
{
    if (m_cachedWordCount >= 0) {
        return m_cachedWordCount;
    }

    if (m_markdown.isEmpty()) {
        m_cachedWordCount = 0;
        return 0;
    }

    // Strip markdown syntax characters, then count word-like tokens
    static const QRegularExpression wordPattern(QStringLiteral(R"(\b[a-zA-Z0-9]+(?:[-'][a-zA-Z0-9]+)*\b)"));

    int count = 0;
    auto it = wordPattern.globalMatch(m_markdown);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    m_cachedWordCount = count;
    return count;
}

int NoteDocument::characterCount() const
{
    return m_markdown.length();
}

} // namespace Corbomite
