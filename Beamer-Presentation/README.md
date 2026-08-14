# Beamer Presentation — Campus Biometric Gate Entry Management System

> **Target Audience**: Faculty Advisory Committee / Professor Supervisor  
> **Purpose**: Demonstrate technical depth, academic novelty, and publication/patent potential to secure faculty co-supervision and guidance.

---

## File Structure

```
Beamer-Presentation/
├── main.tex                    ← Main Beamer presentation (compile this)
├── snippets/
│   └── parity_logic.cpp        ← C++ code snippet included in slides
└── README.md                   ← This file
```

---

## How to Compile

### Option 1: Overleaf (Recommended — No Local Install)
1. Zip the entire `Beamer-Presentation/` folder
2. Upload to [Overleaf](https://www.overleaf.com/) → New Project → Upload Project
3. Set `main.tex` as the main document
4. Compile with **pdfLaTeX**

### Option 2: Local Compilation (Requires TeX Live / MiKTeX)

```bash
# Install TeX Live (macOS)
brew install --cask mactex

# Compile (run twice for ToC + references)
cd Beamer-Presentation
pdflatex main.tex
pdflatex main.tex
```

### Required LaTeX Packages
All packages used are standard and included in TeX Live Full / MiKTeX:
- `beamer`, `amsmath`, `amssymb`, `booktabs`, `tabularx`, `multirow`
- `listings`, `tikz`, `xcolor`, `hyperref`, `fontawesome5`, `graphicx`

---

## Slide Summary (18 Slides)

| # | Section | Content |
|:--|:--------|:--------|
| 1 | Title | Project name, subtitle, team, date |
| 2 | Outline | Table of contents |
| 3 | Problem | Campus gate pain points, queue collapse (M/M/1), industry landscape |
| 4 | Architecture | TikZ diagram of dual-tier C++/Python architecture |
| 5 | Innovation 1 | Residency-aware parity state machine — formal math + C++ code |
| 6 | Innovation 2a | BioHashing mathematical foundation + ISO/IEC 24745 compliance |
| 7 | Innovation 2b | Encrypted template buffer layout (TikZ diagram) |
| 8 | Innovation 3 | Dual-tier coarse-to-fine matching pipeline (TikZ flowchart) |
| 9 | Innovation 4 | Home leave reconciliation + automated curfew audit |
| 10 | Performance 1 | M/M/1 queueing theory benchmark table |
| 11 | Performance 2 | C++ vs Python quantified speedup comparison |
| 12 | Comparative | 12-dimension competitive benchmark matrix |
| 13 | Gaps | Four identified research gaps |
| 14 | Standards | ISO/IEC 24745, 19795, 30107, 2382-37 compliance table |
| 15 | Evolution | v1.0 → v2.0 security hardening comparison |
| 16 | Future Work | v3.0 novel cryptographic protocol vision + faculty opportunity |
| 17 | Roadmap | 5-phase IP + publication timeline (TikZ) |
| 18 | Venues | Target journals (IEEE TII, Access, ACM TECS) + patent IPC codes |
| 19 | Summary | Technical contributions + explicit faculty request |
| 20 | Closing | Thank you + Q&A |

---

## Customization Notes

- **Team members**: Update `\author` in `main.tex` with actual names
- **Institution**: Update `\institute` with your university name and department
- **Date**: Currently set to August 2026
- **Font**: Uses `fontawesome5` for icons — if compilation fails on icons, install the package or remove `\faIcon` calls
