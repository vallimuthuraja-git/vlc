# VLC Dark Mode — Developer & AI Agent Guide

**Scope:** `modules/gui/qt` (the Qt user interface)
**Baseline version:** 3.0.24 (tag `3.0.24`, `AC_INIT(vlc, 3.0.24)` in `configure.ac`)
**Build system:** Autotools (`./bootstrap && ./configure && make`)

This document explains where the theme colours live, how to change them safely,
and how an AI agent should perform and verify a change.

---

## 1. Concepts

VLC's Qt interface has **three** mutually exclusive colour themes:

| Theme | Config option | State |
|---|---|---|
| **System** (default) | `qt-dark-palette = 0` | Follows the desktop preference |
| **Dark** | `qt-dark-palette = 1` | VLC's custom dark `QPalette` |
| **Light** | `qt-dark-palette = 2` | The classic palette, restored as it was |

The option was a boolean (`Use a dark palette`) before this change. It kept its
name and its `0` / `1` values, so existing configurations keep working: `1`
still means Dark, and the old default `0` now means "System" instead of "Light".
The consequence is that anyone who never touched the setting now follows the
desktop rather than always getting the light theme.

Switching is **seamless** — it happens live when the selection in
*Preferences → Interface* changes. No restart is required.

> All three themes are always available. Turning dark mode off restores the
> exact palette that was active before, not a freshly guessed one (see §3.3).

### 1.1 How "System" is detected

`systemPrefersDark()` in `qt.cpp` tries three sources, in order:

1. **Qt 6.5+** — `QStyleHints::colorScheme()`. This is the only source that
   also *notifies* on change, via the `colorSchemeChanged` signal wired up in
   `ThreadPlatform()`, so on Qt 6.5+ the theme follows the desktop live.
2. **Windows** — the `AppsUseLightTheme` value under
   `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize`.
   `0` means apps should be dark. A missing or non-`REG_DWORD` value is treated
   as light, matching the documented Windows default.
3. **Everything else** — the platform style's window colour, cached at startup
   by `savePlatformPalette()` and compared against mid grey.

Two consequences worth knowing:

- On **Qt older than 6.5** there is no scheme query and no change signal, so
  source 1 is compiled out entirely and the theme is decided once at startup.
  Sources 2 and 3 still work.
- The cache in `savePlatformPalette()` is **not** optional. The Qt interface
  forces `Fusion` when a custom palette is in use, and `Fusion`'s standard
  palette is always light — reading `QApplication::style()` after that point
  would report "light" forever and a light → dark desktop switch could never be
  detected.

---

## 2. Where the colours live

### 2.1 The single source of truth: `applyDarkPalette()`

**File:** `modules/gui/qt/qt.cpp` (search for `void applyDarkPalette()`)

Every dark-theme colour is declared in **one function**. There is no second
copy. All `QPalette::Active`, `Inactive` and `Disabled` groups are filled by a
local `setAll()` helper, so a role is defined exactly once and applied to all
three groups consistently.

```cpp
static const QColor windowColor (43, 43, 43);    /* #2B2B2B panels        */
static const QColor baseColor   (24, 24, 24);    /* #181818 text fields   */
static const QColor altColor    (35, 35, 35);    /* #232323 zebra rows    */
static const QColor midColor    (62, 62, 62);    /* #3E3E3E separators    */
static const QColor lightColor  (95, 95, 95);    /* #5F5F5F 3D borders    */
static const QColor darkColor   (15, 15, 15);    /* #0F0F0F shadows       */
static const QColor textColor   (255, 255, 255);
static const QColor dimText     (150, 150, 150); /* unfocused / secondary */
static const QColor disabledText(120, 120, 120);
static const QColor disabledBg  (20, 20, 20);
...
QColor accentColor (255, 136, 0);                /* VLC brand orange      */
```

### 2.2 Role → meaning reference

