# macOS Parity Validation — Undo/Redo + Trash

Manual smoke test for Phase 2 (undo/redo) and Phase 3 (trash) on macOS.
Run after building on a Mac. Linux-side equivalents already verified via
unit tests (`flick_core_ids`, `flick_core_history`, `flick_core_trash`) and
the Qt app.

## Build

```sh
cmake -B build
cmake --build build
./build/flick.app/Contents/MacOS/flick   # or open build/flick.app
```

## Storage path

On macOS, notes live under `~/Library/Application Support/flick/` and
trashed notes under `~/Library/Application Support/flick/trash/`.

Inspect with:

```sh
ls -la "$HOME/Library/Application Support/flick/trash"
```

---

## Undo / Redo

NSTextView's built-in undo is **disabled** (`allowsUndo = NO`). All undo
goes through the core `NoteStore` so it survives note switches.

### U1 — basic undo/redo

1. New note (`Cmd+N`).
2. Type `hello`.
3. Wait >500 ms.
4. Type ` world`.
5. `Cmd+Z` → text shows `hello`.
6. `Cmd+Z` → text empty.
7. `Cmd+Shift+Z` → `hello`.
8. `Cmd+Shift+Z` → `hello world`.

**Pass**: each step matches.

### U2 — coalescing

1. Empty note. Type `abcdefghij` quickly (<500 ms, <20 chars).
2. `Cmd+Z` once → text empty (one step, not ten).

**Pass**: single undo clears it.

### U3 — per-note stack survives switching

1. Note A: type `alpha`, wait, type ` one`.
2. `Cmd+Right` → new note B.
3. Note B: type `beta`, wait, type ` two`.
4. `Cmd+Left` back to note A.
5. `Cmd+Z` → A shows `alpha` (not `beta ...`).
6. `Cmd+Z` → A empty.
7. `Cmd+Right` to B.
8. `Cmd+Z` → B shows `beta`.

**Pass**: each note's stack is independent and preserved.

### U4 — resign-key commits pending edit

1. Type `foo` in a note.
2. Click another app (window resigns key).
3. Return, `Cmd+Z` → text becomes empty.

**Pass**: pending edit was committed as a snapshot on blur.

### U5 — redo cleared on new edit

1. Type `one`, wait, type ` two`.
2. `Cmd+Z` → `one`.
3. Type ` three` → `one three`.
4. `Cmd+Shift+Z` → no-op (redo stack cleared).

**Pass**: redo is empty after new edit.

---

## Trash

Deleted non-empty notes move to `trash/`. Empty notes are dropped without
trashing.

### T1 — delete moves to trash

1. New note, type `trash me`.
2. `Cmd+W` (delete note).
3. Check `~/Library/Application Support/flick/trash/` — one file present,
   filename matches `YYYY-MM-DDTHH-MM-SS-mmm_trash_me.txt`.

**Pass**: file exists with slug derived from first line.

### T2 — empty-note delete does nothing

1. New empty note.
2. `Cmd+W`.
3. Trash directory unchanged.

**Pass**: no new trash file.

### T3 — Trash window restore

1. Delete a note containing `restore me`.
2. Right-click editor → **Trash…** (or `Cmd+Shift+T`).
3. Trash window lists the entry with its preview.
4. Click **Restore**.
5. Trash window shows `Trash empty`.
6. Main editor now shows `restore me` at index 0 with fresh note ID.

**Pass**: note is back, trash cleared of that entry.

### T4 — Trash window purge

1. Delete a note.
2. Open Trash window, click **Delete** next to the entry.
3. File removed from `trash/`. Entry gone from list.

**Pass**: purge removes file immediately.

### T5 — Empty Trash

1. Delete 3 notes.
2. Open Trash window, click **Empty Trash**.
3. `trash/` directory empty. List shows `Trash empty`.

**Pass**: all trash files gone.

### T6 — `Cmd+Option+Z` restores most recent

1. Delete note `A`, then note `B`, then note `C`.
2. `Cmd+Option+Z`.
3. Most recent (`C`) comes back at index 0.
4. Press again → `B` comes back.
5. Press again → `A` comes back.

**Pass**: LIFO restore works, no collision with `Cmd+Shift+Z` (redo).

### T7 — delete-all moves everything

1. Have 3 non-empty notes and 1 empty note.
2. Trigger **Delete All** (if exposed in UI) or delete each via `Cmd+W`.
3. `trash/` contains the 3 non-empty ones.

**Pass**: empty note is not trashed, rest are.

### T8 — retention

Only smoke-test manually; full retention (30 days / 100 entries) covered
by Linux unit tests. Spot check:

1. Touch 101 junk files in `trash/` (or delete 101 notes).
2. Open Trash window.
3. List contains at most 100 entries; oldest pruned.

---

## Restart persistence

1. Delete a note containing `persist me`.
2. Quit (`Cmd+Q`).
3. Relaunch. Open Trash window.
4. Entry still present → can restore.

Undo history is **intentionally not persisted**. After relaunch,
`Cmd+Z` in a note is a no-op until new edits create history.

---

## Keyboard map summary

| Shortcut | Action |
|---|---|
| `Cmd+Z` | Undo (per note, core-managed) |
| `Cmd+Shift+Z` | Redo |
| `Cmd+Option+Z` | Restore most recently trashed note |
| `Cmd+Shift+T` | Open Trash window |
| `Cmd+W` | Delete current note (→ trash if non-empty) |

## Known non-goals

- Undo history is session-only (not persisted across restart).
- Note IDs are session-only.
- `Cmd+Z` does not undo a `Cmd+W` delete. Use Trash window or `Cmd+Option+Z`.
- Trash is local only — not synced via GitHub.
