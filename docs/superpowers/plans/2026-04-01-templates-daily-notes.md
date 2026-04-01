# Templates & Daily Notes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add template variable expansion and daily note creation, two core Obsidian workflow features.

**Architecture:** TemplateService handles template listing and `{{variable}}` expansion. DailyNoteService uses TemplateService to create/open today's note. Both are services in libs/models. TemplatePicker is a simple QDialog. Settings via KConfigXT.

**Tech Stack:** C++20, Qt6, KConfigXT, QRegularExpression for variable parsing

**Spec:** `docs/superpowers/specs/2026-04-01-templates-daily-notes-design.md`

---

### Task 1: TemplateService with Variable Expansion

**Files:**
- Create: `libs/models/include/corbomite/models/TemplateService.h`
- Create: `libs/models/src/TemplateService.cpp`
- Modify: `libs/models/CMakeLists.txt`
- Create: `tests/models/tst_templateservice.cpp`
- Modify: `tests/models/CMakeLists.txt`

- [ ] **Step 1: Write TemplateService tests**

`tests/models/tst_templateservice.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QDate>
#include <QTime>
#include "corbomite/models/TemplateService.h"
#include "corbomite/models/VaultModel.h"

class TestTemplateService : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

private Q_SLOTS:
    void testExpandTitle()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("# {{title}}\n\nContent"), QStringLiteral("My Note"));
        QCOMPARE(result, QStringLiteral("# My Note\n\nContent"));
    }

    void testExpandDate()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("Date: {{date}}"), QStringLiteral("Note"));
        QString expected = QStringLiteral("Date: ") + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
        QCOMPARE(result, expected);
    }

    void testExpandTime()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("Time: {{time}}"), QStringLiteral("Note"));
        // Time changes — just verify it doesn't contain {{time}} anymore
        QVERIFY(!result.contains(QStringLiteral("{{time}}")));
        QVERIFY(result.startsWith(QStringLiteral("Time: ")));
    }

    void testExpandCustomDateFormat()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("{{date:dd/MM/yyyy}}"), QStringLiteral("Note"));
        QString expected = QDate::currentDate().toString(QStringLiteral("dd/MM/yyyy"));
        QCOMPARE(result, expected);
    }

    void testExpandCustomTimeFormat()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("{{time:HH:mm:ss}}"), QStringLiteral("Note"));
        QVERIFY(!result.contains(QStringLiteral("{{time:")));
        QVERIFY(result.contains(QStringLiteral(":")));
    }

    void testExpandMultipleVariables()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("# {{title}}\nDate: {{date}}\nTime: {{time}}"),
            QStringLiteral("Test"));
        QVERIFY(result.startsWith(QStringLiteral("# Test\n")));
        QVERIFY(!result.contains(QStringLiteral("{{")));
    }

    void testNoVariables()
    {
        Corbomite::TemplateService service(nullptr);
        QString input = QStringLiteral("Just plain text\nNo variables here");
        QString result = service.expandTemplate(input, QStringLiteral("Note"));
        QCOMPARE(result, input);
    }

    void testAvailableTemplates()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/vault/Templates/Daily.md", "# {{title}}");
        createFile(tmp.path() + "/vault/Templates/Meeting.md", "## Meeting Notes");
        createFile(tmp.path() + "/vault/Templates/notamd.txt", "ignored");

        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");

        Corbomite::TemplateService service(&vault);
        service.setTemplateFolder(QStringLiteral("Templates"));

        auto templates = service.availableTemplates();
        QCOMPARE(templates.size(), 2);
        QVERIFY(templates.contains(QStringLiteral("Daily")));
        QVERIFY(templates.contains(QStringLiteral("Meeting")));
    }

    void testEmptyTemplateFolder()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");

        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");

        Corbomite::TemplateService service(&vault);
        service.setTemplateFolder(QStringLiteral("Templates"));

        QCOMPARE(service.availableTemplates().size(), 0);
    }

    void testLoadAndExpand()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/vault/Templates/Test.md",
                   "# {{title}}\nCreated: {{date}}");

        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");

        Corbomite::TemplateService service(&vault);
        service.setTemplateFolder(QStringLiteral("Templates"));

        QString result = service.loadAndExpand(QStringLiteral("Test"), QStringLiteral("My Note"));
        QVERIFY(result.startsWith(QStringLiteral("# My Note\n")));
        QVERIFY(!result.contains(QStringLiteral("{{")));
    }
};

QTEST_MAIN(TestTemplateService)
#include "tst_templateservice.moc"
```

