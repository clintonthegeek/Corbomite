## Obsidian Sync Overview

**Obsidian Sync** is Obsidian’s official, end-to-end encrypted (optional) cloud service that creates and maintains *remote vaults* on Obsidian’s servers. It keeps a local copy of your vault on every device while syncing changes bidirectionally. It is far more than a generic file-sync tool like Dropbox, iCloud, or Google Drive—especially for collaborative work—because it is deeply integrated with Obsidian’s file format, conflict handling, versioning, and selective controls.

### What Is Actually Being Synced?
Obsidian Sync does **not** just mirror raw files. It syncs:

- **Core content**: All Markdown notes (`.md` files) and attachments. By default it syncs images, audio, video, and PDFs; you can toggle “Sync all other types” for everything else (e.g., PDFs, CSVs, etc.).
- **Attachments**: Subject to plan limits (5 MB per file on Standard; 200 MB on Plus).
- **Vault configuration (selectively)**: You can enable syncing of:
  - Main settings, appearance, themes & snippets, hotkeys.
  - Active core plugin list + their settings.
  - Active + installed community plugin lists (the actual plugin files are still installed locally per device, but the lists keep teams consistent).
- **Exclusions you control**: You can exclude specific folders or files, and certain things are *always* excluded (`.git`, `.obsidian` subfolders you don’t want, hidden files starting with `.`, File Recovery snapshots, and Sync’s own device-specific settings).

Everything (except Sync settings themselves) can be selectively enabled or disabled **per device**, so a phone can skip large attachments while a desktop syncs everything.

**Technically**, Obsidian watches for local changes, optionally encrypts them end-to-end with a password you set, uploads the changes to your remote vault on Obsidian’s servers, and other devices download and apply those changes. It is **not** real-time peer-to-peer or WebSocket-based live editing. Sync happens on save + periodic background checks (or manual “Sync now”).

### Features That Go Beyond Simple File Syncing (Killer Features)
These are what make Obsidian Sync special even for single-user use, and they become extremely powerful for teams:

- **Version history** (the biggest “beyond files” feature): Obsidian keeps a full history of every note and attachment.  
  - Notes: 1 month (Standard plan) or 12 months (Plus plan).  
  - Attachments: 2 weeks.  
  You can open any file → “Open version history,” view past versions, see timestamps, and restore any version with one click. There is also a separate “Deleted files” view for recovering or bulk-restoring deleted items. This is *not* just local File Recovery; it lives in the cloud and works across all devices and collaborators.

- **Conflict resolution**: You choose per device whether to auto-merge changes or create `.conflict` files when two people edit the same note at the same time. Merges happen automatically during the next sync.

- **Sync history sidebar**: Shows recent changes across the vault. On **shared vaults** it even tells you *who* last edited a file (hover tooltip).

- **Selective + device-specific sync**: You decide exactly what each device gets. Great for mobiles, shared team vaults, or keeping .obsidian configs in sync without forcing identical setups everywhere.

- **End-to-end encryption** (optional): Only you (and invited collaborators who know the password) can read the data—Obsidian’s servers see only encrypted blobs.

- **Headless Sync** and cross-platform support: Works on Mac/Windows/Linux/iOS/Android and even servers via command line.

### How Obsidian Sync Enables Collaborative Work (Shared Vaults)
This is the feature that directly answers “collaborative work that goes beyond syncing files between vaults.”

- **Shared remote vaults**: The vault owner can invite up to **20 other users** (by email) directly from Obsidian Settings → Sync → Manage → Manage sharing.  
- Every collaborator must have their own active Obsidian Sync subscription (joining a shared vault does **not** count against their personal remote-vault limit).  
- Once invited, everyone gets a full local copy of the same remote vault and syncs to/from it exactly like a personal vault.

**What actually happens for collab**:
- Any user edits a note → saves → it uploads to the shared remote vault.
- Other users’ devices download the change on their next sync (or background sync).
- **No live editing / real-time cursors** — edits only appear after sync. If two people edit the same file simultaneously, changes are merged (or conflict files are created, per your settings).
- **Sync history** shows who last edited what.
- **Version history** lets anyone view the full timeline of changes and restore any past version — the closest thing Obsidian has to Git blame / audit trail for teams.

**Permissions**: Currently all-or-nothing. Everyone has owner-level access; only the vault owner can invite or remove people. No fine-grained folder-level permissions yet.

**Setup is dead simple**:
1. Owner goes to Settings → Sync → Manage → Manage sharing → Invite user (email).
2. Invited user accepts and the shared vault appears in their Sync list.
3. (Optional) If E2EE is enabled, everyone enters the same encryption password once.

### Killer Features Specifically for Collaboration
- One canonical remote vault that everyone syncs against — no more “who has the latest copy?” problems that plague shared Dropbox/Google Drive vaults.
- Built-in versioning + who-edited-last visibility without needing Git.
- Automatic merge + easy rollback of team mistakes.
- Vault configuration can be partially synced so the team stays on the same themes/plugins/hotkeys without manual copying.
- Everything stays inside Obsidian — no switching to Notion, Google Docs, or external Git tools.

### Important Limitations (Especially for Teams)
- Requires a paid Sync subscription for **every** collaborator.
- No real-time collaborative editing (yet).
- No fine-grained permissions.
- Attachment size limits apply.
- Heavy simultaneous editing on the same files can produce merge conflicts (though the merge engine is quite good).
- Sync settings and some advanced config changes require a restart/reload on each device.

In short, Obsidian Sync turns your local Markdown vault into a team-friendly, versioned, selectively-synced knowledge base with almost zero setup overhead. It is the officially supported way to do collaborative work in Obsidian without leaving the app or relying on third-party sync services that frequently break Obsidian’s internal folders.

All of the above is taken directly from Obsidian’s current official help documentation (as of April 2026). If your team’s workflow needs true real-time editing, third-party plugins (e.g., Relay or LiveSync with custom setups) are still the workaround, but for most small-to-medium teams the built-in shared vaults + version history are the cleanest native solution.
