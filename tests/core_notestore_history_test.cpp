// Phase 2 tests: per-note undo/redo history in flick::NoteStore.

#include "../src/core/core_notestore.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;

static void setupIsolatedStorage() {
    auto tmp = fs::temp_directory_path() / "flick_history_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    setenv("XDG_DATA_HOME", tmp.c_str(), 1);
}

static void sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// 1. Fast typing (< 500 ms between tiny edits) collapses to one undo step.
static void test_fast_edits_coalesce() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.setCurrentText("a");
    s.setCurrentText("ab");
    s.setCurrentText("abc");
    assert(s.canUndo());
    bool ok = s.undo();
    assert(ok);
    // Coalesced: one undo should go back to the pre-"a" state ("").
    assert(s.currentText().empty());
    assert(!s.canUndo());
    std::cout << "  ok: fast edits coalesce\n";
}

// 2. Edits separated by > kCoalesceMs create separate undo steps.
static void test_slow_edits_separate() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.setCurrentText("one");
    sleepMs(600);
    s.setCurrentText("one two");
    sleepMs(600);
    s.setCurrentText("one two three");
    assert(s.undo());
    assert(s.currentText() == "one two");
    assert(s.undo());
    assert(s.currentText() == "one");
    assert(s.undo());
    assert(s.currentText().empty());
    assert(!s.canUndo());
    std::cout << "  ok: slow edits create separate steps\n";
}

// 3. Undo then redo returns to latest.
static void test_undo_redo_symmetry() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.setCurrentText("one");
    sleepMs(600);
    s.setCurrentText("two");
    s.undo();
    assert(s.currentText() == "one");
    assert(s.canRedo());
    s.redo();
    assert(s.currentText() == "two");
    assert(!s.canRedo());
    std::cout << "  ok: undo/redo symmetry\n";
}

// 4. History is per-note: switching notes does not bleed.
static void test_history_per_note() {
    setupIsolatedStorage();
    flick::NoteStore s;                // starts with 1 note at index 0
    s.setCurrentText("A1");
    sleepMs(600);
    s.setCurrentText("A2");
    s.createNote();                    // new note at index 0, A shifted to 1
    assert(s.currentText().empty());
    s.setCurrentText("B1");
    sleepMs(600);
    s.setCurrentText("B2");

    // Undo on B → "B1"
    assert(s.undo());
    assert(s.currentText() == "B1");

    // Switch to A (now at index 1)
    s.setCurrentIndex(1);
    assert(s.currentText() == "A2");

    // Undo on A → "A1"
    assert(s.undo());
    assert(s.currentText() == "A1");

    // Switch back to B — B's history still intact.
    s.setCurrentIndex(0);
    assert(s.currentText() == "B1");
    assert(s.undo());
    assert(s.currentText().empty());
    std::cout << "  ok: per-note history across switches\n";
}

// 5. Deleting a note drops its history.
static void test_delete_drops_history() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.setCurrentText("hello");
    sleepMs(600);
    s.setCurrentText("hello world");
    s.createNote();                    // now 2 notes, current = index 0
    s.setCurrentText("other");
    s.deleteNote(1);                   // drop the "hello..." note
    // Undo on remaining note affects only its own history
    assert(s.canUndo());
    s.undo();
    assert(s.currentText().empty());
    std::cout << "  ok: delete drops history\n";
}

// 6. New edit after undo clears the redo stack.
static void test_edit_after_undo_clears_redo() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.setCurrentText("a");
    sleepMs(600);
    s.setCurrentText("ab");
    s.undo();
    assert(s.currentText() == "a");
    assert(s.canRedo());
    sleepMs(600);
    s.setCurrentText("az");
    assert(!s.canRedo());
    std::cout << "  ok: edit after undo clears redo\n";
}

int main() {
    std::cout << "core_notestore_history_test\n";
    test_fast_edits_coalesce();
    test_slow_edits_separate();
    test_undo_redo_symmetry();
    test_history_per_note();
    test_delete_drops_history();
    test_edit_after_undo_clears_redo();
    std::cout << "all ok\n";
    return 0;
}