| QPalette role | Used for | Current dark value |
|---|---|---|
| `Window` | Panels, dialogs, toolbars, window bg | `#2B2B2B` |
| `WindowText` | Labels, menu text | `#FFFFFF` / `#969696` inactive |
| `Base` | Text fields, list areas, input backgrounds | `#181818` |
| `AlternateBase` | Zebra striping in tables | `#232323` |
| `Button` | Push/tool buttons, checkbox bodies | `#2B2B2B` |
| `ButtonText` | Button labels | `#FFFFFF` |
| `Text` | Editable text, table cells | `#FFFFFF` |
| `Highlight` | Selected row / text selection background | accent `#FF8800` |
| `HighlightedText` | Text on `Highlight` | `#141414` (dark, on orange) |
| `Link` | Hyperlinks in dialogs | accent lightened 140% |
| `LinkVisited` | Visited hyperlinks | accent lightened 105% |
| `PlaceholderText` | Greyed-out hint text in inputs | `#969696` |
| `ToolTipBase` | Tooltip background | `#3A3A3A` |
| `ToolTipText` | Tooltip text | `#FFFFFF` |
| `Light` | Raised 3D edges (top/left) | `#5F5F5F` |
| `Midlight` | Subtle edge tone | `#3E3E3E` |
| `Mid` | Separators, grid lines, borders | `#3E3E3E` |
| `Dark` | Recessed edges | `#0F0F0F` |
| `Shadow` | Drop shadows | `#0F0F0F` |
| `BrightText` | Emphasised/inverted text | `#FF7800` |
| `Accent` | Qt 6.6+ accent colour | `#FF8800` |

### 2.3 Colour constants currently in play

| Constant | Hex | Used for |
|---|---|---|
| `windowColor` | `#2B2B2B` | Dialog / panel background |
| `baseColor` | `#181818` | Input fields, list backgrounds |
| `altColor` | `#232323` | Alternating table rows |
| `midColor` | `#3E3E3E` | Separators, grid lines |
| `lightColor` | `#5F5F5F` | 3D bevel highlights |
| `darkColor` | `#0F0F0F` | Shadows, recessed edges |
| `dimText` | `#969696` | Unfocused/secondary text |
| `disabledText` | `#5A5A5A` | Disabled control text |
| `disabledBg` | `#141414` | Disabled control background |
| `tooltipColor` | `#3A3A3A` | Tooltip surface |
| `accentColor` | `#FF8800` | Selection, links, focus ring (VLC orange) |

### 2.4 The classic (light) theme

The classic theme is **not** a second VLC palette. It is whatever palette Qt
hands out for the selected style at startup — normally the platform default or
`Fusion`'s standard palette. It is captured once (§3.3) and restored verbatim.

---

## 3. How the palette is applied

### 3.1 Startup (`modules/gui/qt/qt.cpp`, in `Thread()`)

```cpp
/* Loads and tries to apply the preferred QStyle */
QString s_style = getSettings()->value( "MainWindow/QtStyle", "" ).toString();
if (!s_style.isEmpty())
    QApplication::setStyle( s_style );

// Apply dark palette only if dark palette is enabled
if (isDarkPaletteEnabled(p_intf))
{
    /* The native platform styles ignore palette colours; without a style
     * explicitly chosen by the user, use Fusion so the dark palette is
     * actually honoured. */
    if (s_style.isEmpty())
        QApplication::setStyle( QStringLiteral( "Fusion" ) );
    applyDarkPalette();
}
```

Two important consequences:

1. **Fusion is forced** when dark mode is on and the user has not picked a
   style. Native styles (Windows, GNOME/Kvantum, macOS) hardcode their own
   paints and would ignore the palette entirely.
2. The palette is set **before** `app.exec()` and before the main window is
   created, so the whole interface is born dark — no flash of light.

### 3.2 Runtime switch (`components/simple_preferences.cpp`)

```cpp
CONFIG_BOOL( "qt-dark-palette", qtdark );
connect(ui.qtdark, &QCheckBox::stateChanged, ui.stylesCombo,
    [combobox = ui.stylesCombo](const int state) {
        if (state == Qt::CheckState::Checked) {
            combobox->setCurrentText(QStringLiteral("Fusion"));
            applyDarkPalette();
        } else {
            applyClassicPalette();
        }
    });
```

