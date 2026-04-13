// Phase 3 tests: trash in flick::NoteStore.

#include "../src/core/core_notestore.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path g_tmp;

static void setupIsolatedStorage() {
    g_tmp = fs::temp_directory_path() / "flick_trash_test";
    fs::remove_all(g_tmp);
    fs::create_directories(g_tmp);
    setenv("XDG_DATA_HOME", g_tmp.c_str(), 1);
}

static fs::path trashDir() {
    return g_tmp / "flick" / "trash";
}

static int countTrashFiles() {
    if (!fs::exists(trashDir())) return 0;
    int n = 0;
    for (auto &e : fs::directory_iterator(trashDir()))
        if (e.is_regular_file()) ++n;
    return n;
}

static void test_delete_nonempty_moves_to_trash() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.setCurrentText("hello world");
    s.createNote();
    s.setCurrentText("second");
    // delete index 0 (second)
    s.deleteNote(0);
    auto entries = s.listTrash();
    assert(entries.size() == 1);
    assert(entries[0].preview.find("second") != std::string::npos);
    assert(countTrashFiles() == 1);
    std::cout << "OK test_delete_nonempty_moves_to_trash\n";
}

static void test_delete_empty_no_trash() {
    setupIsolatedStorage();
    flick::NoteStore s;
    // current note is empty; delete should not create trash
    assert(s.noteCount() == 1);
    s.createNote(); // now 2 notes, both empty, current=0
    s.deleteNote(0);
    assert(s.listTrash().empty());
    std::cout << "OK test_delete_empty_no_trash\n";
}

static void test_restore_inserts_at_front_with_new_id() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.setCurrentText("restore me");
    s.createNote();
    s.setCurrentText("keeper");
    auto keeperId = s.currentNoteId();
    s.deleteNote(1); // delete "restore me"
    auto entries = s.listTrash();
    assert(entries.size() == 1);
    bool ok = s.restoreFromTrash(entries[0].id);
    assert(ok);
    assert(s.noteCount() == 2);
    assert(s.currentIndex() == 0);
    assert(s.getText(0) == "restore me");
    // keeper still present and retains its id
    assert(s.noteIdAt(1) == keeperId);
    // new note gets fresh id
    assert(s.currentNoteId() != keeperId);
    assert(s.listTrash().empty());
    std::cout << "OK test_restore_inserts_at_front_with_new_id\n";
}

static void test_delete_all_moves_nonempty() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.setCurrentText("one");
    s.createNote();
    s.setCurrentText("two");
    s.createNote();
    s.setCurrentText(""); // empty
    s.deleteAllNotes();
    auto entries = s.listTrash();
    assert(entries.size() == 2);
    std::cout << "OK test_delete_all_moves_nonempty\n";
}

static void test_purge_and_empty() {
    setupIsolatedStorage();
    flick::NoteStore s;
    s.setCurrentText("a");
    s.createNote();
    s.setCurrentText("b");
    s.createNote();
    s.setCurrentText("c");
    s.deleteNote(0);
    s.deleteNote(0);
    auto entries = s.listTrash();
    assert(entries.size() == 2);
    assert(s.purgeFromTrash(entries[0].id));
    assert(s.listTrash().size() == 1);
    s.emptyTrash();
    assert(s.listTrash().empty());
    assert(countTrashFiles() == 0);
    std::cout << "OK test_purge_and_empty\n";
}

int main() {
    test_delete_nonempty_moves_to_trash();
    test_delete_empty_no_trash();
    test_restore_inserts_at_front_with_new_id();
    test_delete_all_moves_nonempty();
    test_purge_and_empty();
    std::cout << "All trash tests passed.\n";
    return 0;
}
