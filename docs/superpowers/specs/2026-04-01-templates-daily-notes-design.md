# Templates & Daily Notes — Design Specification

## Overview

Two tightly related features:
- **Templates:** Reusable note templates with variable expansion (`{{title}}`, `{{date}}`, `{{time}}`)
- **Daily Notes:** Create/open today's note with a single command, optionally using a template

## TemplateService

New service in `libs/models/` — manages template listing and variable expansion.

```cpp
class TemplateService : public QObject {
    Q_OBJECT
public:
    explicit TemplateService(VaultModel *vault, QObject *parent = nullptr);

    // Configuration
    void setTemplateFolder(const QString &folder);   // Relative to vault root
    QString templateFolder() const;

    // Template listing
    QStringList availableTemplates() const;           // Filenames without .md

    // Variable expansion
    QString expandTemplate(const QString &templateContent,
                           const QString &noteTitle) const;

    // Load and expand a template by name
    QString loadAndExpand(const QString &templateName,
                          const QString &noteTitle) const;
};
```

### Template Variables

| Variable | Expansion | Example |
|----------|-----------|---------|
| `{{title}}` | Name of the note being created | "My New Note" |
| `{{date}}` | Current date in default format | "2026-04-01" |
| `{{time}}` | Current time in default format | "14:30" |
| `{{date:FORMAT}}` | Date with custom Qt format | `{{date:dd/MM/yyyy}}` → "01/04/2026" |
| `{{time:FORMAT}}` | Time with custom Qt format | `{{time:HH:mm:ss}}` → "14:30:45" |

Default date format: `yyyy-MM-dd`. Default time format: `HH:mm`. Both configurable in settings.

### Template Folder

- Default: `Templates/` relative to vault root
- All `.md` files in that folder are available as templates
- Templates are just regular markdown files with optional `{{variables}}`
- Configurable via settings

## DailyNoteService

New service in `libs/models/` — creates or opens daily notes.

```cpp
class DailyNoteService : public QObject {
    Q_OBJECT
public:
    explicit DailyNoteService(VaultModel *vault, NoteService *noteService,
                               TemplateService *templateService, QObject *parent = nullptr);

    // Configuration
    void setDateFormat(const QString &format);   // Qt date format string
    void setFolder(const QString &folder);       // Relative to vault root
    void setTemplateName(const QString &name);   // Template to use for new daily notes

    // Core operation
    QString todayNotePath() const;               // Generates today's path: "Daily Notes/2026-04-01.md"
    bool todayNoteExists() const;                // Checks if today's file exists

    // Open or create
    NoteDocument *openOrCreateToday();           // Opens if exists, creates with template if not
};
```

### Daily Note Lifecycle

1. User triggers "Open Daily Note" command
2. `DailyNoteService::openOrCreateToday()`:
   a. Compute filename from current date + configured format
   b. Compute full path: `folder/filename.md`
   c. If file exists: open it via `NoteService::openNote()`
   d. If not: create via `NoteService::createNote()`, apply template if configured, save
3. Note opens in editor tab

## TemplatePicker Dialog

Simple modal for selecting a template to insert.

```cpp
class TemplatePicker : public QDialog {
    Q_OBJECT
public:
    explicit TemplatePicker(const QStringList &templates, QWidget *parent = nullptr);
    QString selectedTemplate() const;
};
```

- `QListWidget` showing template names
- Double-click or Enter selects
- Escape cancels
- Compact dialog, centered on parent

## Settings (KConfigXT)

Add to `corbomite.kcfg`:

```xml
<group name="Templates">
    <entry name="TemplateFolder" type="String">
        <label>Folder containing note templates</label>
        <default>Templates</default>
    </entry>
    <entry name="DefaultDateFormat" type="String">
        <label>Default date format for template variables</label>
        <default>yyyy-MM-dd</default>
    </entry>
    <entry name="DefaultTimeFormat" type="String">
        <label>Default time format for template variables</label>
        <default>HH:mm</default>
    </entry>
</group>
<group name="DailyNotes">
    <entry name="DailyNoteDateFormat" type="String">
        <label>Date format for daily note filenames</label>
        <default>yyyy-MM-dd</default>
    </entry>
    <entry name="DailyNoteFolder" type="String">
        <label>Folder for daily notes</label>
        <default>Daily Notes</default>
    </entry>
    <entry name="DailyNoteTemplate" type="String">
        <label>Template to apply to new daily notes (empty for none)</label>
        <default></default>
    </entry>
</group>
```

## MainWindow Integration

### New Actions

| Shortcut | Action ID | Description |
|----------|-----------|-------------|
| `Ctrl+T` | `insert_template` | Insert template at cursor |
| (none) | `open_daily_note` | Open/create today's daily note |

Both accessible via Command Palette (Ctrl+P).

### Menu

Add to Go menu in `corbomiteui.rc.in`:
```xml
<Action name="open_daily_note"/>
<Action name="insert_template"/>
```

### Action Handlers

**Insert Template:**
1. Get available templates from `TemplateService`
2. Show `TemplatePicker` dialog
3. On selection: load + expand template, insert at cursor (or replace note content if empty)

**Open Daily Note:**
1. Call `DailyNoteService::openOrCreateToday()`
2. Open returned document in editor

### Settings Dialog

Add a "Daily Notes" page to `SettingsDialog`:
- Daily note date format (QLineEdit)
- Daily note folder (QLineEdit)
- Daily note template (QComboBox populated from TemplateService)

Add a "Templates" section to existing editor page:
- Template folder (QLineEdit)

## Testing

### tst_templateservice.cpp

- `{{title}}` expansion with note name
- `{{date}}` expansion with default format
- `{{time}}` expansion with default format
- `{{date:dd/MM/yyyy}}` custom format
- `{{time:HH:mm:ss}}` custom format
- Multiple variables in one template
- No variables → content unchanged
- Available templates lists .md files in folder
- Empty template folder → empty list
- Template folder doesn't exist → empty list

### tst_dailynoteservice.cpp

- `todayNotePath()` generates correct path with date format
- `todayNoteExists()` returns false when file missing
- `openOrCreateToday()` creates file when missing
- `openOrCreateToday()` applies template on creation
- `openOrCreateToday()` opens existing file without modifying

## What This Does NOT Include

- Templater-style scripting (community plugin, far more complex)
- Periodic notes (weekly, monthly) — future
- Calendar widget for daily note navigation — future
- Template creation wizard — users create templates as regular .md files
