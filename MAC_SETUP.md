# Building Flick on macOS

## Prerequisites

```bash
brew install qt6 cmake
```

## Clone and Build

```bash
git clone https://github.com/pascalwiemers/flick.git
cd flick
cmake -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt6)
cmake --build build
open build/flick.app
```

## Known Issues to Investigate

- **`startSystemResize()`** — the custom resize handles may not work on macOS with frameless windows. If resizing is broken, you may need to implement a QML-based drag resize fallback for macOS.
- **Frameless window dragging** — the drag-to-move MouseArea should work but test behavior on different macOS versions.
- **Font fallback** — the app prefers JetBrains Mono / Fira Code / Cascadia Code, falling back to "monospace". macOS should resolve these fine if installed, otherwise SF Mono is a good macOS-native fallback to add.
- **Git CLI** — required for GitHub sync. Ships with Xcode command line tools (`xcode-select --install` if not present).

## Creating a .app Bundle for Distribution

Once it builds and runs:

```bash
# Build in release mode
cmake -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Bundle Qt frameworks into the .app
$(brew --prefix qt6)/bin/macdeployqt build/flick.app -qmldir=src/qml

# Create a .dmg
hdiutil create -volname Flick -srcfolder build/flick.app -ov flick.dmg
```

## CI Workflow

Once the macOS build works locally, add a macOS job to `.github/workflows/build-appimage.yml` using `macos-14` (Apple Silicon) runner with aqtinstall for Qt. This will produce a `.dmg` alongside the Linux AppImage on tag pushes.
