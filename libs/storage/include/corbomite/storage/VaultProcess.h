// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QString>

namespace Corbomite {

class DataAdapter;

/// Atomic whole-body read-modify-write on a vault note.
///
/// Mirrors Obsidian's `Vault.process` (`docs/obsidian-audit/domains/vault.md §1`).
/// Reads the current file content, passes it to `mutator`, writes the result
/// back atomically via the `DataAdapter`.
///
/// Concurrency contract: concurrent `process()` calls on the **same path** are
/// serialised via a per-path mutex, so the final file is always the result of
/// one mutator's output applied to the other's — never a lost update. Cross-
/// process safety (another editor writing the file externally) is not
/// provided; for that Corbomite relies on the external-edit watcher and the
/// `WriteHints{mtimeMs}` echo-suppression contract.
class VaultProcess
{
public:
    /// Mutator: `(currentContent) -> newContent`.
    using Mutator = std::function<QString(const QString &)>;

    /// Run the atomic RMW cycle. Returns true on success, false on read/write
    /// failure. The mutator is invoked once per call (no retry loop in the
    /// single-process model; the per-path mutex ensures serialisation).
    static bool process(DataAdapter *fs,
                        const QString &absolutePath,
                        const Mutator &mutator);
};

} // namespace Corbomite