`CONFIG_BOOL` persists the preference; the lambda applies it immediately.

### 3.3 Save / restore of the classic palette

```cpp
static QPalette classicPalette;
static bool classicPaletteSaved = false;

void applyDarkPalette()
{
    if (!classicPaletteSaved) {
        classicPalette = QApplication::palette();
        classicPaletteSaved = true;
    }
    ...
}

void applyClassicPalette()
{
    if (classicPaletteSaved)
        QApplication::setPalette(classicPalette);
    else
        QApplication::setPalette(QApplication::style()->standardPalette());
}
```

The guard exists because a second `applyDarkPalette()` must **not** capture the
dark palette as "classic".

### 3.4 Perception of `isDarkPaletteEnabled()`

```cpp
bool isDarkPaletteEnabled(intf_thread_t *p_intf)
{
    return var_InheritBool( p_intf, "qt-dark-palette" );
}
```

Warning: it must **never** be cached in a `static const` local. It was once,
which made runtime switching silently non-functional after the first read. This
is a regression class to watch for.

---

## 4. Widget-level colours (paint code)

Some widgets paint with `QPainter` instead of using the palette. Those colours
must be theme-aware or they break one of the two themes.

**Rule of thumb — in this order of preference:**

1. Read from the widget's `palette()` (`QPalette::Base`, `QPalette::Window`,
   `QPalette::Mid`, `QPalette::Button`, ...).
2. If a genuinely custom colour is required, branch on the current theme
   lightness and define **both** branches.
3. Never hardcode a single light *or* dark colour.

**Detecting the active theme inside a paint method** (works for both themes
without needing the interface):

```cpp
const bool darkTheme = palette().color( QPalette::Base ).lightness() < 128;
```

### 4.1 Files that already contain theme-aware paint code

| File | What it does |
|---|---|
| `styles/seekstyle.cpp` | Seek-bar knob, uses `isDarkPaletteEnabled()` |
| `util/input_slider.cpp` | Volume/seek gradients from `p.window()` / `p.shadow()` |
| `dialogs/help.cpp` | About dialog frame/footer derived from `QApplication::palette()` |
| `components/epg/EPGItem.cpp` | Programme gradients + border (both branches) |
| `components/epg/EPGView.cpp` | Channel lines from `QPalette::Mid` |
| `components/info_widgets.cpp` | Stats graph fill + ruler lines (both branches) |
| `util/buttons/RoundButton.cpp` | Button gradient from `QPalette::Button` |

### 4.2 Known remaining light-only spots (review before touching)

| File:line | Colour | Risk |
|---|---|---|
| `dialogs/toolbar.cpp:376` | `QColor(255,255,255,128)` | Preview overlay only — cosmetic, acceptable |
| `dialogs/plugins.cpp:1248` | `QColor(255,255,255,128)` | Same overlay pattern |
| `components/info_panels.cpp:621` | `#ff8c00` | Orange — fine on both themes |
| `components/epg/EPGRuler.cpp:125` | `QColor(255,0,0,128)` | Semi-transparent red, fine on both |

---
## 5. Changing the colours — worked procedure

### 5.1 The only edit you normally need

Open `modules/gui/qt/qt.cpp`, find `applyDarkPalette()`, and change the RGB
triples. Nothing else. The palette is applied to `QApplication`, so every
`QWidget` — dialogs, menus, lists, file browser, preferences pages — updates
at once.

```bash
grep -n "void applyDarkPalette" modules/gui/qt/qt.cpp
```

### 5.2 Contrast rules (do not skip these)

| Pair | Required ratio | Why |
|---|---|---|
| `Text` on `Base` | >= 7:1 | Body text, long reading |
| `WindowText` on `Window` | >= 4.5:1 | Labels, menu items |
| `HighlightedText` on `Highlight` | >= 4.5:1 | Selected rows |
| `ToolTipText` on `ToolTipBase` | >= 4.5:1 | Tooltips |
| any text on `Disabled` | >= 3:1 | Disabled *should* recede |

