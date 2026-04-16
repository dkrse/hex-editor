# Hex Editor

A lightweight native hex editor built with GTK4 and libadwaita.


## Features

- **Hex and Binary view** — switch with Ctrl+B
- **Edit** hex bytes, binary bits, or ASCII text directly
- **Undo/Redo** — full undo history (Ctrl+Z / Ctrl+Shift+Z)
- **Copy/Paste** — copy selection as hex, paste hex or ASCII (Ctrl+C / Ctrl+V)
- **Find & Replace** — search and replace hex patterns or ASCII text (Ctrl+H)
- **Export selection** — C array, Python bytes, Base64, hex dump
- **Cross-highlight** — cursor position shown in both hex/bin and ASCII panes with distinct colors (blue=hex, green=ASCII)
- **Data Inspector** — right panel showing value at cursor as int8/16/32/64, float, double (LE/BE), with magic byte detection (toggleable in Settings)
- **Entropy visualization** — color bar showing file entropy per block (blue=low, red=high), click to navigate
- **Byte frequency histogram** — bar chart of all 256 byte values with top-10 list
- **Strings extraction** — find all ASCII strings with offsets, click to navigate
- **Checksums** — CRC32, MD5, SHA1, SHA256 of file or selection
- **Search** — find hex patterns (`FF D8 FF`) or ASCII text
- **Go to offset** — jump to any position (hex or decimal)
- **Recent files** — quick access to last 10 opened files
- **Drag & drop** — drop a file onto the window to open it
- **Command line** — `hex-editor file.bin` opens a file directly
- **SSH/SFTP** — connect to remote servers, browse and edit remote binary files
- **13 themes** — System, Light, Dark, Solarized, Monokai, Gruvbox, Nord, Dracula, Tokyo Night, Catppuccin
- **Modified bytes** highlighted in red, modified rows highlighted in offset column
- **Auto-growing buffer** — create new files from scratch, buffer expands as you type
- **Save protection** — prompts Save/Discard/Cancel before closing with unsaved changes
- **Scroll percentage** — current scroll position shown in status bar
- **Persistent settings** — font, theme, layout preferences, SSH connections remembered

## Build

Requirements: `gcc`, `pkg-config`, `libadwaita-1-dev`, `libgtk-4-dev`

```bash
sudo apt install libadwaita-1-dev libgtk-4-dev
make
```

## Run

```bash
./build/hex-editor [file.bin]
```

## Install Desktop Shortcut

The desktop file is installed automatically at `~/.local/share/applications/hex-editor.desktop`.

To reinstall manually:

```bash
cp ~/.local/share/applications/hex-editor.desktop ~/.local/share/applications/
update-desktop-database ~/.local/share/applications/
```

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| Ctrl+N | New file |
| Ctrl+O | Open file |
| Ctrl+S | Save |
| Ctrl+Shift+S | Save as |
| Ctrl+Z | Undo |
| Ctrl+Shift+Z | Redo |
| Ctrl+C | Copy (hex) |
| Ctrl+V | Paste (hex or ASCII) |
| Ctrl+F | Find (hex or text) |
| Ctrl+H | Find & Replace |
| Ctrl+G | Go to offset |
| Ctrl+B | Toggle hex/binary |
| Ctrl+Plus/Minus | Zoom in/out |
| Ctrl+Q | Quit |
| Tab | Switch hex/ASCII pane |
| Shift+Arrows | Select bytes |
| Page Up/Down | Scroll page |
| Ctrl+Home/End | Jump to start/end |

## SSH/SFTP

1. Open SSH Connect dialog from the menu or click the "SSH: Off" button in the header
2. Enter connection details (host, user, port, SSH key) and save the profile
3. Click Connect — the connection is tested asynchronously
4. Use "Open Remote File" to browse and open remote binary files
5. Save (Ctrl+S) writes back to the remote server via SSH

Connections are stored in `~/.config/hex-editor/connections.conf`.

## Configuration

Settings are stored in `~/.config/hex-editor/settings.conf`.

## Documentation

- [Architecture](docs/architecture.md)
- [Changelog](docs/changelog.md)

## Author

krse

## License

MIT
