// SPDX-License-Identifier: GPL-3.0-or-later
// (c) 2026 Corbomite contributors, GPL-3.0-or-later.

// TODO(port-foundation-exploration): the new Markoff::Theme uses a different
// model than master's Element/ElementStyle/elements map. Entire builder body
// disabled until the theme port is tackled as a feature in its own right;
// Corbomite themes degrade to whatever the default Theme value carries.
#if 0

#include "corbomite/core/SystemThemeBuilder.h"

#include <KColorScheme>

#include <QApplication>
#include <QHash>
#include <QPalette>
#include <QString>
#include <QUuid>

namespace Corbomite::Core::SystemThemeBuilder {

namespace {

// Base layer for code-syntax token colours + callout accents. Picked once
// at build time based on background luminance.
struct BaseLayer {
    QColor codeKeyword;
    QColor codeString;
    QColor codeComment;
    QColor codeType;
    QColor codeNumLiteral;
    QColor codeBuiltIn;
    QHash<QString, QColor> calloutAccents;
    QColor calloutDefault;
};

const QHash<QString, QColor> &commonCalloutAccents() {
    // Obsidian-compatible accent palette. Same in light + dark.
    static const QHash<QString, QColor> map = {
        {"note",    QColor("#448aff")}, {"info",    QColor("#448aff")}, {"todo",    QColor("#448aff")},
        {"abstract",QColor("#00b8d4")}, {"summary", QColor("#00b8d4")}, {"tldr",    QColor("#00b8d4")},
        {"tip",     QColor("#00bfa5")}, {"hint",    QColor("#00bfa5")}, {"important",QColor("#00bfa5")},
        {"success", QColor("#00c853")}, {"check",   QColor("#00c853")}, {"done",    QColor("#00c853")},
        {"question",QColor("#ffab00")}, {"help",    QColor("#ffab00")}, {"faq",     QColor("#ffab00")},
        {"warning", QColor("#ff9100")}, {"caution", QColor("#ff9100")}, {"attention",QColor("#ff9100")},
        {"failure", QColor("#ff5252")}, {"fail",    QColor("#ff5252")}, {"missing", QColor("#ff5252")},
        {"danger",  QColor("#ff1744")}, {"error",   QColor("#ff1744")}, {"bug",     QColor("#ff1744")},
        {"example", QColor("#7c4dff")}, {"quote",   QColor("#9e9e9e")}, {"cite",    QColor("#9e9e9e")},
    };
    return map;
}

BaseLayer lightBaseLayer() {
    BaseLayer b;
    b.codeKeyword     = QColor("#0033b3");
    b.codeString      = QColor("#067d17");
    b.codeComment     = QColor("#8c8c8c");
    b.codeType        = QColor("#000080");
    b.codeNumLiteral  = QColor("#1750eb");
    b.codeBuiltIn     = QColor("#7a3e9d");
    b.calloutAccents  = commonCalloutAccents();
    b.calloutDefault  = QColor("#9e9e9e");
    return b;
}

BaseLayer darkBaseLayer() {
    BaseLayer b;
    b.codeKeyword     = QColor("#cf8e6d");
    b.codeString      = QColor("#6aab73");
    b.codeComment     = QColor("#7a7e85");
    b.codeType        = QColor("#bcbec4");
    b.codeNumLiteral  = QColor("#2aacb8");
    b.codeBuiltIn     = QColor("#c77dbb");
    b.calloutAccents  = commonCalloutAccents();
    b.calloutDefault  = QColor("#9e9e9e");
    return b;
}

} // namespace

Markoff::Theme buildFromKColorScheme(KColorSchemeManager * /*mgr*/)
{
    KColorScheme view(QPalette::Active, KColorScheme::View);
    KColorScheme sel(QPalette::Active, KColorScheme::Selection);

    const QColor bg       = view.background(KColorScheme::NormalBackground).color();
    const QColor altBg    = view.background(KColorScheme::AlternateBackground).color();
    const QColor fg       = view.foreground(KColorScheme::NormalText).color();
    const QColor inactive = view.foreground(KColorScheme::InactiveText).color();
    const QColor link     = view.foreground(KColorScheme::LinkText).color();
    const QColor neutral  = view.foreground(KColorScheme::NeutralText).color();
    const QColor negative = view.foreground(KColorScheme::NegativeText).color();
    const QColor selBg    = sel.background(KColorScheme::NormalBackground).color();
    const QColor selFg    = sel.foreground(KColorScheme::NormalText).color();

    const bool isDark = (bg.lightnessF() < 0.5);
    const BaseLayer base = isDark ? darkBaseLayer() : lightBaseLayer();

    Markoff::Theme t;
    t.name = QStringLiteral("System (%1)").arg(isDark ? QStringLiteral("Dark") : QStringLiteral("Light"));
    t.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    t.isDark = isDark;
    t.textFont = QApplication::font();
    t.codeFont = t.textFont;
    t.codeFont.setFamily(QStringLiteral("monospace"));

    auto put = [&](Markoff::Element e, std::optional<QColor> fgCol,
                   std::optional<QColor> bgCol = std::nullopt,
                   bool bold = false, int adapt = 100) {
        Markoff::ElementStyle s;
        s.fg = fgCol;
        s.bg = bgCol;
        s.bold = bold;
        s.fontSizeAdaptionPercent = adapt;
        t.elements[e] = s;
    };

    put(Markoff::Element::Text,                  fg, bg);
    put(Markoff::Element::CurrentLineBackground, std::nullopt, altBg);
    put(Markoff::Element::Selection,             selFg, selBg);
    put(Markoff::Element::Cursor,                fg);
    put(Markoff::Element::LineNumberBg,          std::nullopt, altBg);
    put(Markoff::Element::LineNumberFg,          inactive);
    put(Markoff::Element::ActiveLineNumberFg,    fg);
    put(Markoff::Element::FoldMarker,            inactive);
    put(Markoff::Element::BracketMatch,          std::nullopt, altBg);
    put(Markoff::Element::IndentGuide,           inactive);

    put(Markoff::Element::H1, fg, std::nullopt, true, 200);
    put(Markoff::Element::H2, fg, std::nullopt, true, 160);
    put(Markoff::Element::H3, fg, std::nullopt, true, 130);
    put(Markoff::Element::H4, fg, std::nullopt, true, 110);
    put(Markoff::Element::H5, fg, std::nullopt, true, 100);
    put(Markoff::Element::H6, fg, std::nullopt, true, 90);

    put(Markoff::Element::Bold, fg, std::nullopt, true);
    {
        Markoff::ElementStyle s; s.fg = fg; s.italic = true;
        t.elements[Markoff::Element::Italic] = s;
    }
    {
        Markoff::ElementStyle s; s.fg = fg; s.strike = true;
        t.elements[Markoff::Element::Strikethrough] = s;
    }
    put(Markoff::Element::InlineCode, fg, altBg);

    put(Markoff::Element::Math,        neutral);
    put(Markoff::Element::Highlight,   fg, neutral);
    put(Markoff::Element::Comment,     inactive);
    put(Markoff::Element::Tag,         neutral);
    put(Markoff::Element::FootnoteRef, link);
    put(Markoff::Element::FootnoteDef, fg);

    put(Markoff::Element::Link,       link);
    {
        Markoff::ElementStyle s; s.fg = link; s.underline = true;
        t.elements[Markoff::Element::WikiLink] = s;
    }
    {
        Markoff::ElementStyle s; s.fg = negative; s.italic = true;
        t.elements[Markoff::Element::BrokenLink] = s;
    }
    put(Markoff::Element::Image,      link);
    put(Markoff::Element::Embed,      link);

    put(Markoff::Element::CodeBlock,        fg, altBg);
    put(Markoff::Element::BlockQuote,       fg, altBg);
    put(Markoff::Element::HorizontalRule,   inactive);
    put(Markoff::Element::ListMarker,       neutral);
    put(Markoff::Element::Table,            fg);
    put(Markoff::Element::FrontmatterBlock, inactive, altBg);
    put(Markoff::Element::Callout,          fg, altBg);
    put(Markoff::Element::HtmlBlock,        inactive);
    put(Markoff::Element::HtmlInline,       inactive);

    put(Markoff::Element::CheckboxUnchecked, fg);
    put(Markoff::Element::CheckboxChecked,   inactive);

    put(Markoff::Element::CodeKeyword,    base.codeKeyword, std::nullopt, true);
    put(Markoff::Element::CodeString,     base.codeString);
    {
        Markoff::ElementStyle s; s.fg = base.codeComment; s.italic = true;
        t.elements[Markoff::Element::CodeComment] = s;
    }
    put(Markoff::Element::CodeType,       base.codeType);
    put(Markoff::Element::CodeNumLiteral, base.codeNumLiteral);
    put(Markoff::Element::CodeBuiltIn,    base.codeBuiltIn);
    put(Markoff::Element::CodeOther,      fg);

    put(Markoff::Element::MaskedSyntax,   inactive);
    put(Markoff::Element::TrailingSpace,  std::nullopt, negative);

    t.paint.codeBlockBg              = altBg;
    t.paint.codeBlockBorder          = inactive;
    t.paint.codeBlockLanguageLabel   = inactive;
    t.paint.searchMatchBg            = neutral;
    t.paint.searchCurrentMatchBg     = link;
    t.paint.checkboxCheckedFill      = fg;
    t.paint.checkboxCheckMark        = bg;
    t.paint.checkboxUncheckedOutline = fg;
    t.paint.imagePlaceholderBg       = altBg;
    t.paint.imagePlaceholderBorder   = inactive;
    t.paint.imagePlaceholderText     = inactive;
    t.paint.blockSelectionOverlay    = selBg;
    t.paint.calloutAccents           = base.calloutAccents;
    t.paint.calloutDefault           = base.calloutDefault;

    return t;
}

} // namespace Corbomite::Core::SystemThemeBuilder

#endif // 0 — disabled pending theme port (Markoff::Theme shape change)

// Minimal stub OUTSIDE the #if 0 so downstream linkers can resolve.
#include "corbomite/core/SystemThemeBuilder.h"
namespace Corbomite::Core::SystemThemeBuilder {
Markoff::Theme buildFromKColorScheme(KColorSchemeManager *) { return {}; }
}  // namespace Corbomite::Core::SystemThemeBuilder
