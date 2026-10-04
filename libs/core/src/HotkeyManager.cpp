// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/HotkeyManager.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace Corbomite {

void HotkeyManager::addDefaultHotkeys(const QString &commandId,
                                      const QList<Hotkey> &bindings)
{
    m_defaults.insert(commandId, bindings);
}

void HotkeyManager::removeDefaultHotkeys(const QString &commandId)
{
    m_defaults.remove(commandId);
}

QList<Hotkey> HotkeyManager::defaultHotkeys(const QString &commandId) const
{
    return m_defaults.value(commandId);
}

std::optional<QList<Hotkey>> HotkeyManager::customHotkeys(const QString &commandId) const
{
    const auto it = m_custom.constFind(commandId);
    if (it == m_custom.constEnd()) return std::nullopt;
    return it.value();
}

void HotkeyManager::setCustomHotkeys(const QString &commandId,
                                    const QList<Hotkey> &bindings)
{
    if (!m_custom.contains(commandId)) m_customOrder.append(commandId);
    m_custom.insert(commandId, bindings);
}

void HotkeyManager::removeCustomHotkeys(const QString &commandId)
{
    m_custom.remove(commandId);
    m_customOrder.removeAll(commandId);
}

bool HotkeyManager::load(const QByteArray &json)
{
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;

    const auto parsed = HotkeyFile::parse(json);
    m_custom = parsed.bindings;
    m_customOrder = parsed.order;
    return true;
}

QByteArray HotkeyManager::save() const
{
    HotkeyFile f;
    f.order = m_customOrder;
    f.bindings = m_custom;
    return f.serialise();
}

QList<Hotkey> HotkeyManager::effectiveHotkeys(const QString &commandId) const
{
    const auto custom = customHotkeys(commandId);
    if (custom) return *custom;
    return defaultHotkeys(commandId);
}

QStringList HotkeyManager::overriddenIds() const
{
    QStringList ids;
    for (const auto &id : m_customOrder) {
        if (m_custom.contains(id)) ids.append(id);
    }
    return ids;
}

namespace {

Hotkey bind(std::initializer_list<HotkeyModifier> mods, const QString &key)
{
    Hotkey h;
    h.modifiers = QList<HotkeyModifier>(mods);
    h.key = key;
    return h;
}

constexpr HotkeyModifier Mod = HotkeyModifier::Mod;
constexpr HotkeyModifier Ctrl = HotkeyModifier::Ctrl;
constexpr HotkeyModifier Meta = HotkeyModifier::Meta;
constexpr HotkeyModifier Shift = HotkeyModifier::Shift;
constexpr HotkeyModifier Alt = HotkeyModifier::Alt;

} // namespace

