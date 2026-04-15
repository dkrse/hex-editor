# Changelog

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
- Mouse wheel scrolling with virtual scrollbar
- Save confirmation dialog on close with unsaved changes
- Zoom in/out (Ctrl+Plus/Minus)
- 13 color themes: System, Light, Dark, Solarized Light/Dark, Monokai, Gruvbox Light/Dark, Nord, Dracula, Tokyo Night, Catppuccin Latte/Mocha
- Settings dialog: theme, editor font, GUI font, bytes per row (8/16/32), show ASCII, uppercase hex
- Persistent settings in `~/.config/hex-editor/settings.conf`
- Remembers last opened file and window size
- Desktop integration (.desktop file, SVG/PNG icon)
- Status bar: current offset, byte value (hex + binary + decimal + ASCII), file size

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
