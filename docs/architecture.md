# Hex Editor — Architecture

## Overview

Hex Editor is a native GTK4/libadwaita binary file editor written in C17 (~4300 lines). It follows the same architecture as [Notes Light](https://github.com/), with a clean separation between window management, actions/dialogs, persistent settings, SSH transport, and analysis tools.

## Project Structure

```
hex-editor/
├── Makefile              # Build system (gcc, pkg-config)
├── README.md
├── LICENSE               # MIT
├── docs/
│   ├── architecture.md   # This file
│   └── changelog.md
├── images/
│   ├── hex-editor.svg    # Application icon (source)
│   └── hex-editor.png    # Application icon (256x256)
├── src/
│   ├── main.c            # Entry point, GApplication, CLI args, crash handler (64 lines)
│   ├── window.c          # Window, hex rendering, input, undo, copy/paste, drag&drop (1841 lines)
│   ├── window.h          # HexWindow struct, HexUndoEntry, public API
│   ├── actions.c         # GAction handlers, dialogs, SFTP UI, export, recent files (1293 lines)
│   ├── actions.h         # hex_actions_setup() declaration
│   ├── analysis.c        # Entropy, byte frequency, strings, checksums (453 lines)
│   ├── analysis.h        # Analysis function declarations
│   ├── settings.c        # Config + connections + recent files load/save (222 lines)
│   ├── settings.h        # HexSettings, SftpConnection structs
│   ├── ssh.c             # SSH transport: ControlMaster, cat, tee, validation (266 lines)
│   └── ssh.h             # SSH API declarations
└── build/                # Build output (generated)
```

## Module Responsibilities

### main.c (64 lines)
- Creates `AdwApplication` with `G_APPLICATION_HANDLES_OPEN` for CLI file arguments
- Connects `activate` and `open` signals (guards against duplicate activation)
- Registers app-level actions (quit)
- Installs SIGSEGV/SIGABRT crash handler with backtrace output

### window.c (1841 lines)
Core of the application:

- **Theme system** — 13 color themes, CSS generation, Adwaita style manager
- **Hex rendering** — Custom `GtkDrawingArea` with Cairo/Pango. Renders offset column (red for modified rows), hex/binary bytes, ASCII pane, cursor with cross-highlights, selection, modified-byte markers
- **Input handling** — Key controller on the window for hex/binary/ASCII editing, navigation, selection. Auto-growing buffer with overflow-safe allocation
- **Undo/Redo** — Per-byte undo stack (`HexUndoEntry`) tracking offset, old/new value, and insert flag. Redo support via position tracking
- **Copy/Paste** — Copy selection as hex string to clipboard, paste with hex or ASCII parsing. Async clipboard read via `gdk_clipboard_read_text_async`
- **Data Inspector** — Right panel showing cursor value as int8/16/32/64 LE/BE, float, double, with magic byte detection (PNG, JPEG, PDF, ELF, ZIP, etc.). Toggleable via Settings
- **Drag & drop** — `GtkDropTarget` accepting file lists, opens first dropped file
- **File I/O** — Load files up to 64 MB with error checking, atomic saves, alias-safe path handling for `last_file` persistence
- **Search & Replace** — Hex pattern or ASCII text search with bounds-checked parsing, replace current or replace all (same-length)
- **Go to offset** — Dialog with errno-safe strtoull parsing
- **Close protection** — Save/Discard/Cancel dialog via `close-request` signal
- **Scroll percentage** — Virtual scrolling with mouse wheel, scroll position shown as percentage in status bar
- **SSH operations** — Connect/disconnect, remote file open via `ssh cat`, remote save via `ssh tee`, SSH status button in header bar

### actions.c (1293 lines)
- All `GAction` entries (new, open, save, find, find-replace, goto, undo, redo, copy, paste, zoom, toggle binary, export, settings, SSH, analysis)
- **Recent files** — dynamic `recent-N` actions, submenu in main menu, files loaded from settings
- **Export selection** — dialog with C array, Python bytes, Base64, hex dump formats, dropdown switcher, copy to clipboard
- Settings dialog (theme, fonts, bytes per row, ASCII, uppercase, display mode, Data Inspector toggle)
- SFTP connection dialog with saved connections list, async SSH test
- Remote file browser dialog with directory navigation via `ssh ls`
- Keyboard shortcut registration

### analysis.c (453 lines)
- **Entropy visualization** — Shannon entropy per block, `GtkDrawingArea` with blue-green-red gradient, click to navigate
- **Byte frequency histogram** — Dialog with 256-bar Cairo chart, top-10 sorted with `qsort`
- **Strings extraction** — Scans for printable ASCII runs (min 4 chars), dialog with clickable list
- **Checksums** — CRC32 (custom table), MD5/SHA1/SHA256 via `GChecksum`, supports selection range

### settings.c (222 lines)
- Settings load/save from `~/.config/hex-editor/settings.conf`
- SFTP connections load/save from `~/.config/hex-editor/connections.conf`
- **Recent files** — stores up to 10 recent file paths, `hex_settings_add_recent()` manages MRU list
- Simple key=value format, atomic writes, chmod 0600 for connections

### ssh.c (266 lines)
- `ssh_validate_str()` — Input validation for host/user (length, special chars)
- `ssh_argv_new()` — builds SSH command with proper flags (BatchMode, StrictHostKeyChecking, ControlPath)
- `ssh_ctl_start/stop()` — ControlMaster lifecycle for multiplexed connections
- `ssh_cat_file()` — binary-safe remote file reading via `GSubprocess`
- `ssh_write_file()` — binary-safe remote file writing via `tee` + stdin pipe
- `ssh_path_is_remote()` — detect virtual remote paths
- `ssh_to_remote_path()` — safe path conversion with traversal protection (rejects `..` and `//`)

## Key Data Structures

### HexUndoEntry (window.h)
Per-byte undo record:
- `offset` — byte position in file
- `old_val` / `new_val` — before/after values
- `was_insert` — TRUE if byte was appended (grew `data_len`)

### HexWindow (window.h)
Central application state:
- `data` / `data_len` / `data_alloc` — File buffer with overflow-safe dynamic growth
- `original_data` / `original_len` — Original content for dirty detection (NULL-safe)
- `undo_stack` / `undo_pos` / `undo_count` — Undo/redo history
- `cursor_pos` / `cursor_nibble` — Cursor position (byte + nibble/bit within byte)
- `editing_ascii` — Whether cursor is in ASCII or hex/bin pane
- `scroll_offset` / `visible_rows` — Virtual scroll state (percentage shown in status bar)
- `selection_start` / `selection_end` — Byte selection range
- `inspector_panel` / `inspector_label` — Data Inspector widgets (visibility controlled by settings)
- `entropy_bar` / `entropy_data` / `entropy_blocks` — Entropy visualization state
- `replace_entry` / `replace_box` — Find & Replace UI
- `ssh_*` — SSH connection state (host, user, port, key, ControlMaster paths)
- `css_provider` — Managed CSS provider (properly unreffed on destroy)

### HexSettings (settings.h)
Persisted configuration:
- Font settings (editor + GUI), theme, display mode (hex/binary)
- Display preferences (bytes per row, ASCII visibility, hex case, show_inspector)
- Window geometry, last opened file
- Recent files list (up to 10 MRU paths)

### SftpConnection / SftpConnections (settings.h)
- Up to 32 saved SSH connection profiles
- Stored with chmod 0600 for security

## Rendering Pipeline

1. `draw_hex_view()` called by GTK on each frame
2. Font metrics measured once (cached until settings change)
3. Scroll percentage updated in status bar
4. For each visible row:
   - Offset column (hex address, red if row contains modifications)
   - Data bytes in hex (`XX`) or binary (`XXXXXXXX`) format
   - Selection/cursor highlights: **blue** = hex/bin active, **green** = ASCII active, **grey** = cross-reference mirror
   - ASCII column with separator line
5. Modified bytes highlighted in red vs original
6. Data Inspector updated with cursor value interpretations

## Display Modes

- **Hex mode** (default) — 2 chars per byte, edit with 0-9/A-F, Data Inspector visible (if enabled)
- **Binary mode** — 8 chars per byte, edit with 0/1, Data Inspector hidden
- Toggle with Ctrl+B or menu

## Security Measures

- SSH path conversion rejects any `..` or `//` in path suffix
- SSH host/user validated for length and special characters (`ssh_validate_str`)
- SSH port validated with `strtol` (range 1-65535)
- Buffer auto-grow checks for integer overflow before reallocation
- Copy buffer allocated with +1 for null terminator, uses `snprintf`
- Paste zero-fills memory gaps when writing beyond `data_len`
- Replace checks `match_current` bounds before array access
- Search pattern length capped at 4096 bytes
- `sscanf` return values checked, output masked to 0xFF
- `strtoull` uses errno + endptr validation
- `fread` checked with `ferror()`, buffer freed on failure
- `g_memdup2` results NULL-checked before use
- CSS provider properly removed and unreffed on window destroy
- Connection config files written with chmod 0600
- SSH uses BatchMode=yes, ConnectTimeout=10
- Empty-file edge cases handled (data_len=0 guard in navigation)
- Crash handler installed for SIGSEGV/SIGABRT with backtrace output

## Dependencies

- GTK 4
- libadwaita-1
- C17 compiler (gcc)
- pkg-config
- OpenSSH client (for SSH/SFTP features)