QHash<QString, QList<Hotkey>> obsidianDefaultHotkeys(bool macOs)
{
    QHash<QString, QList<Hotkey>> t;

    // App.registerCommands — app/own/2075-Vie.js
    t.insert(QStringLiteral("app:go-back"), {bind({Mod, Alt}, "ArrowLeft")});
    t.insert(QStringLiteral("app:go-forward"), {bind({Mod, Alt}, "ArrowRight")});
    t.insert(QStringLiteral("app:open-settings"), {bind({Mod}, ",")});
    t.insert(QStringLiteral("markdown:toggle-preview"), {bind({Mod}, "E")});
    t.insert(QStringLiteral("markdown:add-metadata-property"), {bind({Mod}, ";")});
    t.insert(QStringLiteral("app:open-help"), {bind({}, "F1")});
    t.insert(QStringLiteral("file-explorer:new-file"), {bind({Mod}, "N")});
    t.insert(QStringLiteral("file-explorer:new-file-in-new-pane"), {bind({Mod, Shift}, "N")});
    t.insert(QStringLiteral("editor:open-search"), {bind({Mod}, "F")});
    t.insert(QStringLiteral("editor:open-search-replace"),
             macOs ? QList<Hotkey>{bind({Mod, Alt}, "F")}
                   : QList<Hotkey>{bind({Mod}, "H")});
    t.insert(QStringLiteral("editor:insert-link"), {bind({Mod}, "K")});
    t.insert(QStringLiteral("editor:toggle-bold"), {bind({Mod}, "B")});
    t.insert(QStringLiteral("editor:toggle-italics"), {bind({Mod}, "I")});
    t.insert(QStringLiteral("editor:toggle-comments"), {bind({Mod}, "/")});
    t.insert(QStringLiteral("editor:toggle-checklist-status"), {bind({Mod}, "l")});
    t.insert(QStringLiteral("editor:delete-paragraph"), {bind({Mod}, "D")});

    // Workspace — app/own/1932-r4.js
    t.insert(QStringLiteral("editor:save-file"), {bind({Mod}, "S")});
    t.insert(QStringLiteral("editor:follow-link"), {bind({Alt}, "Enter")});
    t.insert(QStringLiteral("editor:open-link-in-new-leaf"), {bind({Mod}, "Enter")});
    t.insert(QStringLiteral("editor:open-link-in-new-window"), {bind({Mod, Alt, Shift}, "Enter")});
    t.insert(QStringLiteral("editor:open-link-in-new-split"), {bind({Mod, Alt}, "Enter")});
    t.insert(QStringLiteral("workspace:edit-file-title"), {bind({}, "F2")});
    t.insert(QStringLiteral("workspace:undo-close-pane"), {bind({Mod, Shift}, "T")});
    t.insert(QStringLiteral("workspace:next-tab"),
             macOs ? QList<Hotkey>{bind({Ctrl}, "Tab"), bind({Meta, Shift}, "]")}
                   : QList<Hotkey>{bind({Ctrl}, "Tab"), bind({Ctrl}, "PageDown")});
    for (int n = 1; n <= 8; ++n) {
        t.insert(QStringLiteral("workspace:goto-tab-%1").arg(n),
                 {bind({Mod}, QString::number(n))});
    }
    t.insert(QStringLiteral("workspace:goto-last-tab"), {bind({Mod}, "9")});
    t.insert(QStringLiteral("workspace:previous-tab"),
             macOs ? QList<Hotkey>{bind({Ctrl, Shift}, "Tab"), bind({Meta, Shift}, "[")}
                   : QList<Hotkey>{bind({Ctrl, Shift}, "Tab"), bind({Ctrl}, "PageUp")});
    t.insert(QStringLiteral("workspace:new-tab"), {bind({Mod}, "T")});
    t.insert(QStringLiteral("workspace:close"), {bind({Mod}, "W")});
    t.insert(QStringLiteral("workspace:close-window"), {bind({Mod, Shift}, "W")});

    // Core plugins — `registerGlobalCommand` feeds `app.commands`, whose
    // `addCommand` is what calls `addDefaultHotkeys` (app/own/1963-s5.js:22).
    // These four are the only core-plugin commands in 1.13.7 that carry a
    // `hotkeys:` field, and all four belong to `defaultOn` plugins, so this is
    // the stock-vault set. Divergence: Obsidian only registers a plugin's
    // commands once the plugin is *enabled* (app/own/1939-m4.js:560), so
    // disabling one of these four drops the default there. This table has no
    // enabled-plugin input, so Corbomite keeps the binding — it can only ever
    // add a default Obsidian would have dropped, never remove a live one.
    t.insert(QStringLiteral("global-search:open"), {bind({Mod, Shift}, "F")}); // 1848-Y1.js:226
    t.insert(QStringLiteral("graph:open"), {bind({Mod}, "G")});               // 1848-Y1.js:762
    t.insert(QStringLiteral("command-palette:open"), {bind({Mod}, "P")});     // 2032-E9.js:12
    t.insert(QStringLiteral("switcher:open"), {bind({Mod}, "O")});            // 2040-Vee.js:2966

    return t;
}

} // namespace Corbomite
