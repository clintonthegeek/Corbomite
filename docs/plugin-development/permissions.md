# Permissions

Corbomite gates plugin access to host services with a permission token system.
Each plugin declares the permissions it needs in its `metadata.json`; trusted
plugins (system-installed, marked `X-Corbomite-Trusted: true`) auto-grant their
declared set, while untrusted plugins prompt the user via
`PluginPermissionGrantDialog` at first enable.

A plugin that calls a permission-gated `PluginContext` accessor without holding
the relevant token gets `nullptr` back — the API silently no-ops rather than
throwing. Plugin authors should write defensive code that handles this case.

## Declaring permissions

```json
{
  "KPlugin": { "Id": "your-plugin-id", "Name": "Your Plugin" },
  "X-Corbomite-Permissions": [
    "vault.read",
    "ui.commands"
  ]
}
```

Constants live in `corbomite/core/PluginPermissions.h`
(`Corbomite::Permissions::kVaultRead` etc.) and are stable across releases —
the string values are the on-disk contract.

## Token reference

| Token | Gates | Example plugins | Deny-by-default if… |
|---|---|---|---|
| `vault.read` | `Vault::read`, `Vault::readBinary`, `Vault::readRaw`, `getFiles`, etc.; `vault.events` and `metadata.read` overlap (any one suffices for `PluginContext::vault()` to return non-null) | search-extension, backlink-aggregator, file-indexer | Plugin claims read but never calls a vault-read accessor — review |
| `vault.write` | `Vault::modify`, `Vault::create`, `Vault::process`, `Vault::rename`, `Vault::trash`, `FileManager::renameFile`, `FileManager::generateMarkdownLink` (the rewrite path), the `RenameDialog`/`DeleteConfirmDialog` prompts | note-template, daily-notes, pandoc-import | Pure read-only viewer plugin — deny |
| `vault.events` | Subscription to `Vault::created`, `modified`, `deletedFile`, `renamed`, `documentSaved`, `raw`, `configChanged`. Independent of `vault.read` so an event-only plugin can listen without read access. | linter, indexer, sync-watch | Plugin only renders, no event listening — deny |
| `metadata.read` | `MetadataCache` reads, `SQLiteIndex` queries via `SearchProxy` | breadcrumb-nav, related-files, tag-explorer | n/a (read-only data) |
| `workspace` | `WorkspaceController`: open / close leaves, switch tabs, query active leaf | quick-switcher, tab-manager, session-restore | Read-only viewer plugins — deny |
| `ui.commands` | `addCommand`, `addRibbonIcon`. The Cluster B ribbon proxy lives under this token (no separate `ui.ribbon`) since ribbon icons are command surrogates. | most plugins | Hidden background plugins (rare) |
| `ui.views` | `registerView` (custom panes / leaves) + the workspace tool-view registration path | bases, kanban, graph-view, file-recovery | Plugins with no UI surface — deny |
| `ui.menus` | `MenuInjector` for context-menu / file-menu / editor-menu / tab-menu hooks | context-menu plugins, custom right-click actions | Plugins that don't add menu entries — deny |
| `ui.rendering` | `registerHoverLinkSource`, `registerEmbed`, `registerMarkdownPostProcessor`, `registerMarkdownCodeBlockProcessor` | math, mermaid, custom-embed, footnote | Non-rendering plugins — deny |
| `ui.editor` | `registerEditorSuggest`, `registerEditorExtension` (decoration-provider only — see Cluster E for the full extension ABI) | autocomplete-dictionary, smart-typography, syntax-highlight-extras | n/a |
| `ui.statusbar` | `addStatusBarItem` | word-count, sync-status, current-vault-indicator | Most plugins — deny by default |
| `ui.icons` | `addIcon` for the Lucide-named icon registry | theme-extension, branding | Most plugins — deny |
| `network` | `QNetworkAccessManager` access | web-clipper, sync-providers, fetch-bibliography | Local-only plugins — deny |
| `secrets` | `SecretStorage` (QtKeychain-backed) for OAuth tokens / API keys / encrypted credentials | sync-providers, oauth, api-clients | n/a |
| `process` | `ProcessSpawner` for spawning external processes | git-integration, pandoc-export, shell-scripts | Most plugins — deny |
| `config` | Per-plugin `data.json` read/write via `PluginContext::loadData`/`saveData`; KConfig group access; the `Plugin::onExternalSettingsChange()` dispatcher | every plugin with persistent state | n/a |
| `protocol` | `registerObsidianProtocolHandler` for `corbomite://` / `obsidian://` URL routing | url-router, deep-link plugins, vault-sync handshake | Most plugins — deny |

## Per-token notes

### `vault.read`, `vault.write`, `vault.events`

These three split what was a single `vault` token in pre-Cluster-Q drafts.
The split is load-bearing: a search indexer plugin that only needs to listen
for `modified` to invalidate its index can declare *just* `vault.events` and
won't get read access to file content. The `VaultProxy` constructed by
`PluginContext::vault()` then per-method-gates every accessor.

`Vault::raw` (Cluster B) and `Vault::configChanged` (Cluster B) are
event-side signals — they ride `vault.events`. Plugins that want to
observe `.obsidian/` writes specifically should declare `vault.events`
and connect to `configChanged` (which only fires for `.obsidian/*.json`).

### `ui.rendering`

Aggregates four registries: hover-link source, embed factory, markdown
post-processor, code-block processor. The grouping is intentional — a
plugin that adds a math renderer typically wants both the embed factory
(for `![[equation.tex]]`) and the code-block processor (for ` ```math `
fences). One token, one prompt.

`MarkdownRenderer::render` (Cluster B item #10) is permissionless — it's a
side-effect-free function that produces a widget from a markdown string.
Plugins call it directly without permission gating.

### `ui.editor`

Two registries: editor-suggest and editor-extension (decoration-provider).
The full `EditorExtension` ABI (gutter widgets, keymap injection, theme
overrides, custom cursor) is deferred to Cluster E; today only the
decoration-provider hook is exposed.

### `ui.statusbar` vs `ui.commands`

The status bar is a separate token from `ui.commands` because adding a
permanent widget to the status bar is a different kind of access from
adding a command palette entry. A plugin that only adds commands shouldn't
get a free pass to put persistent UI in the user's chrome.

### `protocol`

Specifically gates URL handler registration. The plugin doesn't gain the
ability to *open* URLs — every plugin can open URLs without permission.
This token gates *receiving* URL dispatches.

## Trust model

| Origin | Default | Override |
|---|---|---|
| **System** (KDE_INSTALL_PLUGINDIR/corbomite) | Trusted | n/a |
| **User** (~/.local/share/corbomite/plugins) | Untrusted regardless of `X-Corbomite-Trusted` | n/a — user-installed plugins always prompt |

Trusted plugins auto-grant their declared permissions; untrusted plugins
prompt via `PluginPermissionGrantDialog`. The user's grants persist in
KConfig under `Plugin-<id>/GrantedPermissions=...`.

## Adding a new permission token

When a new host capability needs gating:

1. Add a `kFooBar` constant to `corbomite/core/PluginPermissions.h`.
2. Update the `PluginContext` accessor to call `hasPermission(QLatin1String(kFooBar))`.
3. Add a row to the table above.
4. Update the kitchen-sink reference plugin
   (`tests/plugins/cluster-b-kitchen-sink/`) to declare and exercise the
   token if it's broadly relevant.

The string value is the on-disk contract — never rename a published token.
Add new tokens; never repurpose old ones.
