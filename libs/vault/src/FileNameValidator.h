// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>

namespace Corbomite {

class Vault;
class TAbstractFile;

/// Validates a proposed filename (or rename target).
///
/// Returns an empty `QString` when valid; a localised, user-facing error
/// message otherwise.
///
/// Rules (in order):
///   1. Empty name → rejected.
///   2. Bad characters `\\ / : * ? " < > |` → rejected.
///   3. Windows reserved basenames (CON, PRN, AUX, NUL, COM1..9, LPT1..9):
///      always rejected on Windows; rejected on other OSes only when
///      `isFinal == true` (so vaults roaming across OSes don't silently
///      produce unusable filenames).
///   4. Collision detection: when `vault` is non-null, a file at the
///      resolved candidate path other than `sourceFile` is a collision.
///
/// `sourceFile` is the file being renamed — used to exclude the file
/// itself from collision checks (a rename to the same name is always OK).
/// Pass nullptr when validating a brand-new filename.
///
/// `vault` may be nullptr to skip collision checks. Non-null enables the
/// full rule set.
///
/// `isFinal = true` applies stricter Windows reserved-name enforcement on
/// non-Windows hosts; `false` (the default) is a good fit for
/// typing-in-progress validation where partial rules would frustrate users.
QString validateFileName(const QString &newName,
                         const TAbstractFile *sourceFile,
                         const Vault *vault,
                         bool isFinal = false);

}  // namespace Corbomite
