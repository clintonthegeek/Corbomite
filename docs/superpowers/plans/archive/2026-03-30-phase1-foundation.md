# Phase 1: Foundation & Core Editor — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a functioning KDE note editor that opens Obsidian vaults, browses files, edits markdown in tabs, autosaves, watches for external changes, and persists settings and session state.

**Architecture:** Monorepo with three static libraries (core, storage, models) plus an application shell. KateMDI sidebar framework adapted from Kate (GPLv3). QMarkdownTextEdit as git submodule for the editor widget. KConfigXT for settings. PlanStan-pattern CMake with dev/release build isolation.

**Tech Stack:** C++20, Qt 6.8+, KDE Frameworks 6 (KXmlGui, KConfig, KI18n, KCoreAddons, KWidgetsAddons, KIconThemes, KDBusAddons), CMake 3.19+, ECM 6.0+

**Reference sources:**
- Kate MDI framework: `~/src/kde/src/kate/apps/lib/katemdi.h/cpp`
- Kate tab bar: `~/src/kde/src/kate/apps/lib/katetabbar.h/cpp`
- Kate splitter: `~/src/kde/src/kate/apps/lib/katesplitter.h/cpp`
- PlanStan CMake: `~/dev/PlanStan/CMakeLists.txt`
- KDE design guide: `docs/kde-power-software-design-guide/`
- Phase 1 design spec: `docs/superpowers/specs/2026-03-30-phase1-foundation-design.md`

---

### Task 1: Project Scaffold & Build System

**Files:**
- Create: `CMakeLists.txt`
- Create: `.gitignore`
- Create: `CLAUDE.md`
- Create: `data/org.corbomite.Corbomite.desktop`

- [ ] **Step 1: Create .gitignore**

```gitignore
# Build directories
build/
build-*/
cmake-build-*/

# IDE
.idea/
.vscode/
*.swp
*~
compile_commands.json

# Generated
*.moc
moc_*.cpp
ui_*.h
qrc_*.cpp
*.autosave

# Corbomite runtime
.corbomite/

# OS
.DS_Store
Thumbs.db
```

- [ ] **Step 2: Create root CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite VERSION 0.1.0 LANGUAGES CXX)

option(CORBOMITE_DEV_BUILD "Build with dev identity (isolated config/data, [Dev] suffix)" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets DBus)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)

qt_standard_project_setup()

# Extra CMake Modules (for KDE macros)
find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)

# KDE Frameworks dependencies
find_package(KF6CoreAddons REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(KF6XmlGui REQUIRED)
find_package(KF6WidgetsAddons REQUIRED)
find_package(KF6IconThemes REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6ConfigWidgets REQUIRED)
find_package(KF6DBusAddons REQUIRED)

# Libraries
add_subdirectory(libs/core)
add_subdirectory(libs/storage)
add_subdirectory(libs/models)
add_subdirectory(libs/qmarkdowntextedit)

# Application
add_subdirectory(src)

qt_add_executable(Corbomite WIN32 MACOSX_BUNDLE src/app/main.cpp)

if(CORBOMITE_DEV_BUILD)
    target_compile_definitions(Corbomite PRIVATE CORBOMITE_DEV_BUILD)
    target_compile_definitions(CorbomiteApp PRIVATE CORBOMITE_DEV_BUILD)
    set(_xmlgui_prefix "/kxmlgui5/corbomite-dev")
    set(CORBOMITE_COMPONENT_NAME "corbomite-dev")
else()
    set(_xmlgui_prefix "/kxmlgui5/corbomite")
    set(CORBOMITE_COMPONENT_NAME "corbomite")
endif()

set(_xmlgui_rcfile "${CORBOMITE_COMPONENT_NAME}ui.rc")

configure_file(src/app/corbomiteui.rc.in
               ${CMAKE_CURRENT_BINARY_DIR}/${_xmlgui_rcfile} @ONLY)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${_xmlgui_rcfile}
    PROPERTIES QT_RESOURCE_ALIAS ${_xmlgui_rcfile})

qt_add_resources(Corbomite "xmlgui"
    PREFIX "${_xmlgui_prefix}"
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${_xmlgui_rcfile}
)

target_link_libraries(Corbomite
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::DBus
        KF6::CoreAddons
        KF6::I18n
        KF6::XmlGui
        KF6::WidgetsAddons
        KF6::IconThemes
        KF6::ConfigCore
        KF6::ConfigWidgets
        KF6::DBusAddons
        CorbomiteApp
)

target_include_directories(Corbomite PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Install
include(GNUInstallDirs)

install(TARGETS Corbomite CorbomiteApp
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_xmlgui_rcfile}
        DESTINATION ${KDE_INSTALL_KXMLGUIDIR}/${CORBOMITE_COMPONENT_NAME})

