# Flick

A fast, minimal note-taking scratchpad for Linux and macOS. Swipe between notes, evaluate math inline, manage checklists, and preview markdown.

## Features

- **Multiple notes** with animated swipe transitions
- **Inline math** — start a note with `math:` and type expressions to see results appear inline
- **Checklist mode** — start a note with `list` or `list: Title` to turn it into a checklist
- **Markdown preview** — toggle with `Ctrl+M`
- **Grid overview** — see all notes at a glance with `Shift+Tab`
- **AutoPaste** — collect clipboard contents automatically with `Ctrl+Shift+V`
- **Frameless UI** with draggable window and resizable edges

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New note |
| `Ctrl+W` | Delete current note |
| `Ctrl+Left/Right` | Previous / next note |
| `Ctrl+Scroll` | Navigate notes with mouse wheel |
| `Shift+Tab` | Grid overview |
| `Ctrl+M` | Toggle markdown preview |
| `Ctrl+Shift+V` | Toggle AutoPaste |
| `Ctrl+=` / `Ctrl+-` | Increase / decrease font size |
| `Ctrl+D` | Toggle dark / light mode |
| `Ctrl+G` | GitHub sync |
| `Ctrl+Q` | Quit |

## Checklist Mode

Type `list` or `list: Shopping` on the first line of a note to activate checklist mode. Every non-empty line below becomes a checkbox item.

- **Check off items** — click the checkbox or type `/x` at the end of a line
- **Comments** — start a line with `//` to add a comment (no checkbox)
- **Headings** — start a line with `#`, `##`, or `###` for section headings

Checked items get a strikethrough and are dimmed. The `/x` marker is part of the text so it persists across sessions.

## Install

Download the latest release from [Releases](https://github.com/pascalwiemers/flick/releases):

- **Linux** — `.AppImage`
- **macOS** — `.dmg`

```
# Linux
chmod +x flick-*.AppImage
./flick-*.AppImage
```

## Build from Source

Requires Qt 6 and CMake 3.20+.

```
cmake -B build
cmake --build build
./build/flick
```
