# Hex Editor — Architecture

## Overview

Hex Editor is a native GTK4/libadwaita binary file editor written in C17. It follows the same architecture as [Notes Light](../README.md), with a clean separation between window management, actions/dialogs, and persistent settings.

## Project Structure

```
hex-editor/
├── Makefile              # Build system (gcc, pkg-config)
├── README.md
├── docs/
│   ├── architecture.md   # This file
│   └── changelog.md
├── images/
│   ├── hex-editor.svg    # Application icon (source)
│   └── hex-editor.png    # Application icon (256x256)
├── src/
│   ├── main.c            # Entry point, GApplication setup
│   ├── window.c          # Window construction, hex rendering, input handling
│   ├── window.h          # HexWindow struct, public API
│   ├── actions.c         # GAction handlers, settings dialog, keyboard shortcuts
│   ├── actions.h         # actions_setup() declaration
│   ├── settings.c        # Config file load/save
│   └── settings.h        # HexSettings struct
└── build/                # Build output (generated)
```

## Module Responsibilities

### main.c (30 lines)
- Creates `AdwApplication`
- Connects `activate` signal to create the window
- Registers app-level actions (quit)

### window.c (1170 lines)
Core of the application. Handles:

- **Theme system** — 13 color themes (same as Notes Light), CSS generation, Adwaita style manager integration
- **Hex rendering** — Custom `GtkDrawingArea` with Cairo/Pango drawing. Renders offset column, hex/binary data bytes, ASCII pane, cursor, selection, and cross-highlights
- **Input handling** — Keyboard controller on the window for hex/binary/ASCII editing, navigation (arrows, Page Up/Down, Home/End), selection (Shift+arrows)
- **File I/O** — Load files up to 64 MB, auto-growing buffer for new data, atomic saves
- **Search** — Hex pattern or ASCII text search with match navigation
- **Go to offset** — Dialog accepting hex (0x...) or decimal input
- **Close protection** — Save/Discard/Cancel dialog on close with unsaved changes
- **Scrollbar** — Virtual scrolling with `GtkAdjustment`, mouse wheel support

### actions.c (407 lines)
- Registers all `GAction` entries on the window
- Implements action handlers: new, open, save, save-as, find, goto, zoom, toggle binary, settings
- Settings dialog with theme, font, GUI font, bytes per row, show ASCII, uppercase hex
- Keyboard shortcut registration (Ctrl+O, Ctrl+S, Ctrl+F, Ctrl+G, Ctrl+B, etc.)

### settings.c (116 lines)
- Loads/saves settings from `~/.config/hex-editor/settings.conf`
- Simple key=value format, atomic write (write to .tmp, then rename)
- Sensible defaults (Monospace 13pt, system theme, 16 bytes/row)

## Key Data Structures

### HexWindow (window.h)
Central application state:
- `data` / `data_len` / `data_alloc` — File buffer with dynamic growth
- `original_data` / `original_len` — Original content for dirty detection and modified-byte highlighting
- `cursor_pos` / `cursor_nibble` — Cursor position (byte offset + nibble/bit within byte)
- `editing_ascii` — Whether cursor is in ASCII or hex/bin pane
- `scroll_offset` / `visible_rows` — Virtual scroll state
- `selection_start` / `selection_end` — Byte selection range

### HexSettings (settings.h)
Persisted configuration:
- Font settings (editor + GUI)
- Theme name
- Display preferences (bytes per row, ASCII visibility, hex case, display mode)
- Window geometry
- Last opened file path

## Rendering Pipeline

1. `draw_hex_view()` is called by GTK on each frame
2. Font metrics are measured once (cached until settings change)
3. Scrollbar adjustment is updated based on total rows
4. For each visible row:
   - Offset column (hex address)
   - Data bytes in hex or binary format
   - Selection/cursor highlights (blue=hex active, green=ASCII active, grey=cross-reference)
   - ASCII column with separator line
5. Modified bytes are highlighted in red

## Display Modes

- **Hex mode** (default) — 2 chars per byte (e.g., `FF`), edit with 0-9/A-F keys
- **Binary mode** — 8 chars per byte (e.g., `01101001`), edit with 0/1 keys
- Toggle with Ctrl+B or menu

## Dependencies

- GTK 4
- libadwaita-1
- C17 compiler (gcc)
- pkg-config