Measure with WCAG relative luminance:

```python
def lum(c):
    def f(v):
        v /= 255.0
        return v/12.92 if v <= 0.03928 else ((v+0.055)/1.055) ** 2.4
    return 0.2126*f(c[0]) + 0.7152*f(c[1]) + 0.0722*f(c[2])

def ratio(a, b):
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)
```

If you add a **new** colour, name it like the existing ones (`fooColor`), add a
trailing comment saying what it paints, and pass it through `setAll()` so the
Disabled group is handled too.

### 5.3 Changing a colour that is hardcoded in a widget

See section 4. The correct fix is to remove the constant and read from the
palette. Only branch on `palette().color(...).lightness() < 128` when a
genuinely custom gradient is required. This keeps the two themes in lockstep.

---

## 6. AI agent instructions

An agent asked to "change the dark mode colours" or "fix a dark mode styling
issue" should follow this exactly.

### 6.1 Rules

1. **Never edit the classic/light colours** to fix a dark-mode complaint, or
   vice versa. A "text is invisible" report in dark mode is a *dark* bug.
2. **Never hardcode a new `QColor` in a widget** as a first move. Look for a
   `QPalette` role that already carries the intent.
3. **Never remove the `classicPalette` save/restore logic.** It is what makes
   the toggle reversible (section 3.3).
4. **Never add a new theme.** Two themes only: classic and dark.
5. **Never edit `configure.ac`, `Makefile.am`, or version strings.** Out of
   scope.
6. After changing `applyDarkPalette()`, verify every pair in 5.2 before
   declaring done. Compute the ratios; do not eyeball them.

### 6.2 Task: change the dark palette colours

```
1. Read modules/gui/qt/qt.cpp -> applyDarkPalette(). Note every constant.
2. Confirm there is exactly one applyDarkPalette() and no duplicate
   makeDarkPalette()/getDarkPalette():
       grep -rn "applyDarkPalette\|makeDarkPalette" modules/gui/qt/
3. Edit the constants.
4. Recompute all 5.2 contrast ratios against the new values.
5. Rebuild:  make -j"$(nproc)" 2>&1 | tee /tmp/build.log
6. Confirm zero errors:  grep -c 'error:' /tmp/build.log   -> must be 0
7. Runtime test per section 7.
8. Report: colours before -> after, ratios, build result, screenshot paths.
```

### 6.3 Task: a widget looks wrong in dark mode only

```
1. Reproduce and identify the exact widget + which colour is wrong.
2. Find where that colour comes from:
       grep -rn "<colour literal>" modules/gui/qt/
3. If it comes from a QPalette role -> the role is wrong or unset.
   Fix in applyDarkPalette() only.
4. If hardcoded -> make it theme-aware (sections 4 / 5.3), defining BOTH
   branches.
5. Add the file to the 4.1 table in this document.
6. Rebuild + runtime test BOTH themes, not just the broken one.
```

### 6.4 Definition of done

- `make` completes with **no** `error:` lines.
- Dark mode: every dialog, menu, list, text field, toolbar and tooltip is
  legible; no light widgets on a dark background and vice versa.
- Classic mode is **pixel-identical** to a build without the patch — check a
  dialog in both.
- Toggling the preference flips themes live, both directions, repeatedly, with
  no stale colours left from the previous theme.
- `git diff` touches only `modules/gui/qt/**` (plus this file).

---

## 7. Testing procedure

### 7.1 Build

```bash
cd /path/to/vlc
make -j"$(nproc)" 2>&1 | tee /tmp/build.log
grep -c 'error:' /tmp/build.log        # must be 0
```

### 7.2 Launch with an isolated config

Do not touch the user's `~/.config/vlc`.