- [ ] **Step 2: Implement TemplateService**

`libs/models/include/corbomite/models/TemplateService.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

namespace Corbomite {

class VaultModel;

class TemplateService : public QObject {
    Q_OBJECT

public:
    explicit TemplateService(VaultModel *vault, QObject *parent = nullptr);

    void setTemplateFolder(const QString &folder);
    QString templateFolder() const;

    void setDefaultDateFormat(const QString &format);
    void setDefaultTimeFormat(const QString &format);

    QStringList availableTemplates() const;

    QString expandTemplate(const QString &templateContent,
                           const QString &noteTitle) const;

    QString loadAndExpand(const QString &templateName,
                          const QString &noteTitle) const;

private:
    VaultModel *m_vault;
    QString m_templateFolder = QStringLiteral("Templates");
    QString m_dateFormat = QStringLiteral("yyyy-MM-dd");
    QString m_timeFormat = QStringLiteral("HH:mm");
};

} // namespace Corbomite
```

`libs/models/src/TemplateService.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/TemplateService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/storage/FileSystemAdapter.h"

#include <QDate>
#include <QDir>
#include <QRegularExpression>
#include <QTime>

namespace Corbomite {

TemplateService::TemplateService(VaultModel *vault, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
{
}

void TemplateService::setTemplateFolder(const QString &folder)
{
    m_templateFolder = folder;
}

QString TemplateService::templateFolder() const
{
    return m_templateFolder;
}

void TemplateService::setDefaultDateFormat(const QString &format)
{
    m_dateFormat = format;
}

void TemplateService::setDefaultTimeFormat(const QString &format)
{
    m_timeFormat = format;
}

QStringList TemplateService::availableTemplates() const
{
    if (!m_vault) return {};

    QString absPath = m_vault->path() + QLatin1Char('/') + m_templateFolder;
    QDir dir(absPath);
    if (!dir.exists()) return {};

    QStringList result;
    auto entries = dir.entryInfoList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
    for (const auto &fi : entries) {
        result.append(fi.completeBaseName());
    }
    return result;
}

QString TemplateService::expandTemplate(const QString &templateContent,
                                         const QString &noteTitle) const
{
    QString result = templateContent;
    QDate today = QDate::currentDate();
    QTime now = QTime::currentTime();

    // {{title}}
    result.replace(QStringLiteral("{{title}}"), noteTitle);

    // {{date}} — default format
    result.replace(QStringLiteral("{{date}}"), today.toString(m_dateFormat));

    // {{time}} — default format
    result.replace(QStringLiteral("{{time}}"), now.toString(m_timeFormat));

    // {{date:FORMAT}} — custom date format
    static const QRegularExpression datePattern(QStringLiteral(R"(\{\{date:([^}]+)\}\})"));
    auto dateIt = datePattern.globalMatch(result);
    while (dateIt.hasNext()) {
        auto match = dateIt.next();
        result.replace(match.captured(0), today.toString(match.captured(1)));
    }

    // {{time:FORMAT}} — custom time format
    static const QRegularExpression timePattern(QStringLiteral(R"(\{\{time:([^}]+)\}\})"));
    auto timeIt = timePattern.globalMatch(result);
    while (timeIt.hasNext()) {
        auto match = timeIt.next();
        result.replace(match.captured(0), now.toString(match.captured(1)));
    }

    return result;
}

QString TemplateService::loadAndExpand(const QString &templateName,
                                       const QString &noteTitle) const
{
    if (!m_vault) return {};

    QString path = m_vault->path() + QLatin1Char('/')
                 + m_templateFolder + QLatin1Char('/')
                 + templateName + QStringLiteral(".md");

    FileSystemAdapter fs;
    auto content = fs.readFile(path);
    if (!content.has_value()) return {};

    return expandTemplate(content.value(), noteTitle);
}

} // namespace Corbomite
```

- [ ] **Step 3: Update CMakeLists files**

