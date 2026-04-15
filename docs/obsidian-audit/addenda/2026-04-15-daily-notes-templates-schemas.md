# Daily Notes + Templates JSON schemas

**Discovered during:** Cluster F (Templates / Daily Notes / Moment).
**Supersedes / extends:** `docs/obsidian-audit/VAULT-FORMAT.md §3` (which enumerated `.obsidian/*.json` files but didn't include these two).

## `.obsidian/daily-notes.json`

Controls the Daily Notes internal plugin. Present in Obsidian vaults when Daily Notes is enabled and configured away from defaults.

Schema:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `format` | string (Moment format) | `"YYYY-MM-DD"` | Date format for today's note filename |
| `folder` | string (vault-relative path) | `""` (vault root) | Folder where daily notes live; may include path segments (e.g. `"Daily Notes"`, `"Notes/Daily"`) |
| `template` | string (vault-relative path, optional) | `""` | Path to a template note applied when creating a new daily note (e.g. `"templates/Daily.md"`) |
| `autorun` | bool | `false` | Whether to auto-open today's daily note on vault open |

Unknown keys MUST be preserved on round-trip (standard `VaultConfig` contract).

## `.obsidian/templates.json`

Controls the Templates internal plugin.

Schema:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `folder` | string (vault-relative path) | `""` | Folder where template files live (e.g. `"templates"`) |
| `date_format` | string (Moment format) | `"YYYY-MM-DD"` | Format for `{{date}}` substitution |
| `time_format` | string (Moment format) | `"HH:mm"` | Format for `{{time}}` substitution |

Unknown keys MUST be preserved.

## Why noticed now

Cluster F (Templates / Daily Notes) requires vault-portable config — opening a vault in Obsidian then Corbomite should produce the same daily-note path / same template substitution behaviour. Corbomite's existing `CorbomiteSettings` (KConfig-based) stores these per-host, not per-vault. `VaultConfig` already handles the other `.obsidian/*.json` files; these two just need typed accessors.
