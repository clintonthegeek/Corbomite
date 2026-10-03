# JKQTMathText

LaTeX math formula rendering for Qt6, extracted from [JKQtPlotter](https://github.com/jkriege2/JKQtPlotter).

## ⚠ Version NOT pinned

The upstream commit / release this extraction came from is **not recorded
anywhere in this repository**, so this component cannot be refreshed or audited
against its origin. `libs/mmdr/README.md` documents the same class of problem
for the mermaid renderer, and does at least state it plainly; this one was
silently unpinned.

Two files in this directory also **disagree about provenance**, which is worth
resolving rather than papering over:

| Source | Claim |
|---|---|
| `README.md` (this file, `## Original Author`) | 2008–2024 |
| `jkqtmathtext.readme` (upstream's own file) | 2010–2015 |

The upstream file is the more likely authority for the vendored *code*; the
wider range here may reflect later edits to this extraction. Both are left
as-is because guessing would make the record worse, not better.

**To close this:** record the upstream commit or release tag in this file and
in `REUSE.toml` (which currently annotates `libs/jkqtmathtext/**` but pins no
version), then reconcile the two date ranges.

## Original Author

Copyright (c) 2008-2024 Jan W. Krieger (<jan@jkrieger.de>)

JKQTMathText was originally developed as part of JKQtPlotter by Jan W. Krieger
at the German Cancer Research Center (DKFZ) and the University of Heidelberg.

## License

LGPL-2.1-or-later (GNU Lesser General Public License)

See the original license headers in each source file.

## What This Is

This is a standalone extraction of the JKQTMathText component for use in
Corbomite. It renders LaTeX math formulas directly via QPainter -- no external
TeX installation required.

## Features

- Inline math: `$E = mc^2$`
- Display math: `$$\int_a^b f(x)\,dx$$`
- Supports ~93% of MathJax/KaTeX math commands
- Pure C++/Qt, no external dependencies
- Renders via QPainter (vector graphics, scalable)
- Includes XITS and Fira Math fonts