Add `src/TemplateService.cpp` and `include/corbomite/models/TemplateService.h` to `libs/models/CMakeLists.txt`.

Add test target to `tests/models/CMakeLists.txt`:
```cmake
add_executable(tst_templateservice tst_templateservice.cpp)
add_test(NAME tst_templateservice COMMAND tst_templateservice)
target_link_libraries(tst_templateservice PRIVATE Qt6::Test Corbomite::Models Corbomite::Core Corbomite::Storage)
set_tests_properties(tst_templateservice PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 4: Build and run tests**

```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_templateservice --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add libs/models/ tests/models/tst_templateservice.cpp tests/models/CMakeLists.txt
git commit -m "feat: add TemplateService with variable expansion and template listing

Expands {{title}}, {{date}}, {{time}}, {{date:FORMAT}}, {{time:FORMAT}}.
Lists .md files from configurable template folder. 10 unit tests."
```

---

### Task 2: DailyNoteService

**Files:**
- Create: `libs/models/include/corbomite/models/DailyNoteService.h`
- Create: `libs/models/src/DailyNoteService.cpp`
- Modify: `libs/models/CMakeLists.txt`
- Create: `tests/models/tst_dailynoteservice.cpp`
- Modify: `tests/models/CMakeLists.txt`

- [ ] **Step 1: Write DailyNoteService tests**

`tests/models/tst_dailynoteservice.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDate>
#include <QDir>
#include <QFile>
#include "corbomite/models/DailyNoteService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include "corbomite/models/TemplateService.h"

class TestDailyNoteService : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

private Q_SLOTS:
    void testTodayNotePath()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");
        Corbomite::NoteService noteService(&vault);
        Corbomite::TemplateService templateService(&vault);

        Corbomite::DailyNoteService daily(&vault, &noteService, &templateService);
        daily.setDateFormat(QStringLiteral("yyyy-MM-dd"));
        daily.setFolder(QStringLiteral("Daily Notes"));

        QString expected = QStringLiteral("Daily Notes/")
            + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))
            + QStringLiteral(".md");
        QCOMPARE(daily.todayNotePath(), expected);
    }

    void testTodayNoteNotExists()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");
        Corbomite::NoteService noteService(&vault);
        Corbomite::TemplateService templateService(&vault);

        Corbomite::DailyNoteService daily(&vault, &noteService, &templateService);

        QVERIFY(!daily.todayNoteExists());
    }

    void testOpenOrCreateToday()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");
        Corbomite::NoteService noteService(&vault);
        Corbomite::TemplateService templateService(&vault);

        Corbomite::DailyNoteService daily(&vault, &noteService, &templateService);
        daily.setFolder(QStringLiteral("Daily Notes"));

        auto *doc = daily.openOrCreateToday();
        QVERIFY(doc != nullptr);
        QVERIFY(daily.todayNoteExists());

        // File should exist on disk
        QString absPath = tmp.path() + "/vault/" + daily.todayNotePath();
        QVERIFY(QFileInfo::exists(absPath));
    }

    void testOpenOrCreateWithTemplate()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/vault/Templates/Daily.md",
                   "# {{title}}\n\nDate: {{date}}\n\n## Tasks\n\n- [ ] ");

        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");
        Corbomite::NoteService noteService(&vault);
        Corbomite::TemplateService templateService(&vault);
        templateService.setTemplateFolder(QStringLiteral("Templates"));

        Corbomite::DailyNoteService daily(&vault, &noteService, &templateService);
        daily.setFolder(QStringLiteral("Daily Notes"));
        daily.setTemplateName(QStringLiteral("Daily"));

        auto *doc = daily.openOrCreateToday();
        QVERIFY(doc != nullptr);

        // Content should have expanded template
        QString content = doc->markdown();
        QVERIFY(content.contains(QStringLiteral("## Tasks")));
        QVERIFY(!content.contains(QStringLiteral("{{title}}")));
        QVERIFY(!content.contains(QStringLiteral("{{date}}")));
    }

    void testOpenExistingDoesNotModify()
    {
        QTemporaryDir tmp;
        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");
        Corbomite::NoteService noteService(&vault);
        Corbomite::TemplateService templateService(&vault);

        Corbomite::DailyNoteService daily(&vault, &noteService, &templateService);
        daily.setFolder(QStringLiteral("Daily Notes"));

        // Create today's note first
        auto *doc1 = daily.openOrCreateToday();
        QVERIFY(doc1);
        doc1->setMarkdown(QStringLiteral("My custom content"));
        noteService.saveNote(doc1);

        // Open again — should return same doc, not overwrite
        auto *doc2 = daily.openOrCreateToday();
        QCOMPARE(doc2, doc1);
        QCOMPARE(doc2->markdown(), QStringLiteral("My custom content"));
    }
};

