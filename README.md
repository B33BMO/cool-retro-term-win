# cool-retro-term for Windows

A Windows port of [cool-retro-term](https://github.com/Swordfish90/cool-retro-term) — the retro CRT terminal emulator — built with **Qt 6 + MSVC** and using the **ConPTY API** for the terminal backend.

![Retro glow on Windows](https://img.shields.io/badge/Windows-10%2B-blue) ![Qt](https://img.shields.io/badge/Qt-6.11-green) ![MSVC](https://img.shields.io/badge/MSVC-2022-purple)

---

## Features

- Full CRT shader effects (scanlines, bloom, burn-in, curvature, jitter, chroma aberration, phosphor glow)
- **ConPTY backend** — connects to any Windows shell via the modern Windows Pseudo Console API (Win10 1809+)
- **Shell preset picker** built into the settings:
  - cmd.exe
  - Windows PowerShell (`powershell.exe`)
  - PowerShell Core (`pwsh.exe`)
  - WSL (default distro, or explicit e.g. `Ubuntu-24.04`)
  - Git Bash
  - Custom command
- Menu bar enabled by default for easy discovery (File / Edit / View / Profiles / Help)
- Right-click context menu with the same options
- All the original profiles (Default Amber, Vintage, Futuristic, IBM DOS, Apple ][, Commodore 64, etc.)
- Single-instance application (clicking the shortcut focuses the existing window)

---
<img width="1034" height="805" alt="image" src="https://github.com/user-attachments/assets/fde49ed3-25e6-4951-b5c2-9003c45d8f66" />
<img width="1028" height="799" alt="image" src="https://github.com/user-attachments/assets/350445af-3e17-4f63-9b0a-df0fc4e81c01" />
<img width="998" height="780" alt="image" src="https://github.com/user-attachments/assets/346185bb-8320-47b8-a7e9-995096602bf1" />
<img width="1029" height="764" alt="image" src="https://github.com/user-attachments/assets/ee1d8e97-3c6b-4112-932e-6ff1ff0593d5" />
<img width="955" height="777" alt="image" src="https://github.com/user-attachments/assets/0357492c-460f-439a-835f-4d76fbb70315" />
And more!



## Installing

### Option A — Installer
Download the latest `cool-retro-term-<version>-win64-setup.exe` from [Releases](../../releases) and run it. Offers optional desktop icon and PATH entry.

### Option B — Portable ZIP
Download `cool-retro-term-<version>-win64-portable.zip`, extract anywhere, and run `cool-retro-term.exe`.

**System requirements:**
- Windows 10 version 1809 (build 17763) or later — ConPTY is required
- x86-64

---

## Building from source

### Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| Qt | 6.11 (msvc2022_64) | Must include the **Qt 5 Compatibility Module** — this provides the `Qt5Compat.GraphicalEffects` QML plugin (FastBlur etc.) used by the bloom effect. |
| Visual Studio | 2022 Community or higher | MSVC 2022 (cl.exe, link.exe, nmake.exe) |
| Python | 3.9+ | Used once to generate `crt.ico` from the PNG icons |
| Inno Setup | 6.x | For producing the installer (optional) |

Install the **Qt 5 Compatibility Module** via the Qt Maintenance Tool: Qt → 6.11.0 → MSVC 2022 64-bit → **Qt 5 Compatibility Module**. Without it, the app will start but fail to load the bloom/CRT effects at QML compile time.

### One-shot build

```cmd
packaging\windows\build-all.bat
```

This runs the whole pipeline: MSVC environment setup → shader pre-compilation → qmake → nmake → windeployqt → Inno Setup installer → portable ZIP.

Output lands in `dist\`:
- `cool-retro-term-1.0.0-win64-setup.exe`
- `cool-retro-term-1.0.0-win64-portable.zip`

### Step-by-step

```cmd
REM Compile and deploy (puts everything into deploy\cool-retro-term\)
packaging\windows\build.bat

REM Clean rebuild
packaging\windows\build.bat --clean

REM Re-run windeployqt without recompiling
packaging\windows\build.bat --deploy-only

REM Make the installer
packaging\windows\build-installer.bat

REM Make the portable ZIP
packaging\windows\make-portable.bat
```

The build script expects Qt at `C:\Qt\6.11.0\msvc2022_64` and Visual Studio 2022 Community at the default location. Edit the `QTDIR` and `VCVARS` lines at the top of `packaging\windows\build.bat` if your install paths differ.

### Icon generation

The Windows icon (`app/icons/crt.ico`) was generated once from the existing PNG sources using Pillow:

```python
from PIL import Image
sizes = [256, 128, 64, 32]
images = [Image.open(f'app/icons/{s}x{s}/cool-retro-term.png').resize((s,s), Image.LANCZOS) for s in sizes]
images[0].save('app/icons/crt.ico', format='ICO', sizes=[(s,s) for s in sizes], append_images=images[1:])
```

---

## Architecture

The port is built around a platform abstraction at the `Pty` class level in `qmltermwidget/lib/`:

```
        ┌───────────────────┐
        │   Session.cpp     │   (platform-agnostic terminal session)
        └─────────┬─────────┘
                  │
          ┌───────┴────────┐
          │                │
     ┌────▼────┐      ┌────▼──────────┐
     │  Pty    │      │  Pty (win32)  │
     │ (unix)  │      │               │
     └────┬────┘      └────┬──────────┘
          │                │
   ┌──────▼──────┐    ┌────▼──────────┐
   │ KPtyProcess │    │ ConPtyProcess │
   │ (fork+pty)  │    │ (CreatePseudo │
   │             │    │  Console +    │
   │             │    │  CreateProcess│
   │             │    │  W)           │
   └─────────────┘    └───────────────┘
```

### `ConPtyProcess` (qmltermwidget/lib/ConPtyProcess.{h,cpp})

A new class wrapping the Windows ConPTY API:

- `CreatePseudoConsole(size, hInputRead, hOutputWrite, 0, &hPC)` to create the pseudo console
- `CreateProcessW` with `STARTUPINFOEXW` and the `PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE` attribute to bind the child process to the console
- A dedicated `ConPtyReadThread` does blocking `ReadFile()` on the output pipe and marshals the data back to the main thread via `QMetaObject::invokeMethod(Qt::QueuedConnection)`
- A `ConPtyWaitThread` does `WaitForSingleObject(hProcess)` to detect exit
- Graceful shutdown via `ClosePseudoConsole()`, falling back to `TerminateProcess()` after a 2-second timeout

### Cross-platform compatibility shims

A few things needed help to survive MSVC / Qt 6:

| Shim | File | Why |
|------|------|-----|
| `compat_mman.h` | `qmltermwidget/lib/` | Provides `mmap`/`munmap` via `VirtualAlloc` and `MapViewOfFile` for `History` / `BlockArray` |
| `compat_textcodec.h` | `qmltermwidget/lib/` | Minimal UTF-8-only `QTextCodec` stub, avoids depending on `Qt5Compat` C++ module |
| `mk_wcwidth()` | `konsole_wcwidth.cpp` | MSVC has no `wcwidth()`; bundled Markus Kuhn public-domain implementation |
| POSIX I/O aliases | `History.cpp`, `BlockArray.cpp` | `read`→`_read`, `write`→`_write`, `lseek`→`_lseek`, `ftruncate`→`_chsize`, `dup`→`_dup`, `getpagesize`→`GetSystemInfo().dwPageSize` |

### Shader pipeline notes

The project ships ~60 pre-baked shader variants (5 rasterization modes × 2 burn-in × 2 frame × 2 chroma for the dynamic shader, 2 × 2 × 2 × 2 for the static shader). The Windows build pre-compiles these with `qsb` **before** `qmake` runs, so the `.qsb` outputs exist before `rcc` embeds them into the binary.

Two shader patches were required for the HLSL backend:

1. `terminal_dynamic.frag` / `.vert` samplers moved from `binding=0..3` to `binding=1..4` — the std140 uniform buffer also lived at binding 0, which confused the GLSL→HLSL register slot allocator and produced `X4500: overlapping register semantics` errors.
2. `PreprocessedTerminal.qml` no longer assigns `enableItalic` — the C++ `TerminalDisplay` doesn't expose that property (only `enableBold`).

### Resource splitting (MSVC-specific)

The bundled fonts (28 of them) plus 56 shader variants produced a single `qrc_resources.cpp` of ~2.7 million lines, which overflowed `cl.exe`'s heap with `C1060: compiler is out of heap space`. The fix was to split the QRC into five files:

- `resources.qrc` — QML, images, icons (small)
- `resources_fonts1.qrc`, `resources_fonts2.qrc`, `resources_fonts3.qrc` — fonts split into three groups
- `resources_shaders.qrc` — all 60 compiled shader variants

Each generated `qrc_*.cpp` is small enough for MSVC to compile happily with `/bigobj`.

---

## Qt 5 → Qt 6 migration

The port doubles as a Qt 5 to Qt 6 migration for `qmltermwidget`. The following changes were applied to `qmltermwidget/lib` (they're correct on all platforms, not just Windows):

- `QRegExp` → `QRegularExpression` (`Filter`, `HistorySearch`, `KeyboardTranslator`, `Session`, `ksession`)
- `QTextCodec` → UTF-8-only stub (`compat_textcodec.h`)
- `QByteRef ch = ba[i];` → `char ch = ba[i];`
- `QFontMetrics::width()` → `horizontalAdvance()`
- `QPalette::Background` → `QPalette::Window`
- `Qt::MidButton` → `Qt::MiddleButton`
- `QWheelEvent::delta()` / `pos()` / `orientation()` → `angleDelta()` / `position()` (and derived checks)
- `QQuickPaintedItem::geometryChanged` → `geometryChange`
- `QDrag::start()` → `QDrag::exec()`
- `Qt::ImMicroFocus` → `Qt::ImCursorRectangle`
- `qsrand` / `qrand` → `QRandomGenerator`
- `QString::midRef()` → `QStringView(s).mid()`
- Explicit `QChar(int)` construction
- `QFont::ForceIntegerMetrics` removed (no longer exists in Qt 6)

---

## Known limitations

- **Fullscreen on Windows:** works but the menu bar disappears in fullscreen mode (press `Esc` or toggle via the View menu to exit).
- **Window transparency:** disabled on Windows. The Linux/macOS build uses `color: "#00000000"` to let the compositor blend the window with the desktop; on Windows this caused the shader-less regions to leak desktop pixels through. The `windowOpacity` slider still works via DWM.
- **WSL quirks:** most things work, but very fast-scrolling output can occasionally stall the ConPTY reader. Pressing a key unblocks it. Same behavior as the default Windows Terminal with WSL.
- **ProcessInfo:** the Windows `ProcessInfo` implementation only reads the PID, parent PID, and executable name. Foreground-process detection (for tab titles showing the currently-running program) uses the ConPTY's root process only, not the actual foreground inside the shell.
- **Signal warnings at startup:** the qmltermwidget library emits a couple of `QObject::connect: No such signal` warnings on terminal creation — these are pre-existing Qt 5 style SIGNAL/SLOT strings in `TerminalDisplay.cpp` that don't match the current signal signatures. Non-fatal; will be cleaned up in a follow-up commit.
- **Qt 5 Compat dependency:** only the QML `Qt5Compat.GraphicalEffects` plugin is required (for `FastBlur`). The C++ `Qt5Compat` module is not needed thanks to the compat shims.

---

## Project layout

```
cool-retro-term-master/
├── app/                              The main QML application
│   ├── main.cpp                      Entry point, Q_OS_WIN initialization
│   ├── fontmanager.cpp               Font management (Cascadia Mono fallback on win32)
│   ├── icons/crt.ico                 Windows icon
│   ├── shaders/                      GLSL shader sources
│   ├── qml/
│   │   ├── main.qml
│   │   ├── TerminalWindow.qml        Opaque bg on Windows
│   │   ├── ApplicationSettings.qml   isWindows property, Windows defaults
│   │   ├── SettingsAdvancedTab.qml   Shell preset dropdown
│   │   ├── resources.qrc             Split across 5 QRC files for MSVC
│   │   └── ...
│   └── app.pro
│
├── qmltermwidget/                    Terminal widget (Konsole-derived)
│   ├── lib/
│   │   ├── Pty.{h,cpp}               Platform-abstracted PTY wrapper
│   │   ├── ConPtyProcess.{h,cpp}     ★ Windows ConPTY backend (new)
│   │   ├── compat_mman.h             ★ mmap shim (new)
│   │   ├── compat_textcodec.h        ★ QTextCodec stub (new)
│   │   ├── kpty*.cpp                 Unix PTY stack (excluded on win32)
│   │   └── ...
│   ├── src/
│   │   ├── qmltermwidget_plugin.cpp  Plugin registration (1.0 + 2.0)
│   │   └── ksession.{h,cpp}          QML session API
│   └── qmltermwidget.pro
│
├── KDSingleApplication/              Single-instance helper (flattened)
│
├── packaging/windows/                ★ Windows build pipeline (new)
│   ├── build.bat                     Main build script
│   ├── build-all.bat                 Build + installer + portable ZIP
│   ├── build-installer.bat           Inno Setup wrapper
│   ├── make-portable.bat             PowerShell Compress-Archive wrapper
│   └── installer.iss                 Inno Setup 6 script
│
├── cool-retro-term.pro               Top-level subdirs project
└── README-Windows.md                 This file
```

---

## License

Same as upstream cool-retro-term — GPL v2 or v3 (see `gpl-2.0.txt` and `gpl-3.0.txt`).

`qmltermwidget` is LGPL v2+ (from Konsole / qterminal). `KDSingleApplication` is MIT.

---

## Credits

- **Filippo Scognamiglio** ([Swordfish90](https://github.com/Swordfish90)) — original cool-retro-term
- **qmltermwidget** maintainers — Qt/QML wrapper around Konsole's terminal widget
- **KDAB** — KDSingleApplication
- **Konsole (KDE)** — terminal emulation core
- **Markus Kuhn** — public-domain `wcwidth()` implementation used in the MSVC fallback
