// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QWidget;

namespace Corbomite {
class Vault;
class TFile;
}  // namespace Corbomite

namespace Corbomite::ExportToPdf {

/// Show a Save-As dialog and export the note to PDF. Returns true on
/// success. Cluster R Task 3.3 — backs the MarkdownView "Export to PDF..."
/// menu action.
bool exportFile(Corbomite::TFile *file, Corbomite::Vault *vault,
                QWidget *parent);

/// Test seam: no dialog. Writes a PDF to `outPath` directly. Renders the
/// note's markdown through QTextDocument — a minimum-viable PDF pipeline;
/// richer fidelity (via ReadingView) is a follow-up.
bool exportFileToPath(Corbomite::TFile *file, Corbomite::Vault *vault,
                      const QString &outPath);

}  // namespace Corbomite::ExportToPdf