QTEST_MAIN(TestDailyNoteService)
#include "tst_dailynoteservice.moc"
```

- [ ] **Step 2: Implement DailyNoteService**

`libs/models/include/corbomite/models/DailyNoteService.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace Corbomite {

class VaultModel;
class NoteService;
class NoteDocument;
class TemplateService;

class DailyNoteService : public QObject {
    Q_OBJECT

public:
    explicit DailyNoteService(VaultModel *vault, NoteService *noteService,
                               TemplateService *templateService, QObject *parent = nullptr);

    void setDateFormat(const QString &format);
    void setFolder(const QString &folder);
    void setTemplateName(const QString &name);

    QString todayNotePath() const;
    bool todayNoteExists() const;

    NoteDocument *openOrCreateToday();

private:
    VaultModel *m_vault;
    NoteService *m_noteService;
    TemplateService *m_templateService;
    QString m_dateFormat = QStringLiteral("yyyy-MM-dd");
    QString m_folder = QStringLiteral("Daily Notes");
    QString m_templateName;
};

} // namespace Corbomite
```

`libs/models/src/DailyNoteService.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/DailyNoteService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include "corbomite/models/TemplateService.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"

#include <QDate>

namespace Corbomite {

DailyNoteService::DailyNoteService(VaultModel *vault, NoteService *noteService,
                                     TemplateService *templateService, QObject *parent)
    : QObject(parent)
    , m_vault(vault)
    , m_noteService(noteService)
    , m_templateService(templateService)
{
}

void DailyNoteService::setDateFormat(const QString &format)
{
    m_dateFormat = format;
}

void DailyNoteService::setFolder(const QString &folder)
{
    m_folder = folder;
}

void DailyNoteService::setTemplateName(const QString &name)
{
    m_templateName = name;
}

QString DailyNoteService::todayNotePath() const
{
    QString filename = QDate::currentDate().toString(m_dateFormat);
    if (m_folder.isEmpty()) {
        return filename + QStringLiteral(".md");
    }
    return m_folder + QLatin1Char('/') + filename + QStringLiteral(".md");
}

bool DailyNoteService::todayNoteExists() const
{
    if (!m_vault) return false;
    QString absPath = m_vault->path() + QLatin1Char('/') + todayNotePath();
    return QFileInfo::exists(absPath);
}

NoteDocument *DailyNoteService::openOrCreateToday()
{
    if (!m_vault || !m_noteService) return nullptr;

    QString relPath = todayNotePath();

    // If exists, just open
    if (todayNoteExists()) {
        return m_noteService->openNote(relPath);
    }

    // Create with template if configured
    QString filename = QDate::currentDate().toString(m_dateFormat);
    auto *doc = m_noteService->createNote(filename, m_folder);
    if (!doc) return nullptr;

    if (!m_templateName.isEmpty() && m_templateService) {
        QString content = m_templateService->loadAndExpand(m_templateName, filename);
        if (!content.isEmpty()) {
            doc->setMarkdown(content);
            m_noteService->saveNote(doc);
        }
    }

    return doc;
}

} // namespace Corbomite
```

- [ ] **Step 3: Update CMakeLists, build, test, commit**

Add both to `libs/models/CMakeLists.txt`. Add test target. Build, test, commit.

```bash
git commit -m "feat: add DailyNoteService — open/create today's note with template support"
```

---

### Task 3: KConfigXT Settings + TemplatePicker + MainWindow Wiring

**Files:**
- Modify: `src/app/corbomite.kcfg`
- Create: `src/dialogs/TemplatePicker.h`
- Create: `src/dialogs/TemplatePicker.cpp`
- Modify: `src/app/MainWindow.h`
- Modify: `src/app/MainWindow.cpp`
- Modify: `src/app/corbomiteui.rc.in`
- Modify: `src/CMakeLists.txt`
- Modify: `src/dialogs/SettingsDialog.cpp`

- [ ] **Step 1: Add settings to KConfigXT schema**

Append to `src/app/corbomite.kcfg` before the closing `</kcfg>`:

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
      <label>Template to apply to new daily notes</label>
      <default></default>
    </entry>
  </group>
```

