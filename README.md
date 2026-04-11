# Flick

A fast, minimal note-taking scratchpad for Linux. Swipe between notes, evaluate math inline, and preview markdown.

## Features

- **Multiple notes** with animated swipe transitions
- **Inline math** — start a note with `math:` and type expressions to see results appear inline
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
| `Ctrl+Q` | Quit |

## Install

Download the latest `.AppImage` from [Releases](https://github.com/pascalwiemers/flick/releases):

```
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
