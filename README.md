# Hex Editor

A lightweight native hex editor built with GTK4 and libadwaita.


## Features

- **Hex and Binary view** — switch with Ctrl+B
- **Edit** hex bytes, binary bits, or ASCII text directly
- **Cross-highlight** — cursor position shown in both hex/bin and ASCII panes with distinct colors (blue=hex, green=ASCII)
- **Search** — find hex patterns (`FF D8 FF`) or ASCII text
- **Go to offset** — jump to any position (hex or decimal)
- **SSH/SFTP** — connect to remote servers, browse and edit remote binary files
- **13 themes** — System, Light, Dark, Solarized, Monokai, Gruvbox, Nord, Dracula, Tokyo Night, Catppuccin
- **Modified bytes** highlighted in red
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
./build/hex-editor
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
| Ctrl+F | Find (hex or text) |
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