install(FILES data/org.corbomite.Corbomite.desktop DESTINATION ${KDE_INSTALL_APPDIR})

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_generate_deploy_app_script(
        TARGET Corbomite
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

# Tests
enable_testing()
add_subdirectory(tests/core)
add_subdirectory(tests/storage)
add_subdirectory(tests/models)
add_subdirectory(tests/integration)
```

- [ ] **Step 3: Create desktop file**

`data/org.corbomite.Corbomite.desktop`:
```ini
[Desktop Entry]
Name=Corbomite
GenericName=Knowledge Manager
Comment=A native Qt6/KDE Obsidian-inspired note-taking application
Exec=Corbomite %F
Icon=accessories-text-editor
Type=Application
Terminal=false
Categories=Office;TextEditor;
Keywords=notes;markdown;knowledge;vault;wiki;
StartupNotify=true
X-KDE-StartupNotify=true
```

- [ ] **Step 4: Create CLAUDE.md**

```markdown
## Building

Configure and build with the dev build flag:

```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON
cmake --build build
```

Run:
```bash
./build/Corbomite
```

Run tests:
```bash
cd build && ctest --output-on-failure
```

### Dev Build Isolation

Always configure with `-DCORBOMITE_DEV_BUILD=ON` so dev builds use isolated config/data directories and don't interfere with any installed release version.

| | Release | Dev (`-DCORBOMITE_DEV_BUILD=ON`) |
|---|---|---|
| Config | `~/.config/corbomiterc` | `~/.config/corbomite-devrc` |
| Data | `~/.local/share/corbomite/` | `~/.local/share/corbomite-dev/` |
| Window title | "Corbomite" | "Corbomite [Dev]" |

## Library Structure

| Library | Target | Purpose |
|---------|--------|---------|
| `libs/core` | `Corbomite::Core` | Domain types: NoteMeta, NoteDocument |
| `libs/storage` | `Corbomite::Storage` | File I/O: FileSystemAdapter, VaultScanner |
| `libs/models` | `Corbomite::Models` | Qt item models: VaultModel, NotesTreeModel, TabModel |
| `libs/qmarkdowntextedit` | `qmarkdowntextedit` | Markdown editor widget (git submodule) |

## Testing

Tests define **expected behavior**. When a test fails, fix the code, not the test.

Run a single test:
```bash
cd build && ctest -R tst_notemeta --output-on-failure
```

## Code Conventions

- C++20, Qt6/KDE Frameworks 6
- Use `i18n()` for all user-visible strings
- Use `QIcon::fromTheme()` for all icons
- Use `KStandardAction` where applicable
- GPLv3 license
```

- [ ] **Step 5: Create stub directories and placeholder files so CMake can find subdirectories**

Create these empty `CMakeLists.txt` files as placeholders (they will be filled in subsequent tasks):

`libs/core/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(corbomite-core VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(corbomite-core STATIC
    src/NoteMeta.cpp
)
set_target_properties(corbomite-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Core ALIAS corbomite-core)

target_include_directories(corbomite-core
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-core PUBLIC Qt6::Core)
```

`libs/storage/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(corbomite-storage VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(corbomite-storage STATIC
    src/FileSystemAdapter.cpp
)
set_target_properties(corbomite-storage PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Storage ALIAS corbomite-storage)

target_include_directories(corbomite-storage
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-storage PUBLIC Qt6::Core)

if(NOT PROJECT_IS_TOP_LEVEL)
    target_link_libraries(corbomite-storage PRIVATE Corbomite::Core)
    target_include_directories(corbomite-storage PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/core/include
    )
endif()
```

`libs/models/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(corbomite-models VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

add_library(corbomite-models STATIC
    src/TabModel.cpp
)
set_target_properties(corbomite-models PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Models ALIAS corbomite-models)

target_include_directories(corbomite-models
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-models PUBLIC Qt6::Core Qt6::Widgets)

if(NOT PROJECT_IS_TOP_LEVEL)
    target_link_libraries(corbomite-models PRIVATE Corbomite::Core Corbomite::Storage)
    target_include_directories(corbomite-models PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/core/include
        ${CMAKE_SOURCE_DIR}/libs/storage/include
    )
endif()
```

`libs/qmarkdowntextedit/CMakeLists.txt`:
```cmake
# This is the upstream CMakeLists.txt from the submodule.
# It builds the qmarkdowntextedit library.
# No changes needed — the submodule provides this.
```

Create stub source files so the project compiles:

`libs/core/include/corbomite/core/NoteMeta.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Corbomite Contributors

#pragma once

#include <QString>

namespace Corbomite {

struct NoteMeta {
    QString relativePath;
};

} // namespace Corbomite
```

`libs/core/src/NoteMeta.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/NoteMeta.h"
```

`libs/storage/include/corbomite/storage/FileSystemAdapter.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Corbomite {
class FileSystemAdapter {};
} // namespace Corbomite
```

`libs/storage/src/FileSystemAdapter.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/FileSystemAdapter.h"
```

`libs/models/include/corbomite/models/TabModel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Corbomite {
class TabModel {};
} // namespace Corbomite
```

`libs/models/src/TabModel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/TabModel.h"
```

Create the minimal app shell stubs (filled in properly in later tasks):

`src/CMakeLists.txt`:
```cmake
include(KDECMakeSettings)
kconfig_add_kcfg_files(CORBOMITE_KCFG_SRCS app/corbomitesettings.kcfgc GENERATE_MOC)

add_library(CorbomiteApp STATIC
    ${CORBOMITE_KCFG_SRCS}
    app/CorbomiteApp.cpp
)

target_link_libraries(CorbomiteApp
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::DBus
        KF6::CoreAddons
        KF6::I18n
        KF6::XmlGui
        KF6::WidgetsAddons
        KF6::IconThemes
        KF6::ConfigCore
        KF6::ConfigWidgets
        KF6::DBusAddons
        Corbomite::Core
        Corbomite::Storage
        Corbomite::Models
        qmarkdowntextedit
)

target_include_directories(CorbomiteApp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_BINARY_DIR}
)
```

`src/app/corbomite.kcfg`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<kcfg xmlns="http://www.kde.org/standards/kcfg/1.0"
      xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
      xsi:schemaLocation="http://www.kde.org/standards/kcfg/1.0
                          http://www.kde.org/standards/kcfg/1.0/kcfg.xsd">
  <kcfgfile/>

  <group name="Editor">
    <entry name="FontSize" type="Int">
      <label>Editor font size in points</label>
      <default>16</default>
      <min>6</min>
      <max>72</max>
    </entry>
    <entry name="TabSize" type="Int">
      <label>Tab width in spaces</label>
      <default>4</default>
      <min>1</min>
      <max>8</max>
    </entry>
    <entry name="LineNumbers" type="Bool">
      <label>Show line numbers</label>
      <default>false</default>
    </entry>
    <entry name="LineWrap" type="Bool">
      <label>Wrap long lines</label>
      <default>true</default>
    </entry>
    <entry name="AutoSaveDelayMs" type="Int">
      <label>Autosave delay in milliseconds</label>
      <default>2000</default>
      <min>500</min>
      <max>30000</max>
    </entry>
  </group>

  <group name="Files">
    <entry name="TrashOption" type="String">
      <label>Delete behavior: system, vault, or permanent</label>
      <default>system</default>
    </entry>
    <entry name="PromptDelete" type="Bool">
      <label>Show confirmation before deleting notes</label>
      <default>true</default>
    </entry>
  </group>

  <group name="Appearance">
    <entry name="Theme" type="String">
      <label>Color theme: light, dark, or system</label>
      <default>system</default>
    </entry>
  </group>
</kcfg>
```

`src/app/corbomitesettings.kcfgc`:
```ini
File=corbomite.kcfg
ClassName=CorbomiteSettings
Mutators=true
Singleton=true
GenerateProperties=true
ParentInConstructor=true
```

`src/app/corbomiteui.rc.in`:
```xml
<!DOCTYPE gui SYSTEM "kpartgui.dtd">
<gui name="@CORBOMITE_COMPONENT_NAME@" version="1">
  <MenuBar>
    <Menu name="file">
      <text>&amp;File</text>
      <Action name="file_new_note"/>
      <Separator/>
      <Action name="file_save"/>
      <Separator/>
    </Menu>
    <Menu name="view">
      <text>&amp;View</text>
      <Action name="view_toggle_left_sidebar"/>
      <Separator/>
      <Action name="view_zoom_in"/>
      <Action name="view_zoom_out"/>
      <Action name="view_zoom_reset"/>
    </Menu>
  </MenuBar>
  <ToolBar name="mainToolBar" noMerge="1">
    <text>Main Toolbar</text>
    <Action name="file_new_note"/>
    <Action name="file_save"/>
  </ToolBar>
</gui>
```

`src/app/CorbomiteApp.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

namespace Corbomite {

class CorbomiteApp : public QObject {
    Q_OBJECT
public:
    explicit CorbomiteApp(QObject *parent = nullptr);
};

} // namespace Corbomite
```

`src/app/CorbomiteApp.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CorbomiteApp.h"

namespace Corbomite {

CorbomiteApp::CorbomiteApp(QObject *parent)
    : QObject(parent)
{
}

} // namespace Corbomite
```

`src/app/main.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <KAboutData>
#include <KLocalizedString>
#include <KDBusService>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("corbomite");

#ifdef CORBOMITE_DEV_BUILD
    const auto componentName = QStringLiteral("corbomite-dev");
    const auto displayName = i18n("Corbomite [Dev]");
    const auto desktopFile = QStringLiteral("org.corbomite.Corbomite.Dev");
#else
    const auto componentName = QStringLiteral("corbomite");
    const auto displayName = i18n("Corbomite");
    const auto desktopFile = QStringLiteral("org.corbomite.Corbomite");
#endif

    KAboutData aboutData(
        componentName,
        displayName,
        QStringLiteral("0.1.0"),
        i18n("A native Obsidian-inspired knowledge management application"),
        KAboutLicense::GPL_V3,
        i18n("(c) 2026 Corbomite Contributors"),
        QString(),
        QString()
    );
    aboutData.setOrganizationDomain("corbomite.org");
    aboutData.setDesktopFileName(desktopFile);

    KAboutData::setApplicationData(aboutData);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));

    KDBusService service(KDBusService::Unique);

    return app.exec();
}
```

Create empty test stubs:

`tests/core/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_CoreTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
```

`tests/storage/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_StorageTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
```

`tests/models/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_ModelTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
```

`tests/integration/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_IntegrationTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
```

- [ ] **Step 6: Add qmarkdowntextedit as git submodule**

Run:
```bash
cd /home/clinton/dev/Corbomite
git submodule add https://github.com/pbek/qmarkdowntextedit.git libs/qmarkdowntextedit
```

- [ ] **Step 7: Verify the project configures and builds**

Run:
```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON
cmake --build build
```

Expected: Configures and builds successfully. The `Corbomite` executable exists at `build/Corbomite`. Running it should show a blank window briefly then exit (no main window created yet).

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "feat: project scaffold with CMake, KDE frameworks, and library structure

- Root CMakeLists with Qt6/KF6/ECM, dev/release build isolation
- Three library stubs: core, storage, models
- qmarkdowntextedit as git submodule
- KConfigXT settings schema
- XMLGUI menu/toolbar definition
- Application bootstrap with KAboutData, KDBusService
- Desktop file, CLAUDE.md, .gitignore
- Empty test directories"
```

---

### Task 2: NoteMeta & NoteDocument (libs/core)

**Files:**
- Create: `libs/core/include/corbomite/core/NoteMeta.h`
- Create: `libs/core/include/corbomite/core/NoteDocument.h`
- Create: `libs/core/src/NoteMeta.cpp`
- Create: `libs/core/src/NoteDocument.cpp`
- Modify: `libs/core/CMakeLists.txt`
- Create: `tests/core/tst_notemeta.cpp`
- Create: `tests/core/tst_notedocument.cpp`
- Modify: `tests/core/CMakeLists.txt`

- [ ] **Step 1: Write NoteMeta tests**

`tests/core/tst_notemeta.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/NoteMeta.h"

class TestNoteMeta : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testFromFileInfo()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create a test file
        QString vaultRoot = tmpDir.path();
        QString filePath = vaultRoot + "/subfolder/my-note.md";
        QDir().mkpath(vaultRoot + "/subfolder");
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# Hello\n\nSome content.");
        f.close();

        QFileInfo fi(filePath);
        auto meta = Corbomite::NoteMeta::fromFileInfo(fi, vaultRoot);

        QCOMPARE(meta.relativePath, QStringLiteral("subfolder/my-note.md"));
        QCOMPARE(meta.name, QStringLiteral("my-note"));
        QVERIFY(meta.modified.isValid());
        QVERIFY(meta.sizeBytes > 0);
    }

    void testNameStripsExtension()
    {
        Corbomite::NoteMeta meta;
        meta.relativePath = QStringLiteral("folder/My Note.md");

        QCOMPARE(meta.nameFromPath(), QStringLiteral("My Note"));
    }

    void testNameStripsCanvasExtension()
    {
        Corbomite::NoteMeta meta;
        meta.relativePath = QStringLiteral("canvas/Brainstorm.canvas");

        QCOMPARE(meta.nameFromPath(), QStringLiteral("Brainstorm"));
    }

    void testAbsolutePath()
    {
        Corbomite::NoteMeta meta;
        meta.relativePath = QStringLiteral("subfolder/note.md");

        QString abs = meta.absolutePath(QStringLiteral("/home/user/vault"));
        QCOMPARE(abs, QStringLiteral("/home/user/vault/subfolder/note.md"));
    }

    void testPathNormalization()
    {
        // Backslashes should be normalized to forward slashes
        auto meta = Corbomite::NoteMeta::fromRelativePath(QStringLiteral("folder\\note.md"));
        QCOMPARE(meta.relativePath, QStringLiteral("folder/note.md"));
    }

    void testNoLeadingSlash()
    {
        auto meta = Corbomite::NoteMeta::fromRelativePath(QStringLiteral("/folder/note.md"));
        QCOMPARE(meta.relativePath, QStringLiteral("folder/note.md"));
    }
};

QTEST_MAIN(TestNoteMeta)
#include "tst_notemeta.moc"
```

- [ ] **Step 2: Write NoteDocument tests**

`tests/core/tst_notedocument.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include "corbomite/core/NoteDocument.h"

class TestNoteDocument : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testInitialState()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));
        QCOMPARE(doc.relativePath(), QStringLiteral("note.md"));
        QCOMPARE(doc.name(), QStringLiteral("note"));
        QCOMPARE(doc.markdown(), QString());
        QVERIFY(!doc.isModified());
    }

    void testSetMarkdownEmitsTextChanged()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));
        QSignalSpy spy(&doc, &Corbomite::NoteDocument::textChanged);

        doc.setMarkdown(QStringLiteral("# Hello"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(doc.markdown(), QStringLiteral("# Hello"));
    }

    void testModifiedStateTracking()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));
        QSignalSpy spy(&doc, &Corbomite::NoteDocument::modificationChanged);

        QVERIFY(!doc.isModified());

        doc.setMarkdown(QStringLiteral("changed"));
        QVERIFY(doc.isModified());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);

        doc.setModified(false);
        QVERIFY(!doc.isModified());
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }

    void testWordCount()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));

        QCOMPARE(doc.wordCount(), 0);

        doc.setMarkdown(QStringLiteral("hello world"));
        QCOMPARE(doc.wordCount(), 2);

        doc.setMarkdown(QStringLiteral("one two three four five"));
        QCOMPARE(doc.wordCount(), 5);
    }

    void testWordCountHandlesMarkdown()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));

        doc.setMarkdown(QStringLiteral("# Heading\n\nSome **bold** text."));
        // Words: Heading, Some, bold, text = 4
        // # and ** are not words
        QCOMPARE(doc.wordCount(), 4);
    }

    void testCharacterCount()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));

        QCOMPARE(doc.characterCount(), 0);

        doc.setMarkdown(QStringLiteral("hello"));
        QCOMPARE(doc.characterCount(), 5);
    }

    void testEmptyDocument()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QString());

        QCOMPARE(doc.wordCount(), 0);
        QCOMPARE(doc.characterCount(), 0);
    }

    void testFilePath()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/home/user/vault"), QStringLiteral("folder/note.md"));

        QCOMPARE(doc.filePath(), QStringLiteral("/home/user/vault/folder/note.md"));
        QCOMPARE(doc.relativePath(), QStringLiteral("folder/note.md"));
        QCOMPARE(doc.name(), QStringLiteral("note"));
    }
};