```bash
export CFG=/tmp/vlc-theme-test
rm -rf "$CFG" && mkdir -p "$CFG"
printf '[qt]\nqt-dark-palette=1\n' > "$CFG/vlcrc"
```

### 7.3 Toggling at runtime

Both themes must be reachable without a restart:

```bash
./vlc --intf qt --qt-dark-palette=1   # dark
./vlc --intf qt --qt-dark-palette=0   # classic
```

Or at runtime via *Tools -> Preferences -> Interface* ("Use dark palette")
while the window is open.

### 7.4 Manual sweep checklist

Run through every item **in both themes** and look for light-on-dark or
dark-on-light artefacts, invisible text, invisible cursors and missing borders:

| # | Surface | How to open |
|---|---|---|
| 1 | Main window / playlist | launch |
| 2 | Preferences (all tabs) | Tools -> Preferences |
| 3 | Advanced / "All" preferences tree | Preferences -> *All* |
| 4 | Playlist context menu | right-click an item |
| 5 | Menu bar + each menu | click each |
| 6 | Open Media dialog (all tabs) | Media -> Open File... |
| 7 | Convert / Stream dialogs | Media -> Convert/Save |
| 8 | Messages window | Tools -> Messages |
| 9 | Effects & Filters | Tools -> Effects |
| 10 | Track synchronization | Tools -> Track Sync |
| 11 | VLM configuration | Tools -> VLM |
| 12 | Open Network Stream | Media -> Open Network Stream |
| 13 | Codec information | Tools -> Codec Information |
| 14 | Snapshots / Record dialogs | Video -> Snapshots, Record |
| 15 | Seek bar + volume slider | drag them while playing |
| 16 | Tooltips on every control | hover |
| 17 | Text selection in a field / list | drag-select |
| 18 | Disabled controls | Stop/Play when idle |
| 19 | File browser dialog | Open File -> Browse |

### 7.5 Automated contrast check

The strongest non-visual test: extract the dark colours from
`applyDarkPalette()` and assert the 5.2 ratios.

```bash
cat > /tmp/contrast.py <<'EOF'
import re, sys
src = open('modules/gui/qt/qt.cpp').read()
body = src[src.index('void applyDarkPalette()'):]
body = body[:body.index('\n}')]

def col(name):
    # matches both QColor foo(r, g, b) and const auto foo = QColor(r, g, b)
    m = re.search(r'%s[^\n(]*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)' % name, body)
    return tuple(int(x) for x in m.groups()) if m else None

def lum(c):
    f = lambda v: (v/255)/12.92 if v/255 <= 0.03928 else (((v/255)+0.055)/1.055)**2.4
    return 0.2126*f(c[0]) + 0.7152*f(c[1]) + 0.0722*f(c[2])

def ratio(a, b):
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)

CHECKS = [
    ('textColor',     'baseColor',   7.0, 'Text on Base'),
    ('textColor',     'windowColor', 4.5, 'WindowText on Window'),
    ('highlightText', 'accentColor', 4.5, 'HighlightedText on Highlight'),
    ('textColor',     'tooltipColor',4.5, 'ToolTipText on ToolTipBase'),
    ('dimText',       'baseColor',   4.5, 'Placeholder on Base'),
    ('disabledText',  'baseColor',   3.0, 'DisabledText on Base'),
]
bad = 0
for fg, bg, need, label in CHECKS:
    a, b = col(fg), col(bg)
    if not a or not b:
        print('SKIP  %-32s (colour not found)' % label); continue
    r = ratio(a, b)
    ok = r >= need
    bad += not ok
    print('%s %-32s %5.2f:1 (need %.1f) #%02X%02X%02X on #%02X%02X%02X'
          % ('PASS' if ok else 'FAIL', label, r, need, *a, *b))
sys.exit(1 if bad else 0)
EOF
python3 /tmp/contrast.py   # exit 0 = all good
```

### 7.6 Screenshots

```bash
DISPLAY=:0 import -window root /tmp/shot.png
xwininfo -root -tree | grep -i vlc        # find a specific window id
DISPLAY=:0 import -window 0x400001 /tmp/shot.png
```

