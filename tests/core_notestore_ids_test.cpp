// Phase 1 tests: stable per-note IDs in flick::NoteStore.
//
// Runs against a temp XDG_DATA_HOME so it does not touch the real
// ~/.local/share/flick directory. Linux-only; not built on macOS.

#include "../src/core/core_notestore.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>

namespace fs = std::filesystem;

static void setupIsolatedStorage() {
    auto tmp = fs::temp_directory_path() / "flick_ids_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    setenv("XDG_DATA_HOME", tmp.c_str(), 1);
}

static void test_fresh_store_has_nonzero_stable_id() {
    flick::NoteStore s;
    auto id = s.currentNoteId();
    assert(id != 0);
    assert(s.currentNoteId() == id);
    assert(s.noteIdAt(0) == id);
    assert(s.indexOfNoteId(id) == 0);
    std::cout << "  ok: fresh store id = " << id << "\n";
}

static void test_create_note_assigns_distinct_monotonic_ids() {
    setupIsolatedStorage();
    flick::NoteStore s;
    auto id0 = s.noteIdAt(0);
    s.createNote();
    s.createNote();
    s.createNote();
    assert(s.noteCount() == 4);
    std::set<flick::NoteStore::NoteId> ids;
    for (int i = 0; i < s.noteCount(); ++i) ids.insert(s.noteIdAt(i));
    assert(ids.size() == 4);
    assert(ids.count(id0) == 1);
    // New IDs all greater than the original
    for (auto id : ids)
        assert(id >= id0);
    std::cout << "  ok: distinct monotonic IDs\n";
}

static void test_delete_preserves_other_ids() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.createNote();
    s.createNote();
    s.createNote();
    // indices: [0,1,2,3]
    auto idA = s.noteIdAt(0);
    auto idB = s.noteIdAt(1);
    auto idC = s.noteIdAt(2);
    auto idD = s.noteIdAt(3);

    s.setCurrentIndex(1);  // so deleteNote(1) is unambiguous on layout
    s.deleteNote(1);

    assert(s.noteCount() == 3);
    assert(s.indexOfNoteId(idB) == -1);        // gone
    assert(s.indexOfNoteId(idA) == 0);         // untouched
    assert(s.indexOfNoteId(idC) == 1);         // shifted left
    assert(s.indexOfNoteId(idD) == 2);
    std::cout << "  ok: delete preserves sibling IDs\n";
}

static void test_delete_last_note_clears_but_keeps_id() {
    setupIsolatedStorage();
    flick::NoteStore s;
    // Force non-empty, then back to a single note
    s.setCurrentText("hello");
    assert(s.noteCount() == 1);
    auto id = s.currentNoteId();
    s.deleteNote(0);  // single-note branch: clears content, same slot
    assert(s.noteCount() == 1);
    assert(s.currentNoteId() == id);
    assert(s.currentText().empty());
    std::cout << "  ok: single-note delete keeps ID\n";
}

static void test_round_trip_via_index_of_id() {
    setupIsolatedStorage();
    flick::NoteStore s;
    auto first = s.noteIdAt(0);
    s.createNote();  // new note at index 0
    assert(s.indexOfNoteId(first) == 1);
    assert(s.indexOfNoteId(s.noteIdAt(0)) == 0);
    std::cout << "  ok: indexOfNoteId round-trip\n";
}

static void test_reload_regenerates_fresh_ids() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.setCurrentText("persisted");
    s.createNote();
    s.setCurrentText("second");
    // Capture current IDs for comparison
    std::set<flick::NoteStore::NoteId> before;
    for (int i = 0; i < s.noteCount(); ++i) before.insert(s.noteIdAt(i));

    s.reload();
    assert(s.noteCount() >= 1);
    for (int i = 0; i < s.noteCount(); ++i) {
        // Post-reload IDs are fresh
        assert(before.count(s.noteIdAt(i)) == 0);
    }
    std::cout << "  ok: reload regenerates IDs\n";
}

int main() {
    setupIsolatedStorage();
    std::cout << "core_notestore_ids_test\n";
    test_fresh_store_has_nonzero_stable_id();
    test_create_note_assigns_distinct_monotonic_ids();
    test_delete_preserves_other_ids();
    test_delete_last_note_clears_but_keeps_id();
    test_round_trip_via_index_of_id();
    test_reload_regenerates_fresh_ids();
    std::cout << "all ok\n";
    return 0;
}
