# Hex Editor — Architecture

## Overview

Hex Editor is a native GTK4/libadwaita binary file editor written in C17 (~3000 lines). It follows the same architecture as [Notes Light](https://github.com/), with a clean separation between window management, actions/dialogs, persistent settings, and SSH transport.

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
│   ├── main.c            # Entry point, GApplication setup (30 lines)
│   ├── window.c          # Window, hex rendering, input, SSH ops (1346 lines)
│   ├── window.h          # HexWindow struct, public API
│   ├── actions.c         # GAction handlers, dialogs, SFTP UI (1044 lines)
│   ├── actions.h         # hex_actions_setup() declaration
│   ├── settings.c        # Config + connections load/save (179 lines)
│   ├── settings.h        # HexSettings, SftpConnection structs
│   ├── ssh.c             # SSH transport: ControlMaster, cat, tee (255 lines)
│   └── ssh.h             # SSH API declarations
└── build/                # Build output (generated)
```

## Module Responsibilities

### main.c (30 lines)
- Creates `AdwApplication`
- Connects `activate` signal to create the window
- Registers app-level actions (quit)

### window.c (1346 lines)
Core of the application:

- **Theme system** — 13 color themes, CSS generation, Adwaita style manager
- **Hex rendering** — Custom `GtkDrawingArea` with Cairo/Pango. Renders offset column, hex/binary bytes, ASCII pane, cursor with cross-highlights, selection, modified-byte markers
- **Input handling** — Key controller on the window for hex/binary/ASCII editing, navigation, selection. Auto-growing buffer for new files
- **File I/O** — Load files up to 64 MB with error checking, atomic saves
- **Search** — Hex pattern or ASCII text search with bounds-checked parsing
- **Go to offset** — Dialog with errno-safe strtoull parsing
- **Close protection** — Save/Discard/Cancel dialog via `close-request` signal
- **Scrollbar** — Virtual scrolling with `GtkAdjustment`, mouse wheel
- **SSH operations** — Connect/disconnect, remote file open via `ssh cat`, remote save via `ssh tee`, SSH status button in header bar

### actions.c (1044 lines)
- All `GAction` entries (new, open, save, find, goto, zoom, toggle binary, settings, SSH connect/disconnect/open-remote)
- Settings dialog (theme, fonts, bytes per row, ASCII, uppercase, display mode)
- SFTP connection dialog with saved connections list, async SSH test
- Remote file browser dialog with directory navigation via `ssh ls`
- Keyboard shortcut registration

### settings.c (179 lines)
- Settings load/save from `~/.config/hex-editor/settings.conf`
- SFTP connections load/save from `~/.config/hex-editor/connections.conf`
- Simple key=value format, atomic writes, chmod 0600 for connections

### ssh.c (255 lines)
- `ssh_argv_new()` — builds SSH command with proper flags (BatchMode, StrictHostKeyChecking, ControlPath)
- `ssh_ctl_start/stop()` — ControlMaster lifecycle for multiplexed connections
- `ssh_cat_file()` — binary-safe remote file reading via `GSubprocess`
- `ssh_write_file()` — binary-safe remote file writing via `tee` + stdin pipe
- `ssh_path_is_remote()` — detect virtual remote paths
- `ssh_to_remote_path()` — safe path conversion with traversal protection

## Key Data Structures

### HexWindow (window.h)
Central application state:
- `data` / `data_len` / `data_alloc` — File buffer with dynamic growth
- `original_data` / `original_len` — Original content for dirty detection (NULL-safe)
- `cursor_pos` / `cursor_nibble` — Cursor position (byte + nibble/bit within byte)
- `editing_ascii` — Whether cursor is in ASCII or hex/bin pane
- `scroll_offset` / `visible_rows` — Virtual scroll state
- `selection_start` / `selection_end` — Byte selection range
- `ssh_*` — SSH connection state (host, user, port, key, ControlMaster paths)
- `css_provider` — Managed CSS provider (properly unreffed on destroy)

### HexSettings (settings.h)
Persisted configuration:
- Font settings (editor + GUI), theme, display mode (hex/binary)
- Display preferences (bytes per row, ASCII visibility, hex case)
- Window geometry, last opened file

### SftpConnection / SftpConnections (settings.h)
- Up to 32 saved SSH connection profiles
- Stored with chmod 0600 for security

## Rendering Pipeline

1. `draw_hex_view()` called by GTK on each frame
2. Font metrics measured once (cached until settings change)
3. Scrollbar adjustment updated based on total rows
4. For each visible row:
   - Offset column (hex address)
   - Data bytes in hex (`XX`) or binary (`XXXXXXXX`) format
   - Selection/cursor highlights: **blue** = hex/bin active, **green** = ASCII active, **grey** = cross-reference mirror
   - ASCII column with separator line
5. Modified bytes highlighted in red vs original

## Display Modes

- **Hex mode** (default) — 2 chars per byte, edit with 0-9/A-F
- **Binary mode** — 8 chars per byte, edit with 0/1
- Toggle with Ctrl+B or menu

## Security Measures

- SSH path conversion validates prefix match and rejects `..` traversal
- Search pattern length capped at 4096 bytes
- `sscanf` return values checked, output masked to 0xFF
- `strtoull` uses errno + endptr validation
- `fread` checked with `ferror()`, buffer freed on failure
- `g_memdup2` results NULL-checked before use
- CSS provider properly removed and unreffed on window destroy
- Connection config files written with chmod 0600
- SSH uses BatchMode=yes, ConnectTimeout=10
- Empty-file edge cases handled (data_len=0 guard in navigation)

## Dependencies

- GTK 4
- libadwaita-1
- C17 compiler (gcc)
- pkg-config
- OpenSSH client (for SSH/SFTP features)
