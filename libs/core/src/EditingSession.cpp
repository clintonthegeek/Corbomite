// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/EditingSession.h"

#include <markoff/core/MarkoffDocument.h>

#include <QUuid>

#include <QDebug>

namespace Corbomite {
namespace {

void warnImportFailed(const Markoff::VNext::ActionPublicationResult &published)
{
    if (published.diagnostic) {
        const std::string message(published.diagnostic->message());
        qWarning("initial Markdown import failed: %s", message.c_str());
    } else {
        qWarning("initial Markdown import failed");
    }
}

class DefaultEditingSessionSource final : public EditingSessionSource {
public:
    EditingBinding bindingFor(const EditingRequest &request) override
    {
        EditingBinding binding;
        auto provider = Markoff::VNext::makeEphemeralDocumentStorageProvider();
        if (!provider.ok()) {
            qWarning("ephemeral document provider was refused: %s",
                     std::string(provider.error().message()).c_str());
            return binding;
        }
        binding.provider = std::shared_ptr<Markoff::VNext::DocumentStorageProvider>(
            std::move(provider).value());
        binding.mode = Markoff::VNext::StorageMode::ephemeral;
        binding.scope.epoch = 1;
        binding.scope.documentKey = request.keyHint.isEmpty()
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : request.keyHint;
        return binding;
    }
};

EditingSessionSource *g_source = nullptr;

} // namespace

EditingSessionSource *editingSessionSource()
{
    if (g_source)
        return g_source;
    static DefaultEditingSessionSource fallback;
    return &fallback;
}

void setEditingSessionSource(EditingSessionSource *source)
{
    g_source = source;
}

std::unique_ptr<Markoff::MarkoffDocument> openMarkoffDocument(
    const EditingBinding &binding, QObject *parent)
{
    if (!binding.provider)
        return nullptr;
    auto opened = Markoff::MarkoffDocument::open(
        *binding.provider, binding.scope, binding.mode);
    if (!opened.ok()) {
        qWarning("MarkoffDocument::open failed: %s",
                 std::string(opened.error().message()).c_str());
        return nullptr;
    }
    auto document = std::move(opened).value();
    if (parent)
        document->setParent(parent);
    return document;
}

OpenDocument::OpenDocument() = default;
OpenDocument::~OpenDocument() = default;
OpenDocument::OpenDocument(OpenDocument &&) noexcept = default;
OpenDocument &OpenDocument::operator=(OpenDocument &&) noexcept = default;

bool publicationCommitted(const Markoff::VNext::ActionPublicationResult &result)
{
    return result.state == Markoff::VNext::ActionPublicationState::committed
        || result.state == Markoff::VNext::ActionPublicationState::committedHistoryPending;
}

OpenDocument openImportedDocument(const QByteArray &markdown, const QString &keyHint)
{
    OpenDocument opened;
    EditingRequest request;
    request.keyHint = keyHint;
    opened.binding = editingSessionSource()->bindingFor(request);
    opened.document = openMarkoffDocument(opened.binding);
    if (!opened.document)
        return opened;
    const auto published = opened.document->importInitialMarkdown(markdown);
    opened.imported = publicationCommitted(published);
    if (!opened.imported)
        warnImportFailed(published);
    return opened;
}

QString noteDocumentKeyHint(const QString &vaultRoot, const QString &relativePath)
{
    return vaultRoot + QLatin1Char('\n') + relativePath;
}

QString canvasTextCardKeyHint(const QString &canvasRelativePath, const QString &nodeId)
{
    return QStringLiteral("canvas:%1#%2").arg(canvasRelativePath, nodeId);
}

OpenDocument openForRender(const QByteArray &markdown, const QString &keyHint)
{
    OpenDocument opened;
    EditingRequest request;
    request.keyHint = keyHint;
    opened.binding = editingSessionSource()->bindingFor(request);
    opened.document = openMarkoffDocument(opened.binding);
    if (!opened.document)
        return opened;

    const auto state = opened.document->providerState();
    if (state == Markoff::VNext::ProviderDocumentState::ready) {
        opened.imported = true;
        return opened;
    }
    if (state == Markoff::VNext::ProviderDocumentState::recoveryRequired)
        return opened;
    if (state != Markoff::VNext::ProviderDocumentState::awaitingInitialImport
        && state != Markoff::VNext::ProviderDocumentState::initialImportAuthorized) {
        return opened;
    }

    const auto published = opened.document->importInitialMarkdown(markdown);
    opened.imported = publicationCommitted(published);
    if (!opened.imported)
        warnImportFailed(published);
    return opened;
}

} // namespace Corbomite