---

## 8. Packaging as a .deb

VLC's autotools build produces an install tree; the distribution packaging
lives under `packages/`.

```bash
cd /path/to/vlc
make -j"$(nproc)"
make install DESTDIR=/tmp/vlc-stage        # stage the full install tree
```

The staged tree is `/tmp/vlc-stage/usr/{bin,lib,share}`. To turn it into an
installable package:

```bash
mkdir -p /tmp/deb/DEBIAN
cat > /tmp/deb/DEBIAN/control <<'EOF'
Package: vlc
Version: 3.0.24
Architecture: amd64
Maintainer: local <local@localhost>
Section: multimedia
Priority: optional
Depends: libc6, libstdc++6, libqt5core5a | libqt6core6, zlib1g
Description: VLC media player (locally built, with dark mode)
 A local build of VLC 3.0.24 including the dark/classic theme work.
EOF
cp -a /tmp/vlc-stage/usr /tmp/deb/
dpkg-deb --build --root-owner-group /tmp/deb /tmp/vlc-3.0.24-darkmode.deb
sudo dpkg -i /tmp/vlc-3.0.24-darkmode.deb
```

Notes:

- `--root-owner-group` makes the package installable without root ownership
  problems.
- This binary will **conflict** with a distribution-installed `vlc` package. If
  one is present, either remove it first (`sudo apt remove vlc`) or skip the
  `.deb` and just run from the build directory.
- To run straight from the build tree without packaging:
  ```bash
  LD_LIBRARY_PATH=$PWD/lib ./vlc --intf qt
  ```
- `make dist` produces `vlc-3.0.24.tar.xz`; `make distcheck` re-runs the whole
  build and test suite from that tarball in a clean directory — the right
  pre-release check, but slow.

---

## 9. Reference: the dark palette, role by role

Quick lookup when you need a single colour. These are the defaults in
`applyDarkPalette()` as of this writing — always confirm against the source
before relying on them.

| Role | Dark | Classic |
|---|---|---|
| `Window` | `#2B2B2B` | system |
| `WindowText` | `#FFFFFF` | system |
| `Base` | `#181818` | system |
| `AlternateBase` | `#232323` | system |
| `Text` | `#FFFFFF` | system |
| `Button` | `#2B2B2B` | system |
| `ButtonText` | `#FFFFFF` | system |
| `Highlight` | `#FF8800` | system |
| `HighlightedText` | `#141414` | system |
| `Link` | lightened accent | system |
| `LinkVisited` | accent, slightly lightened | system |
| `PlaceholderText` | `#969696` | system |
| `ToolTipBase` | `#3A3A3A` | system |
| `ToolTipText` | `#FFFFFF` | system |
| `Mid` / `Midlight` | `#3E3E3E` | system |
| `Light` | `#5F5F5F` | system |
| `Dark` / `Shadow` | `#0F0F0F` | system |
| `BrightText` | `#FF7800` | system |
| `Accent` (Qt 6.6+) | `#FF8800` | system |

An omitted role falls through to the platform theme — which is exactly how
light widgets leak into dark mode. If a widget shows a colour you did not
choose, the role is almost certainly missing a `setAll(...)` line.

---

## 10. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| Dark mode checkbox has no effect | `applyDarkPalette()` never called | Check the connect in `qt.cpp` around the `qt-dark-palette` var callback |
| One dialog stays light | That dialog sets its own `QPalette` | Remove the local palette, let it inherit from `qApp` |
| Text invisible | Contrast below 5.2 ratio | Run `/tmp/contrast.py`; fix the offending constant |
| Classic mode looks wrong after toggling | `classicPalette` not saved | Ensure the save happens *before* the dark palette is set |
| Role looks light in dark mode | Role not set in `applyDarkPalette()` | Add a `setAll()` line for it |
| Widget ignores the palette | It uses a hardcoded `QColor` | Make it theme-aware (5.3) |