- [ ] **Step 2: Implement TemplatePicker dialog**

`src/dialogs/TemplatePicker.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDialog>
#include <QListWidget>

namespace Corbomite {

class TemplatePicker : public QDialog {
    Q_OBJECT
public:
    explicit TemplatePicker(const QStringList &templates, QWidget *parent = nullptr);
    QString selectedTemplate() const;

private:
    QListWidget *m_list;
};

} // namespace Corbomite
```

`src/dialogs/TemplatePicker.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "TemplatePicker.h"

#include <KLocalizedString>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>

namespace Corbomite {

TemplatePicker::TemplatePicker(const QStringList &templates, QWidget *parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
{
    setWindowTitle(i18n("Insert Template"));
    resize(300, 400);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(i18n("Select a template:"), this));

    m_list->addItems(templates);
    if (!templates.isEmpty()) m_list->setCurrentRow(0);
    layout->addWidget(m_list);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
}

QString TemplatePicker::selectedTemplate() const
{
    auto *item = m_list->currentItem();
    return item ? item->text() : QString();
}

} // namespace Corbomite
```

- [ ] **Step 3: Wire into MainWindow**

Add to `MainWindow.h`:
```cpp
class TemplateService;
class DailyNoteService;

// Private members:
    TemplateService *m_templateService = nullptr;
    DailyNoteService *m_dailyNoteService = nullptr;

// Private methods:
    void insertTemplate();
    void openDailyNote();
```

In `MainWindow.cpp` setupActions(), add:
```cpp
    auto *insertTpl = ac->addAction(QStringLiteral("insert_template"));
    insertTpl->setText(i18n("Insert Template"));
    insertTpl->setIcon(QIcon::fromTheme(QStringLiteral("document-new-from-template")));
    ac->setDefaultShortcut(insertTpl, QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(insertTpl, &QAction::triggered, this, &MainWindow::insertTemplate);

    auto *dailyNote = ac->addAction(QStringLiteral("open_daily_note"));
    dailyNote->setText(i18n("Open Daily Note"));
    dailyNote->setIcon(QIcon::fromTheme(QStringLiteral("view-calendar-day")));
    connect(dailyNote, &QAction::triggered, this, &MainWindow::openDailyNote);
```

In `onVaultOpened()`, create the services:
```cpp
    // Template and Daily Note services
    auto *settings = CorbomiteSettings::self();

    delete m_templateService;
    m_templateService = new TemplateService(vault, this);
    m_templateService->setTemplateFolder(settings->templateFolder());
    m_templateService->setDefaultDateFormat(settings->defaultDateFormat());
    m_templateService->setDefaultTimeFormat(settings->defaultTimeFormat());

    delete m_dailyNoteService;
    m_dailyNoteService = new DailyNoteService(vault, m_vaultService->noteService(),
                                                m_templateService, this);
    m_dailyNoteService->setDateFormat(settings->dailyNoteDateFormat());
    m_dailyNoteService->setFolder(settings->dailyNoteFolder());
    m_dailyNoteService->setTemplateName(settings->dailyNoteTemplate());
```

In `onVaultClosed()`:
```cpp
    delete m_templateService;
    m_templateService = nullptr;
    delete m_dailyNoteService;
    m_dailyNoteService = nullptr;
```

Implement `insertTemplate()`:
```cpp
void MainWindow::insertTemplate()
{
    if (!m_templateService) return;

    auto templates = m_templateService->availableTemplates();
    if (templates.isEmpty()) {
        statusBar()->showMessage(i18n("No templates found in '%1' folder",
                                       m_templateService->templateFolder()), 3000);
        return;
    }

    TemplatePicker picker(templates, this);
    if (picker.exec() != QDialog::Accepted) return;

    QString name = picker.selectedTemplate();
    if (name.isEmpty()) return;

    auto *editor = m_editorManager->activeEditor();
    if (!editor || !editor->noteDocument()) return;

    QString expanded = m_templateService->loadAndExpand(name, editor->noteDocument()->name());
    if (expanded.isEmpty()) return;

    // If note is empty, replace content; otherwise insert at cursor
    if (editor->noteDocument()->markdown().trimmed().isEmpty()) {
        editor->noteDocument()->setMarkdown(expanded);
    } else {
        editor->textCursor().insertText(expanded);
    }
}
```