QTEST_MAIN(TestNoteDocument)
#include "tst_notedocument.moc"
```

- [ ] **Step 3: Update test CMakeLists to build the tests**

`tests/core/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_CoreTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_notemeta tst_notemeta.cpp)
add_test(NAME tst_notemeta COMMAND tst_notemeta)
target_link_libraries(tst_notemeta PRIVATE Qt6::Test Corbomite::Core)

add_executable(tst_notedocument tst_notedocument.cpp)
add_test(NAME tst_notedocument COMMAND tst_notedocument)
target_link_libraries(tst_notedocument PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_notedocument PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 4: Run tests to verify they fail**

Run:
```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R "tst_notemeta|tst_notedocument" --output-on-failure
```

Expected: Build fails because NoteMeta and NoteDocument don't have the required methods yet.

- [ ] **Step 5: Implement NoteMeta**

`libs/core/include/corbomite/core/NoteMeta.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QFileInfo>
#include <QString>

namespace Corbomite {

struct NoteMeta {
    QString relativePath;
    QDateTime modified;
    qint64 sizeBytes = 0;

    QString nameFromPath() const;
    QString absolutePath(const QString &vaultRoot) const;

    static NoteMeta fromFileInfo(const QFileInfo &fi, const QString &vaultRoot);
    static NoteMeta fromRelativePath(const QString &path);
};

} // namespace Corbomite
```

`libs/core/src/NoteMeta.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/NoteMeta.h"
#include <QDir>

namespace Corbomite {

QString NoteMeta::nameFromPath() const
{
    QString fileName = relativePath.mid(relativePath.lastIndexOf(QLatin1Char('/')) + 1);
    int dotPos = fileName.lastIndexOf(QLatin1Char('.'));
    if (dotPos > 0) {
        return fileName.left(dotPos);
    }
    return fileName;
}

QString NoteMeta::absolutePath(const QString &vaultRoot) const
{
    return vaultRoot + QLatin1Char('/') + relativePath;
}

NoteMeta NoteMeta::fromFileInfo(const QFileInfo &fi, const QString &vaultRoot)
{
    NoteMeta meta;
    QDir vault(vaultRoot);
    meta.relativePath = vault.relativeFilePath(fi.absoluteFilePath());
    meta.relativePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    meta.modified = fi.lastModified();
    meta.sizeBytes = fi.size();
    return meta;
}

NoteMeta NoteMeta::fromRelativePath(const QString &path)
{
    NoteMeta meta;
    meta.relativePath = path;
    meta.relativePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (meta.relativePath.startsWith(QLatin1Char('/'))) {
        meta.relativePath = meta.relativePath.mid(1);
    }
    return meta;
}

} // namespace Corbomite
```

- [ ] **Step 6: Implement NoteDocument**

`libs/core/include/corbomite/core/NoteDocument.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QTextDocument>

namespace Corbomite {

class NoteDocument : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool modified READ isModified NOTIFY modificationChanged)

public:
    explicit NoteDocument(const QString &vaultRoot, const QString &relativePath,
                          QObject *parent = nullptr);

    QString filePath() const;
    QString relativePath() const;
    QString name() const;

    QString markdown() const;
    void setMarkdown(const QString &text);

    bool isModified() const;
    void setModified(bool modified);

    int wordCount() const;
    int characterCount() const;

Q_SIGNALS:
    void textChanged();
    void modificationChanged(bool modified);
    void saved();

private:
    QString m_vaultRoot;
    QString m_relativePath;
    QString m_markdown;
    bool m_modified = false;
    mutable int m_cachedWordCount = -1;
};

} // namespace Corbomite
```

`libs/core/src/NoteDocument.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/NoteDocument.h"
#include <QRegularExpression>

namespace Corbomite {

NoteDocument::NoteDocument(const QString &vaultRoot, const QString &relativePath,
                           QObject *parent)
    : QObject(parent)
    , m_vaultRoot(vaultRoot)
    , m_relativePath(relativePath)
{
}

QString NoteDocument::filePath() const
{
    return m_vaultRoot + QLatin1Char('/') + m_relativePath;
}

QString NoteDocument::relativePath() const
{
    return m_relativePath;
}

QString NoteDocument::name() const
{
    QString fileName = m_relativePath.mid(m_relativePath.lastIndexOf(QLatin1Char('/')) + 1);
    int dotPos = fileName.lastIndexOf(QLatin1Char('.'));
    if (dotPos > 0) {
        return fileName.left(dotPos);
    }
    return fileName;
}

QString NoteDocument::markdown() const
{
    return m_markdown;
}

void NoteDocument::setMarkdown(const QString &text)
{
    if (m_markdown == text) {
        return;
    }
    m_markdown = text;
    m_cachedWordCount = -1; // Invalidate cache

    if (!m_modified) {
        m_modified = true;
        Q_EMIT modificationChanged(true);
    }

    Q_EMIT textChanged();
}

bool NoteDocument::isModified() const
{
    return m_modified;
}

void NoteDocument::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        Q_EMIT modificationChanged(modified);
    }
}

int NoteDocument::wordCount() const
{
    if (m_cachedWordCount >= 0) {
        return m_cachedWordCount;
    }

    if (m_markdown.isEmpty()) {
        m_cachedWordCount = 0;
        return 0;
    }

    // Strip markdown syntax characters, then count word-like tokens
    static const QRegularExpression wordPattern(QStringLiteral(R"(\b[a-zA-Z0-9]+(?:[-'][a-zA-Z0-9]+)*\b)"));

    int count = 0;
    auto it = wordPattern.globalMatch(m_markdown);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    m_cachedWordCount = count;
    return count;
}

int NoteDocument::characterCount() const
{
    return m_markdown.length();
}

} // namespace Corbomite
```

- [ ] **Step 7: Update libs/core/CMakeLists.txt with both source files**

```cmake
cmake_minimum_required(VERSION 3.19)
project(corbomite-core VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(corbomite-core STATIC
    src/NoteMeta.cpp
    src/NoteDocument.cpp
)
set_target_properties(corbomite-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Core ALIAS corbomite-core)

target_include_directories(corbomite-core
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-core PUBLIC Qt6::Core)
```

- [ ] **Step 8: Build and run tests**

Run:
```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R "tst_notemeta|tst_notedocument" --output-on-failure
```

Expected: All tests PASS.

- [ ] **Step 9: Commit**

```bash
git add libs/core/ tests/core/
git commit -m "feat: add NoteMeta and NoteDocument domain types with tests

NoteMeta: lightweight metadata struct with path normalization,
name extraction, and factory methods from QFileInfo.

NoteDocument: QObject owning note content with modified state
tracking, word/character count, and change signals."
```

---

### Task 3: FileSystemAdapter & VaultScanner (libs/storage)

**Files:**
- Create: `libs/storage/include/corbomite/storage/FileSystemAdapter.h`
- Create: `libs/storage/include/corbomite/storage/VaultScanner.h`
- Create: `libs/storage/src/FileSystemAdapter.cpp`
- Create: `libs/storage/src/VaultScanner.cpp`
- Modify: `libs/storage/CMakeLists.txt`
- Create: `tests/storage/tst_filesystemadapter.cpp`
- Create: `tests/storage/tst_vaultscanner.cpp`
- Modify: `tests/storage/CMakeLists.txt`

- [ ] **Step 1: Write FileSystemAdapter tests**

`tests/storage/tst_filesystemadapter.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include "corbomite/storage/FileSystemAdapter.h"

class TestFileSystemAdapter : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testWriteAndReadRoundTrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString path = tmp.path() + "/test.md";
        QVERIFY(fs.writeFile(path, QStringLiteral("# Hello\n\nWorld")));

        auto result = fs.readFile(path);
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), QStringLiteral("# Hello\n\nWorld"));
    }

    void testReadNonexistent()
    {
        Corbomite::FileSystemAdapter fs;
        auto result = fs.readFile(QStringLiteral("/nonexistent/path/file.md"));
        QVERIFY(!result.has_value());
    }

    void testWriteCreatesIntermediateDirs()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString path = tmp.path() + "/a/b/c/deep.md";
        QVERIFY(fs.writeFile(path, QStringLiteral("deep content")));

        auto result = fs.readFile(path);
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), QStringLiteral("deep content"));
    }

    void testUtf8RoundTrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString content = QString::fromUtf8(u8"日本語テスト 🎉 café résumé");
        QString path = tmp.path() + "/utf8.md";
        QVERIFY(fs.writeFile(path, content));

        auto result = fs.readFile(path);
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), content);
    }

    void testRename()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString oldPath = tmp.path() + "/old.md";
        QString newPath = tmp.path() + "/new.md";
        fs.writeFile(oldPath, QStringLiteral("content"));

        QVERIFY(fs.rename(oldPath, newPath));
        QVERIFY(!fs.exists(oldPath));
        QVERIFY(fs.exists(newPath));

        auto result = fs.readFile(newPath);
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), QStringLiteral("content"));
    }

    void testRemove()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString path = tmp.path() + "/doomed.md";
        fs.writeFile(path, QStringLiteral("bye"));

        QVERIFY(fs.exists(path));
        QVERIFY(fs.remove(path));
        QVERIFY(!fs.exists(path));
    }

    void testExistsReturnsFalseForMissing()
    {
        Corbomite::FileSystemAdapter fs;
        QVERIFY(!fs.exists(QStringLiteral("/surely/not/here.md")));
    }

    void testWriteEmptyFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString path = tmp.path() + "/empty.md";
        QVERIFY(fs.writeFile(path, QString()));

        auto result = fs.readFile(path);
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), QString());
    }
};

QTEST_MAIN(TestFileSystemAdapter)
#include "tst_filesystemadapter.moc"
```

- [ ] **Step 2: Write VaultScanner tests**

`tests/storage/tst_vaultscanner.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "corbomite/storage/VaultScanner.h"

class TestVaultScanner : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content = QStringLiteral("test"))
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

private Q_SLOTS:
    void testScanFindsMarkdownFiles()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note1.md");
        createFile(tmp.path() + "/note2.md");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 2);
    }

    void testScanFindsNestedFiles()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/top.md");
        createFile(tmp.path() + "/folder/nested.md");
        createFile(tmp.path() + "/folder/deep/deeper.md");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 3);
    }

    void testScanFindsCanvasFiles()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note.md");
        createFile(tmp.path() + "/board.canvas");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 2);
    }

    void testScanExcludesObsidianDir()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note.md");
        createFile(tmp.path() + "/.obsidian/app.json");
        createFile(tmp.path() + "/.obsidian/plugins/test/main.js");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 1);
        QCOMPARE(results.at(0).relativePath, QStringLiteral("note.md"));
    }

    void testScanExcludesGitDir()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note.md");
        createFile(tmp.path() + "/.git/HEAD");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 1);
    }

    void testScanExcludesCorbomiteDir()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note.md");
        createFile(tmp.path() + "/.corbomite/index.sqlite");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 1);
    }

    void testScanExcludesNonNoteFiles()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note.md");
        createFile(tmp.path() + "/image.png");
        createFile(tmp.path() + "/data.json");
        createFile(tmp.path() + "/readme.txt");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        // Only .md and .canvas are included
        QCOMPARE(results.size(), 1);
    }

    void testScanEmptyVault()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 0);
    }

    void testScanRelativePathsCorrect()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/folder/note.md");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 1);
        QCOMPARE(results.at(0).relativePath, QStringLiteral("folder/note.md"));
    }
};

QTEST_MAIN(TestVaultScanner)
#include "tst_vaultscanner.moc"
```

- [ ] **Step 3: Run tests to verify they fail**

Run:
```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -20
```

Expected: Build fails — FileSystemAdapter and VaultScanner not implemented.

- [ ] **Step 4: Implement FileSystemAdapter**

`libs/storage/include/corbomite/storage/FileSystemAdapter.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <optional>

namespace Corbomite {

class FileSystemAdapter {
public:
    std::optional<QString> readFile(const QString &absolutePath) const;
    bool writeFile(const QString &absolutePath, const QString &content);
    bool rename(const QString &oldPath, const QString &newPath);
    bool remove(const QString &absolutePath);
    bool moveToTrash(const QString &absolutePath);
    bool exists(const QString &absolutePath) const;
    bool mkpath(const QString &dirPath);
};

} // namespace Corbomite
```

`libs/storage/src/FileSystemAdapter.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/FileSystemAdapter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace Corbomite {

std::optional<QString> FileSystemAdapter::readFile(const QString &absolutePath) const
{
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    return QString::fromUtf8(file.readAll());
}

bool FileSystemAdapter::writeFile(const QString &absolutePath, const QString &content)
{
    // Ensure parent directory exists
    QFileInfo fi(absolutePath);
    QDir().mkpath(fi.absolutePath());

    QSaveFile file(absolutePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(content.toUtf8());
    return file.commit();
}

bool FileSystemAdapter::rename(const QString &oldPath, const QString &newPath)
{
    // Ensure target directory exists
    QFileInfo fi(newPath);
    QDir().mkpath(fi.absolutePath());

    return QFile::rename(oldPath, newPath);
}

bool FileSystemAdapter::remove(const QString &absolutePath)
{
    return QFile::remove(absolutePath);
}

bool FileSystemAdapter::moveToTrash(const QString &absolutePath)
{
    return QFile::moveToTrash(absolutePath);
}

bool FileSystemAdapter::exists(const QString &absolutePath) const
{
    return QFileInfo::exists(absolutePath);
}

bool FileSystemAdapter::mkpath(const QString &dirPath)
{
    return QDir().mkpath(dirPath);
}

} // namespace Corbomite
```

- [ ] **Step 5: Implement VaultScanner**

`libs/storage/include/corbomite/storage/VaultScanner.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/NoteMeta.h"
#include <QVector>
#include <QString>

namespace Corbomite {

class VaultScanner {
public:
    QVector<NoteMeta> scan(const QString &vaultRoot) const;

private:
    bool shouldExcludeDir(const QString &dirName) const;
    bool isNoteFile(const QString &suffix) const;
};

} // namespace Corbomite
```

`libs/storage/src/VaultScanner.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/VaultScanner.h"
#include <QDirIterator>

namespace Corbomite {

QVector<NoteMeta> VaultScanner::scan(const QString &vaultRoot) const
{
    QVector<NoteMeta> results;

    QDirIterator it(vaultRoot, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();

        // Check if any parent directory should be excluded
        QString relPath = QDir(vaultRoot).relativeFilePath(fi.absoluteFilePath());
        bool excluded = false;
        const auto parts = relPath.split(QLatin1Char('/'));
        for (const auto &part : parts) {
            if (shouldExcludeDir(part)) {
                excluded = true;
                break;
            }
        }
        if (excluded) {
            continue;
        }

        if (!isNoteFile(fi.suffix())) {
            continue;
        }

        results.append(NoteMeta::fromFileInfo(fi, vaultRoot));
    }

    return results;
}

bool VaultScanner::shouldExcludeDir(const QString &dirName) const
{
    return dirName == QLatin1String(".obsidian")
        || dirName == QLatin1String(".corbomite")
        || dirName == QLatin1String(".git")
        || dirName == QLatin1String(".trash")
        || dirName == QLatin1String("node_modules");
}

bool VaultScanner::isNoteFile(const QString &suffix) const
{
    return suffix == QLatin1String("md") || suffix == QLatin1String("canvas");
}

} // namespace Corbomite
```

- [ ] **Step 6: Update libs/storage/CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.19)
project(corbomite-storage VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(corbomite-storage STATIC
    src/FileSystemAdapter.cpp
    src/VaultScanner.cpp
)
set_target_properties(corbomite-storage PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Storage ALIAS corbomite-storage)

target_include_directories(corbomite-storage
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-storage PUBLIC Qt6::Core)

if(NOT PROJECT_IS_TOP_LEVEL)
    target_link_libraries(corbomite-storage PUBLIC Corbomite::Core)
endif()
```

- [ ] **Step 7: Update tests/storage/CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_StorageTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_filesystemadapter tst_filesystemadapter.cpp)
add_test(NAME tst_filesystemadapter COMMAND tst_filesystemadapter)
target_link_libraries(tst_filesystemadapter PRIVATE Qt6::Test Corbomite::Storage)

add_executable(tst_vaultscanner tst_vaultscanner.cpp)
add_test(NAME tst_vaultscanner COMMAND tst_vaultscanner)
target_link_libraries(tst_vaultscanner PRIVATE Qt6::Test Corbomite::Storage Corbomite::Core)
```

- [ ] **Step 8: Build and run tests**

Run:
```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R "tst_filesystemadapter|tst_vaultscanner" --output-on-failure
```

Expected: All tests PASS.

- [ ] **Step 9: Commit**

```bash
git add libs/storage/ tests/storage/
git commit -m "feat: add FileSystemAdapter and VaultScanner with tests

FileSystemAdapter: thin file I/O abstraction using QSaveFile for
atomic writes, std::optional for fallible reads.

VaultScanner: recursive vault directory scanner that finds .md
and .canvas files, excluding .obsidian, .corbomite, .git dirs."
```

---

### Task 4: TabModel (libs/models)

**Files:**
- Create: `libs/models/include/corbomite/models/TabModel.h`
- Create: `libs/models/src/TabModel.cpp`
- Modify: `libs/models/CMakeLists.txt`
- Create: `tests/models/tst_tabmodel.cpp`
- Modify: `tests/models/CMakeLists.txt`

- [ ] **Step 1: Write TabModel tests**

`tests/models/tst_tabmodel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include "corbomite/models/TabModel.h"

class TestTabModel : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testInitiallyEmpty()
    {
        Corbomite::TabModel model;
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.activeTabIndex(), -1);
    }

    void testOpenTabAddsRow()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("note.md"));

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.activeTabIndex(), 0);
    }

    void testOpenDuplicateActivatesExisting()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("note.md"));
        model.openTab(QStringLiteral("other.md"));
        model.openTab(QStringLiteral("note.md")); // duplicate

        QCOMPARE(model.rowCount(), 2); // not 3
        QCOMPARE(model.activeTabIndex(), 0); // back to first
    }

    void testCloseTab()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));

        model.closeTab(0);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.tabPath(0), QStringLiteral("b.md"));
    }

    void testCloseOtherTabs()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));
        model.openTab(QStringLiteral("c.md"));

        model.setActiveTab(1); // b.md active
        model.closeOtherTabs(1);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.tabPath(0), QStringLiteral("b.md"));
    }

    void testCloseOtherKeepsPinned()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));
        model.openTab(QStringLiteral("c.md"));
        model.pinTab(0, true); // pin a.md

        model.setActiveTab(1);
        model.closeOtherTabs(1);

        QCOMPARE(model.rowCount(), 2); // a.md (pinned) + b.md (active)
    }

    void testReopenLastClosed()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));

        model.closeTab(1); // close b.md
        QCOMPARE(model.rowCount(), 1);

        model.reopenLastClosed();
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.tabPath(1), QStringLiteral("b.md"));
    }

    void testLruOrdering()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md")); // lru=1
        model.openTab(QStringLiteral("b.md")); // lru=2
        model.openTab(QStringLiteral("c.md")); // lru=3

        // Activate a.md → a becomes most recent
        model.setActiveTab(0); // a.md lru=4

        auto lru = model.lruSortedPaths();
        // Most recent first: a, c, b
        QCOMPARE(lru.at(0), QStringLiteral("a.md"));
        QCOMPARE(lru.at(1), QStringLiteral("c.md"));
        QCOMPARE(lru.at(2), QStringLiteral("b.md"));
    }

    void testPinTab()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));

        QVERIFY(!model.isPinned(0));
        model.pinTab(0, true);
        QVERIFY(model.isPinned(0));
    }

    void testDirtyState()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));

        QVERIFY(!model.isDirty(0));
        model.setDirty(0, true);
        QVERIFY(model.isDirty(0));
    }

    void testMoveTab()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));
        model.openTab(QStringLiteral("c.md"));

        model.moveTab(0, 2); // move a.md to position 2

        QCOMPARE(model.tabPath(0), QStringLiteral("b.md"));
        QCOMPARE(model.tabPath(1), QStringLiteral("c.md"));
        QCOMPARE(model.tabPath(2), QStringLiteral("a.md"));
    }

    void testUpdateNotePath()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("old.md"));

        model.updateNotePath(QStringLiteral("old.md"), QStringLiteral("new.md"));

        QCOMPARE(model.tabPath(0), QStringLiteral("new.md"));
    }

    void testCloseAllTabs()
    {
        Corbomite::TabModel model;
        model.openTab(QStringLiteral("a.md"));
        model.openTab(QStringLiteral("b.md"));

        model.closeAllTabs();

        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.activeTabIndex(), -1);
    }
};

QTEST_MAIN(TestTabModel)
#include "tst_tabmodel.moc"
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build 2>&1 | tail -20
```

Expected: Build fails — TabModel not implemented.

- [ ] **Step 3: Implement TabModel**

`libs/models/include/corbomite/models/TabModel.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace Corbomite {

struct TabState {
    QString notePath;
    int scrollPosition = 0;
    int cursorLine = 0;
    int cursorColumn = 0;
    bool isPinned = false;
    bool isDirty = false;
    quint64 lruCounter = 0;
};

class TabModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        NotePathRole = Qt::UserRole + 1,
        TitleRole,
        IsPinnedRole,
        IsDirtyRole
    };

    explicit TabModel(QObject *parent = nullptr);

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Tab operations
    void openTab(const QString &notePath, bool activate = true);
    void closeTab(int index);
    void closeOtherTabs(int keepIndex);
    void closeAllTabs();
    void pinTab(int index, bool pinned);
    void moveTab(int fromIndex, int toIndex);
    void setActiveTab(int index);
    int activeTabIndex() const;

    // Query
    QString tabPath(int index) const;
    bool isPinned(int index) const;
    bool isDirty(int index) const;
    void setDirty(int index, bool dirty);

    // Rename support
    void updateNotePath(const QString &oldPath, const QString &newPath);

    // LRU navigation
    QStringList lruSortedPaths() const;

    // Closed tab history
    void reopenLastClosed();

Q_SIGNALS:
    void activeTabChanged(int index);

private:
    int findTab(const QString &notePath) const;

    QVector<TabState> m_tabs;
    int m_activeIndex = -1;
    quint64 m_lruCounter = 0;
    QVector<TabState> m_closedHistory;
};

} // namespace Corbomite
```

`libs/models/src/TabModel.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/models/TabModel.h"
#include <algorithm>

namespace Corbomite {

TabModel::TabModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TabModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_tabs.size());
}

QVariant TabModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_tabs.size()) return {};

    const auto &tab = m_tabs.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole: {
        QString name = tab.notePath.mid(tab.notePath.lastIndexOf(QLatin1Char('/')) + 1);
        int dot = name.lastIndexOf(QLatin1Char('.'));
        return dot > 0 ? name.left(dot) : name;
    }
    case NotePathRole: return tab.notePath;
    case IsPinnedRole: return tab.isPinned;
    case IsDirtyRole: return tab.isDirty;
    }
    return {};
}

QHash<int, QByteArray> TabModel::roleNames() const
{
    return {
        {NotePathRole, "notePath"},
        {TitleRole, "title"},
        {IsPinnedRole, "isPinned"},
        {IsDirtyRole, "isDirty"}
    };
}

void TabModel::openTab(const QString &notePath, bool activate)
{
    int existing = findTab(notePath);
    if (existing >= 0) {
        if (activate) setActiveTab(existing);
        return;
    }

    int row = m_tabs.size();
    beginInsertRows(QModelIndex(), row, row);
    TabState tab;
    tab.notePath = notePath;
    tab.lruCounter = ++m_lruCounter;
    m_tabs.append(tab);
    endInsertRows();

    if (activate) setActiveTab(row);
}

void TabModel::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;

    m_closedHistory.append(m_tabs.at(index));

    beginRemoveRows(QModelIndex(), index, index);
    m_tabs.removeAt(index);
    endRemoveRows();

    if (m_tabs.isEmpty()) {
        m_activeIndex = -1;
    } else if (m_activeIndex >= m_tabs.size()) {
        m_activeIndex = m_tabs.size() - 1;
    } else if (m_activeIndex > index) {
        --m_activeIndex;
    }
    Q_EMIT activeTabChanged(m_activeIndex);
}

void TabModel::closeOtherTabs(int keepIndex)
{
    // Close from end to avoid index shifting issues
    for (int i = m_tabs.size() - 1; i >= 0; --i) {
        if (i == keepIndex) continue;
        if (m_tabs.at(i).isPinned) continue;
        closeTab(i);
        if (keepIndex > i) --keepIndex;
    }
}

void TabModel::closeAllTabs()
{
    beginResetModel();
    m_tabs.clear();
    m_activeIndex = -1;
    endResetModel();
    Q_EMIT activeTabChanged(-1);
}

void TabModel::pinTab(int index, bool pinned)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs[index].isPinned = pinned;
    Q_EMIT dataChanged(this->index(index), this->index(index), {IsPinnedRole});
}

void TabModel::moveTab(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_tabs.size()) return;
    if (toIndex < 0 || toIndex >= m_tabs.size()) return;
    if (fromIndex == toIndex) return;

    // beginMoveRows needs special handling
    int destRow = toIndex > fromIndex ? toIndex + 1 : toIndex;
    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), destRow);

    TabState tab = m_tabs.takeAt(fromIndex);
    m_tabs.insert(toIndex, tab);

    endMoveRows();

    // Update active index
    if (m_activeIndex == fromIndex) {
        m_activeIndex = toIndex;
    } else if (fromIndex < m_activeIndex && toIndex >= m_activeIndex) {
        --m_activeIndex;
    } else if (fromIndex > m_activeIndex && toIndex <= m_activeIndex) {
        ++m_activeIndex;
    }
}

void TabModel::setActiveTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_activeIndex = index;
    m_tabs[index].lruCounter = ++m_lruCounter;
    Q_EMIT activeTabChanged(index);
}

int TabModel::activeTabIndex() const
{
    return m_activeIndex;
}

QString TabModel::tabPath(int index) const
{
    if (index < 0 || index >= m_tabs.size()) return {};
    return m_tabs.at(index).notePath;
}

bool TabModel::isPinned(int index) const
{
    if (index < 0 || index >= m_tabs.size()) return false;
    return m_tabs.at(index).isPinned;
}

bool TabModel::isDirty(int index) const
{
    if (index < 0 || index >= m_tabs.size()) return false;
    return m_tabs.at(index).isDirty;
}

void TabModel::setDirty(int index, bool dirty)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs[index].isDirty = dirty;
    Q_EMIT dataChanged(this->index(index), this->index(index), {IsDirtyRole});
}

void TabModel::updateNotePath(const QString &oldPath, const QString &newPath)
{
    int idx = findTab(oldPath);
    if (idx < 0) return;
    m_tabs[idx].notePath = newPath;
    Q_EMIT dataChanged(index(idx), index(idx), {NotePathRole, TitleRole});
}

QStringList TabModel::lruSortedPaths() const
{
    auto sorted = m_tabs;
    std::sort(sorted.begin(), sorted.end(), [](const TabState &a, const TabState &b) {
        return a.lruCounter > b.lruCounter; // Most recent first
    });

    QStringList result;
    result.reserve(sorted.size());
    for (const auto &tab : sorted) {
        result.append(tab.notePath);
    }
    return result;
}

void TabModel::reopenLastClosed()
{
    if (m_closedHistory.isEmpty()) return;
    TabState tab = m_closedHistory.takeLast();
    openTab(tab.notePath);
}

int TabModel::findTab(const QString &notePath) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs.at(i).notePath == notePath) return i;
    }
    return -1;
}

} // namespace Corbomite
```

- [ ] **Step 4: Update CMakeLists files**

`libs/models/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(corbomite-models VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

add_library(corbomite-models STATIC
    src/TabModel.cpp
)
set_target_properties(corbomite-models PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Models ALIAS corbomite-models)

target_include_directories(corbomite-models
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-models PUBLIC Qt6::Core Qt6::Widgets)

if(NOT PROJECT_IS_TOP_LEVEL)
    target_link_libraries(corbomite-models PUBLIC Corbomite::Core Corbomite::Storage)
endif()
```

`tests/models/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.19)
project(Corbomite_ModelTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_tabmodel tst_tabmodel.cpp)
add_test(NAME tst_tabmodel COMMAND tst_tabmodel)
target_link_libraries(tst_tabmodel PRIVATE Qt6::Test Corbomite::Models)
set_tests_properties(tst_tabmodel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 5: Build and run tests**

Run:
```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
cd build && ctest -R tst_tabmodel --output-on-failure
```

Expected: All tests PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/models/ tests/models/
git commit -m "feat: add TabModel with LRU tracking, pin, dirty state, and tests

QAbstractListModel for managing open editor tabs. Features:
- LRU counter for Ctrl+Tab navigation (Kate pattern)
- Pin/unpin tabs
- Dirty state tracking
- Closed tab history with reopen
- Tab reordering and note path rename support"
```

---

### Task 5: MainWindow with KateMDI Sidebar Framework

This is the largest task — adapt Kate's MDI framework and create the main window.

**Files:**
- Create: `src/mdi/CorbomiteMDI.h`
- Create: `src/mdi/CorbomiteMDI.cpp`
- Modify: `src/app/MainWindow.h` (create)
- Modify: `src/app/MainWindow.cpp` (create)
- Modify: `src/app/main.cpp`
- Modify: `src/CMakeLists.txt`

**Reference:** `~/src/kde/src/kate/apps/lib/katemdi.h/cpp` (GPLv3). Copy and adapt — rename `KateMDI::` to `CorbomiteMDI::`, remove all `KTextEditor::Plugin` references, remove Kate-specific includes. Replace `KTextEditor::MainWindow::ToolViewPosition` with our own enum.

- [ ] **Step 1: Copy and adapt KateMDI from Kate source**

This step requires careful adaptation of `~/src/kde/src/kate/apps/lib/katemdi.h` and `~/src/kde/src/kate/apps/lib/katemdi.cpp`. The key changes:

1. Rename namespace `KateMDI` → `CorbomiteMDI`
2. Remove all `KTextEditor::Plugin` references — replace `KTextEditor::Plugin *plugin` member in ToolView with `QObject *plugin` (or remove entirely since we have no plugins in Phase 1)
3. Remove `KTextEditor::MainWindow::ToolViewPosition` — define our own `ToolViewPosition` enum
4. Remove `#include` of Kate-specific headers
5. Remove `sigShowPluginConfigPage` signal (or keep with `QObject *` instead of `KTextEditor::Plugin *`)
6. In `createToolView()`, change first parameter from `KTextEditor::Plugin *` to `QObject *` (nullable)

The agent implementing this task should:
- Read `~/src/kde/src/kate/apps/lib/katemdi.h` (full file)
- Read `~/src/kde/src/kate/apps/lib/katemdi.cpp` (full file)
- Copy to `src/mdi/CorbomiteMDI.h` and `src/mdi/CorbomiteMDI.cpp`
- Apply the renaming and removals listed above
- Ensure it compiles against KF6 (KParts::MainWindow, KMultiTabBar, KConfigGroup, etc.)

- [ ] **Step 2: Create MainWindow**

`src/app/MainWindow.h`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mdi/CorbomiteMDI.h"
#include <KActionCollection>

namespace Corbomite {

class MainWindow : public CorbomiteMDI::MainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupActions();
    void setupSidebars();
    void setupStatusBar();
    void setupCentralWidget();

    // Action slots
    void createNewNote();

    // Central widget placeholder
    QWidget *m_editorArea = nullptr;

    // Status bar widgets
    QLabel *m_wordCountLabel = nullptr;
    QLabel *m_cursorPosLabel = nullptr;
};

} // namespace Corbomite
```

`src/app/MainWindow.cpp`:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"
#include "corbomitesettings.h"

#include <KLocalizedString>
#include <KStandardAction>
#include <KActionCollection>
#include <QLabel>
#include <QStatusBar>
#include <QVBoxLayout>

namespace Corbomite {

MainWindow::MainWindow(QWidget *parent)
    : CorbomiteMDI::MainWindow(parent)
{
#ifdef CORBOMITE_DEV_BUILD
    setObjectName(QStringLiteral("CorbomiteDevMainWindow"));
    setComponentName(QStringLiteral("corbomite-dev"), i18n("Corbomite [Dev]"));
#else
    setObjectName(QStringLiteral("CorbomiteMainWindow"));
    setComponentName(QStringLiteral("corbomite"), i18n("Corbomite"));
#endif

    setupActions();
    setupCentralWidget();
    setupSidebars();
    setupStatusBar();
    setupGUI(ToolBar | Keys | StatusBar | Save);

    resize(1200, 800);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupActions()
{
    KActionCollection *ac = actionCollection();

    // Standard actions
    KStandardAction::quit(qApp, &QApplication::quit, ac);
    KStandardAction::preferences(this, [this]() {
        // Settings dialog will be added in a later task
    }, ac);

    // File actions
    auto *newNote = ac->addAction(QStringLiteral("file_new_note"));
    newNote->setText(i18n("New Note"));
    newNote->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    ac->setDefaultShortcut(newNote, QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(newNote, &QAction::triggered, this, &MainWindow::createNewNote);

    auto *save = ac->addAction(QStringLiteral("file_save"));
    save->setText(i18n("Save"));
    save->setIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    ac->setDefaultShortcut(save, QKeySequence(Qt::CTRL | Qt::Key_S));

    // View actions
    auto *toggleLeftSidebar = ac->addAction(QStringLiteral("view_toggle_left_sidebar"));
    toggleLeftSidebar->setText(i18n("Toggle Left Sidebar"));
    ac->setDefaultShortcut(toggleLeftSidebar, QKeySequence(Qt::CTRL | Qt::Key_Backslash));
    connect(toggleLeftSidebar, &QAction::triggered, this, [this]() {
        setSidebarsVisible(!sidebarsVisible());
    });

    auto *zoomIn = ac->addAction(QStringLiteral("view_zoom_in"));
    zoomIn->setText(i18n("Zoom In"));
    ac->setDefaultShortcut(zoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal));

    auto *zoomOut = ac->addAction(QStringLiteral("view_zoom_out"));
    zoomOut->setText(i18n("Zoom Out"));
    ac->setDefaultShortcut(zoomOut, QKeySequence(Qt::CTRL | Qt::Key_Minus));

    auto *zoomReset = ac->addAction(QStringLiteral("view_zoom_reset"));
    zoomReset->setText(i18n("Reset Zoom"));
    ac->setDefaultShortcut(zoomReset, QKeySequence(Qt::CTRL | Qt::Key_0));
}

void MainWindow::setupCentralWidget()
{
    m_editorArea = new QWidget(this);
    auto *layout = new QVBoxLayout(m_editorArea);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *placeholder = new QLabel(i18n("Open a vault to begin"), m_editorArea);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);

    setCentralWidget(m_editorArea);
}

void MainWindow::setupSidebars()
{
    // Left sidebar: File Explorer (placeholder for now)
    auto *fileExplorer = createToolView(
        nullptr,
        QStringLiteral("file_explorer"),
        KMultiTabBar::Left,
        QIcon::fromTheme(QStringLiteral("folder")),
        i18n("Files")
    );

    auto *explorerLabel = new QLabel(i18n("File Explorer"), fileExplorer);
    explorerLabel->setAlignment(Qt::AlignCenter);
}

void MainWindow::setupStatusBar()
{
    m_wordCountLabel = new QLabel(i18n("0 words"), this);
    m_cursorPosLabel = new QLabel(i18n("Ln 1, Col 1"), this);

    statusBar()->addPermanentWidget(m_wordCountLabel);
    statusBar()->addPermanentWidget(m_cursorPosLabel);
}

void MainWindow::createNewNote()
{
    // Will be implemented when NoteService is integrated
    statusBar()->showMessage(i18n("New note (not yet implemented)"), 3000);
}

} // namespace Corbomite
```

