// SPDX-License-Identifier: GPL-3.0-or-later
#include "ExportToPdf.h"

#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

#include <KLocalizedString>

#include <QFileDialog>
#include <QFileInfo>
#include <QPageSize>
#include <QPrinter>
#include <QTextDocument>

namespace Corbomite::ExportToPdf {

bool exportFile(Corbomite::TFile *file, Corbomite::Vault *vault, QWidget *parent)
{
    if (!file || !vault) return false;

    const QString defaultName = file->basename + QStringLiteral(".pdf");
    const QString out = QFileDialog::getSaveFileName(
        parent, i18n("Export to PDF"), defaultName,
        i18n("PDF files (*.pdf)"));
    if (out.isEmpty()) return false;

    return exportFileToPath(file, vault, out);
}

bool exportFileToPath(Corbomite::TFile *file, Corbomite::Vault *vault,
                       const QString &outPath)
{
    if (!file || !vault || outPath.isEmpty()) return false;

    const QByteArray body = vault->read(file);
    if (body.isEmpty()) return false;

    // MVP pipeline: feed markdown through QTextDocument. Obsidian-equivalent
    // fidelity (callouts, embed blocks, math) needs ReadingView::renderToPdf
    // — a follow-up once ReadingView exposes a print seam. Until then,
    // QTextDocument yields a paginated PDF with headings, lists, tables,
    // emphasis, and code blocks rendered via Qt's Markdown parser.
    QTextDocument doc;
    doc.setMarkdown(QString::fromUtf8(body));

    QPrinter printer(QPrinter::PrinterResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setOutputFileName(outPath);

    doc.print(&printer);

    return QFileInfo::exists(outPath);
}

}  // namespace Corbomite::ExportToPdf
