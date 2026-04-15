// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/FrontMatterWriter.h"

#include <markoff-parser/Document.h>

#include <QFile>
#include <QSaveFile>
#include <QTextStream>

namespace Corbomite {

namespace {

struct LoadedFile {
    bool ok = false;
    QString text;
    QString error;
};

LoadedFile readUtf8(const QString &path)
{
    LoadedFile out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        out.error = QStringLiteral("read-open failed: %1").arg(f.errorString());
        return out;
    }
    out.text = QString::fromUtf8(f.readAll());
    out.ok = true;
    return out;
}

bool writeAtomicUtf8(const QString &path, const QString &content, QString *errorOut)
{
    QSaveFile save(path);
    if (!save.open(QIODevice::WriteOnly)) {
        if (errorOut) *errorOut = QStringLiteral("write-open failed: %1")
                                      .arg(save.errorString());
        return false;
    }
    const QByteArray bytes = content.toUtf8();
    if (save.write(bytes) != bytes.size()) {
        if (errorOut) *errorOut = QStringLiteral("short write: %1")
                                      .arg(save.errorString());
        save.cancelWriting();
        return false;
    }
    if (!save.commit()) {
        if (errorOut) *errorOut = QStringLiteral("commit failed: %1")
                                      .arg(save.errorString());
        return false;
    }
    return true;
}

} // namespace

Markoff::YamlValue FrontMatterWriter::read(const QString &filePath,
                                           QString *errorOut)
{
    const LoadedFile loaded = readUtf8(filePath);
    if (!loaded.ok) {
        if (errorOut) *errorOut = loaded.error;
        return Markoff::YamlValue::emptyMap();
    }
    auto doc = Markoff::Document::fromMarkdown(loaded.text);
    if (!doc) {
        if (errorOut) *errorOut = QStringLiteral("document parse failed");
        return Markoff::YamlValue::emptyMap();
    }
    Markoff::YamlValue value = doc->parsedFrontmatter();
    if (value.isNull()) return Markoff::YamlValue::emptyMap();
    return value;
}

bool FrontMatterWriter::write(const QString &filePath,
                              const Markoff::YamlValue &value,
                              QString *errorOut)
{
    const LoadedFile loaded = readUtf8(filePath);
    if (!loaded.ok) {
        if (errorOut) *errorOut = loaded.error;
        return false;
    }
    auto doc = Markoff::Document::fromMarkdown(loaded.text);
    if (!doc) {
        if (errorOut) *errorOut = QStringLiteral("document parse failed");
        return false;
    }
    const QString rebuilt = doc->withFrontmatter(value);
    return writeAtomicUtf8(filePath, rebuilt, errorOut);
}

bool FrontMatterWriter::process(const QString &filePath,
                                const std::function<void(Markoff::YamlValue &)> &mutator,
                                QString *errorOut)
{
    if (!mutator) {
        if (errorOut) *errorOut = QStringLiteral("null mutator");
        return false;
    }
    const LoadedFile loaded = readUtf8(filePath);
    if (!loaded.ok) {
        if (errorOut) *errorOut = loaded.error;
        return false;
    }
    auto doc = Markoff::Document::fromMarkdown(loaded.text);
    if (!doc) {
        if (errorOut) *errorOut = QStringLiteral("document parse failed");
        return false;
    }

    Markoff::YamlValue current = doc->parsedFrontmatter();
    // Work on a mutable clone so the Document's immutable tree is untouched.
    Markoff::YamlValue working = current.isNull()
        ? Markoff::YamlValue::emptyMap()
        : current.clone();

    mutator(working);

    const QString rebuilt = doc->withFrontmatter(working);
    return writeAtomicUtf8(filePath, rebuilt, errorOut);
}

} // namespace Corbomite