Implement `openDailyNote()`:
```cpp
void MainWindow::openDailyNote()
{
    if (!m_dailyNoteService) return;

    auto *doc = m_dailyNoteService->openOrCreateToday();
    if (doc) {
        m_editorManager->openNote(doc);
    }
}
```

Add both to `updateVaultActions()`:
```cpp
    setEnabled(QStringLiteral("insert_template"), open);
    setEnabled(QStringLiteral("open_daily_note"), open);
```

- [ ] **Step 4: Update XMLGUI**

In `corbomiteui.rc.in`, add to Go menu (after graph_view):
```xml
      <Separator/>
      <Action name="open_daily_note"/>
      <Action name="insert_template"/>
```

Bump version to 7.

- [ ] **Step 5: Add TemplatePicker.cpp to src/CMakeLists.txt**

- [ ] **Step 6: Add Daily Notes settings page to SettingsDialog**

In `src/dialogs/SettingsDialog.cpp`, add a new `setupDailyNotesPage()` method:
```cpp
void SettingsDialog::setupDailyNotesPage()
{
    auto *page = new QWidget;
    auto *layout = new QFormLayout(page);
    auto *settings = CorbomiteSettings::self();

    auto *templateFolder = new QLineEdit;
    templateFolder->setText(settings->templateFolder());
    templateFolder->setObjectName(QStringLiteral("templateFolder"));
    layout->addRow(i18n("Template folder:"), templateFolder);

    auto *dailyFolder = new QLineEdit;
    dailyFolder->setText(settings->dailyNoteFolder());
    dailyFolder->setObjectName(QStringLiteral("dailyNoteFolder"));
    layout->addRow(i18n("Daily notes folder:"), dailyFolder);

    auto *dailyFormat = new QLineEdit;
    dailyFormat->setText(settings->dailyNoteDateFormat());
    dailyFormat->setObjectName(QStringLiteral("dailyNoteDateFormat"));
    layout->addRow(i18n("Daily note date format:"), dailyFormat);

    auto *dailyTemplate = new QLineEdit;
    dailyTemplate->setText(settings->dailyNoteTemplate());
    dailyTemplate->setObjectName(QStringLiteral("dailyNoteTemplate"));
    layout->addRow(i18n("Daily note template:"), dailyTemplate);

    auto item = addPage(page, i18n("Daily Notes"));
    item->setIcon(QIcon::fromTheme(QStringLiteral("view-calendar-day")));
}
```

Call it from the constructor and add settings persistence in `applySettings()`.

- [ ] **Step 7: Build, test, commit**

```bash
cmake -S . -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest --output-on-failure
git add src/ libs/models/ tests/models/
git commit -m "feat: add Templates & Daily Notes — insert template (Ctrl+T), open daily note

TemplateService: variable expansion, template listing.
DailyNoteService: open/create today's note with optional template.
TemplatePicker dialog. KConfigXT settings for folders/formats.
Settings dialog Daily Notes page. XMLGUI v7."
```

---

Self-review:

1. **Spec coverage:** TemplateService ✓ (10 tests). DailyNoteService ✓ (5 tests). Variable expansion (title, date, time, custom formats) ✓. Template listing ✓. Daily note create/open ✓. Template application on creation ✓. Settings (template folder, date/time formats, daily note config) ✓. TemplatePicker dialog ✓. MainWindow actions (Ctrl+T, daily note) ✓. Settings dialog page ✓. XMLGUI ✓.

2. **Placeholder scan:** All code complete. No TBDs.

3. **Type consistency:** `TemplateService::expandTemplate(content, title)` used consistently. `DailyNoteService::openOrCreateToday()` returns `NoteDocument*` matching `NoteService::openNote/createNote`. `CorbomiteSettings::templateFolder()` etc. match the `.kcfg` entry names.