- [ ] **Step 3: Update main.cpp to show MainWindow**

Replace the `main.cpp` to create and show the MainWindow:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <KAboutData>
#include <KLocalizedString>
#include <KDBusService>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("corbomite");

#ifdef CORBOMITE_DEV_BUILD
    const auto componentName = QStringLiteral("corbomite-dev");
    const auto displayName = i18n("Corbomite [Dev]");
    const auto desktopFile = QStringLiteral("org.corbomite.Corbomite.Dev");
#else
    const auto componentName = QStringLiteral("corbomite");
    const auto displayName = i18n("Corbomite");
    const auto desktopFile = QStringLiteral("org.corbomite.Corbomite");
#endif

    KAboutData aboutData(
        componentName,
        displayName,
        QStringLiteral("0.1.0"),
        i18n("A native Obsidian-inspired knowledge management application"),
        KAboutLicense::GPL_V3,
        i18n("(c) 2026 Corbomite Contributors"),
        QString(),
        QString()
    );
    aboutData.setOrganizationDomain("corbomite.org");
    aboutData.setDesktopFileName(desktopFile);
    KAboutData::setApplicationData(aboutData);

    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));

    KDBusService service(KDBusService::Unique);

    Corbomite::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
```

- [ ] **Step 4: Update src/CMakeLists.txt with new files**

Add the MDI and MainWindow sources to the `CorbomiteApp` library:

```cmake
include(KDECMakeSettings)
kconfig_add_kcfg_files(CORBOMITE_KCFG_SRCS app/corbomitesettings.kcfgc GENERATE_MOC)

