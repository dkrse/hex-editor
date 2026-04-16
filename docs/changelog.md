# Changelog

## v1.2.0 — 2026-04-16

### Editing Features
- Undo/Redo (Ctrl+Z / Ctrl+Shift+Z) — per-byte undo stack with insert tracking
- Copy (Ctrl+C) — copies selection as hex string (`FF D8 FF`)
- Paste (Ctrl+V) — parses hex or ASCII text from clipboard
- Find & Replace (Ctrl+H) — replace current match or replace all (same-length patterns)

### Analysis Features
- **Entropy visualization** — color bar below search bar showing Shannon entropy per block across the file; blue = low entropy, green = medium, red = high (compressed/encrypted); click to navigate
- **Byte frequency histogram** — dialog with 256-bar chart and top-10 most frequent bytes
- **Strings extraction** — dialog listing all printable ASCII strings (min 4 chars) with offsets; click to navigate and select
- **Checksums** — CRC32, MD5, SHA1, SHA256 of file or selection with Copy All button

### Display Features
- **Data Inspector** — right panel showing cursor value as int8, int16/32/64 LE/BE, float LE, double LE; auto-detects file magic bytes (PNG, JPEG, PDF, ELF, ZIP, GZIP, GIF, BMP, RIFF, MP4, PE, Mach-O, XML, ICO)
- **Diff indicator** — offset column turns red for rows containing modified bytes
- Data Inspector auto-hides in binary display mode

### Security Fixes
- **CRITICAL**: Fixed buffer overflow in `hex_window_copy` — `g_malloc(count*3)` → `g_malloc(count*3+1)` with `snprintf`
- **CRITICAL**: Fixed integer overflow in auto-grow buffer — added overflow guards on all 3 allocation paths (hex, binary, ASCII editing + paste)
- **CRITICAL**: Strengthened SSH path traversal check — now rejects any `..` or `//` in path suffix
- **HIGH**: Fixed out-of-bounds write in paste — zero-fills gap between `data_len` and target position
- **HIGH**: Fixed `match_current` bounds check in Find & Replace — prevents out-of-bounds read on `match_offsets`
- **HIGH**: Replaced `atoi` with `strtol` for SSH port validation — validates range 1-65535
- **MEDIUM**: Added SSH host/user input validation — `ssh_validate_str()` rejects empty, overlong, and newline-containing strings
- **MEDIUM**: Replaced O(n²) bubble sort with `qsort` in byte frequency analysis

### Bug Fixes
- Removed all debug `fprintf(stderr)` statements
- SSH actions (Open Remote, Disconnect) disabled at startup when not connected

## v1.1.1 — 2026-04-16

### Bug Fixes
- **MEDIUM**: Fixed `last_file` not persisting across restarts — `snprintf` overlapping source/destination buffer caused undefined behavior when reloading the last opened file from settings
- Replaced scrollbar widget with scroll percentage indicator in status bar (scrollbar was invisible in GTK4 overlay mode)
- Prevent duplicate window creation when `on_activate` is called multiple times by GApplication

## v1.1.0 — 2026-04-15

### SSH/SFTP Support
- SSH Connect dialog with saved connection profiles (name, host, port, user, key)
- Async SSH connection testing in background thread
- Remote file browser with directory navigation via `ssh ls -1pA`
- Remote file open via `ssh cat` (binary-safe)
- Remote file save via `ssh tee` (binary-safe)
- SSH ControlMaster for multiplexed connections
- SSH status button in header bar showing connection state
- Connections stored in `~/.config/hex-editor/connections.conf` (chmod 0600)

### Security Fixes
- **CRITICAL**: Fixed path traversal vulnerability in `ssh_to_remote_path` — now validates prefix match and rejects `..` components
- **CRITICAL**: Fixed unchecked pointer arithmetic when converting remote paths
- **HIGH**: Fixed potential heap buffer overflow in `parse_hex_string` — added bounds checking and proper allocation size
- **HIGH**: Fixed `sscanf` without return value check — now validates parse success
- **HIGH**: Fixed search pattern integer overflow — added `plen` limit (4096) and safe `gsize` cast
- **MEDIUM**: Fixed CSS provider memory leak — now properly removed and unreffed on window destroy
- **MEDIUM**: Fixed missing `ferror()` check after `fread` — buffer freed on read failure
- **MEDIUM**: Fixed potential NULL dereference after `g_memdup2` — added NULL-safe handling
- **MEDIUM**: Fixed `strtoull` without `errno` check in goto-offset — now validates with `errno` and `endptr`
- **MEDIUM**: Fixed NULL dereference in SSH error dialog — defensive check for `err` pointer
- **LOW**: Fixed unsigned underflow in Right arrow with empty file — early return when `data_len == 0`
- **LOW**: Fixed Page Down overflow — safe addition with bounds check before increment
- **LOW**: Fixed truncation warnings in SSH path buffers — enlarged to safe sizes

### Improvements
- New file starts with 0 bytes (was 1 dummy byte) — auto-grows from truly empty
- Editing works from empty file in all modes (hex, binary, ASCII)

## v1.0.0 — 2026-04-15

### Initial Release

#### Features
- Hex view with offset, hex bytes, and ASCII columns
- Binary display mode (toggle with Ctrl+B)
- Direct hex editing (0-9, A-F keys)
- Direct binary editing (0, 1 keys)
- ASCII editing (Tab to switch pane)
- Cross-highlight between hex/bin and ASCII panes (blue = hex active, green = ASCII active)
- File operations: New, Open, Save, Save As (all with Ctrl shortcuts)
- Atomic file writes (write to .tmp, rename)
- Search by hex pattern (e.g., `FF D8`) or ASCII text
- Go to offset dialog (hex or decimal input)
- Auto-growing buffer for new files
- Modified bytes highlighted in red
- Selection with Shift+arrow keys
- Mouse click to position cursor
- Mouse wheel scrolling with scroll percentage in status bar
- Save confirmation dialog on close with unsaved changes
- Zoom in/out (Ctrl+Plus/Minus)
- 13 color themes: System, Light, Dark, Solarized Light/Dark, Monokai, Gruvbox Light/Dark, Nord, Dracula, Tokyo Night, Catppuccin Latte/Mocha
- Settings dialog: theme, editor font, GUI font, bytes per row (8/16/32), show ASCII, uppercase hex
- Persistent settings in `~/.config/hex-editor/settings.conf`
- Remembers last opened file and window size
- Desktop integration (.desktop file, SVG/PNG icon)
- Status bar: current offset, byte value (hex + binary + decimal + ASCII), file size, scroll percentage

#### Keyboard Shortcuts
| Shortcut | Action |
|---|---|
| Ctrl+N | New file |
| Ctrl+O | Open file |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save as |
| Ctrl+F | Find |
| Ctrl+G | Go to offset |
| Ctrl+B | Toggle hex/binary mode |
| Ctrl+Plus | Zoom in |
| Ctrl+Minus | Zoom out |
| Ctrl+Q | Quit |
| Tab | Switch hex/ASCII pane |
| Shift+Arrows | Select bytes |
| Page Up/Down | Scroll page |
| Ctrl+Home/End | Jump to start/end |
