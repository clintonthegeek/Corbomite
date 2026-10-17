// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <markoff/vnext/ActionPublicationCoordinator.h>
#include <markoff/vnext/DocumentStorageProvider.h>

#include <QByteArray>
#include <QString>

#include <memory>

namespace Markoff {
class MarkoffDocument;
}

namespace Corbomite {

/// What a caller knows about a document before a provider is chosen.
/// `keyHint` is a stable lookup string, not a document UUID and not a
/// vault path that anyone should persist. Notes pass
/// `vaultRoot + '\n' + relativePath`. A canvas text card should pass
/// `canvas:<vault-relative .canvas path>#<node id>` so a later
/// collaboration plugin can recognise the card without Corbomite having
/// stored an identity for it. An empty hint means "fresh ephemeral key".
struct EditingRequest {
    QString keyHint;
};

/// Caller-owned provider plus the scope Markoff should open. The provider
/// must outlive every document opened against it. The default source
/// returns a new ephemeral provider per call, so each document is its own
/// session. A plugin replaces the source and may instead return one shared
/// durable provider with a different scope for each request.
struct EditingBinding {
    std::shared_ptr<Markoff::VNext::DocumentStorageProvider> provider;
    Markoff::VNext::DocumentStorageScope scope;
    Markoff::VNext::StorageMode mode = Markoff::VNext::StorageMode::ephemeral;
};

class EditingSessionSource {
public:
    virtual ~EditingSessionSource() = default;
    virtual EditingBinding bindingFor(const EditingRequest &request) = 0;
};

/// Process-wide. Plugins call `setEditingSessionSource` before any note or
/// canvas card is opened. Passing nullptr restores the ephemeral default.
/// The pointer is not owned.
EditingSessionSource *editingSessionSource();
void setEditingSessionSource(EditingSessionSource *source);

/// Open `binding`. On success the document is parented to `parent` when
/// `parent` is non-null; the unique_ptr is still the owner. Returns null
/// when the provider is missing or Markoff refuses the open.
std::unique_ptr<Markoff::MarkoffDocument> openMarkoffDocument(
    const EditingBinding &binding, QObject *parent = nullptr);

bool publicationCommitted(const Markoff::VNext::ActionPublicationResult &result);

/// Ephemeral document with its initial Markdown already imported. Keeps the
/// provider alive for as long as this object does. Used by tests and by any
/// host surface that is not a `NoteDocument` (a canvas card will be one).
struct OpenDocument {
    EditingBinding binding;
    std::unique_ptr<Markoff::MarkoffDocument> document;
    bool imported = false;

    OpenDocument();
    ~OpenDocument();
    OpenDocument(OpenDocument &&) noexcept;
    OpenDocument &operator=(OpenDocument &&) noexcept;
    OpenDocument(const OpenDocument &) = delete;
    OpenDocument &operator=(const OpenDocument &) = delete;

    Markoff::MarkoffDocument *get() const { return document.get(); }
};

OpenDocument openImportedDocument(const QByteArray &markdown,
                                  const QString &keyHint = {});

/// Note identity for `EditingRequest::keyHint`. Not a UUID and not written
/// to the vault. `NoteDocument` and a canvas file card for that note must
/// both use this string, so a plugin can recognise them as one document.
QString noteDocumentKeyHint(const QString &vaultRoot, const QString &relativePath);

/// Canvas text-card identity. `canvasRelativePath` is the vault-relative
/// `.canvas` path (absolute path when the canvas is outside a vault).
/// `nodeId` is the card's canvas node id. Not a filesystem path.
QString canvasTextCardKeyHint(const QString &canvasRelativePath, const QString &nodeId);

/// Open for painting. Imports `markdown` only while the provider is
/// `awaitingInitialImport` or `initialImportAuthorized`. A `ready` document
/// is the plugin's live projection: `imported` is true and `markdown` is
/// ignored, so a file-card subpath slice cannot be imported over it.
/// `recoveryRequired` is not imported and `imported` stays false.
/// The default source mints a new ephemeral provider per call, so stock
/// paints import `markdown` into an empty document.
OpenDocument openForRender(const QByteArray &markdown,
                           const QString &keyHint = {});

} // namespace Corbomite