add_library(CorbomiteApp STATIC
    ${CORBOMITE_KCFG_SRCS}
    app/CorbomiteApp.cpp
    app/MainWindow.cpp
    mdi/CorbomiteMDI.cpp
)

target_link_libraries(CorbomiteApp
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::DBus
        KF6::CoreAddons
        KF6::I18n
        KF6::XmlGui
        KF6::WidgetsAddons
        KF6::IconThemes
        KF6::ConfigCore
        KF6::ConfigWidgets
        KF6::DBusAddons
        KF6::Parts
        Corbomite::Core
        Corbomite::Storage
        Corbomite::Models
        qmarkdowntextedit
)

target_include_directories(CorbomiteApp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/mdi
    ${CMAKE_CURRENT_BINARY_DIR}
)
```

Note: Add `KF6::Parts` to the root CMakeLists.txt `find_package` list:
```cmake
find_package(KF6Parts REQUIRED)
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cmake -B build -DCORBOMITE_DEV_BUILD=ON && cmake --build build
./build/Corbomite
```

Expected: Application launches showing a main window with:
- KDE menu bar (File, View, Settings, Help)
- Left sidebar with "Files" toolview
- Central area with "Open a vault to begin" placeholder
- Status bar with word count and cursor position

- [ ] **Step 6: Commit**

```bash
git add src/ CMakeLists.txt
git commit -m "feat: add MainWindow with KateMDI sidebar framework

Adapted Kate's katemdi.h/cpp (GPLv3) as CorbomiteMDI namespace.
MainWindow with KXmlGui integration, left sidebar with file
explorer toolview placeholder, status bar, and basic actions
(new note, save, zoom, sidebar toggle)."
```

---

### Task 6: Remaining tasks (outlined — full code in subsequent plan iterations)

The following tasks complete Phase 1 but are outlined here for scope. Each will follow the same TDD pattern with full code when the plan is executed:

**Task 6: NoteEditorWidget** — Wrap QMarkdownTextEdit, connect to NoteDocument, emit cursor/word count signals.

**Task 7: EditorViewManager + EditorViewSpace** — Central widget with tab bar (adapted from Kate's KateTabBar) and stacked widget. Tab operations wired to TabModel.

**Task 8: VaultModel + NotesTreeModel** — VaultModel wrapping VaultScanner with document cache. NotesTreeModel as QAbstractItemModel for file explorer tree.

**Task 9: NoteService + VaultService** — Service layer orchestrating note CRUD and vault lifecycle. Wire to MainWindow actions.

**Task 10: FileExplorerPanel** — QTreeView in left sidebar bound to NotesTreeModel. Context menu, inline rename, drag-drop.

**Task 11: AutosaveReactor + FileWatchReactor** — Debounced autosave on document modification. QFileSystemWatcher with suppression for own writes.

**Task 12: SettingsDialog** — KPageDialog with Editor/Files/Appearance pages bound to KConfigXT settings.

**Task 13: Session Management** — Save/restore to `.corbomite/session.json`. Window geometry, sidebar state, open tabs with cursor positions.

**Task 14: Integration Tests** — tst_vault_lifecycle, tst_editor_save, tst_filewatch, tst_session.

Each task follows the same pattern: write failing tests → implement → verify tests pass → commit.
