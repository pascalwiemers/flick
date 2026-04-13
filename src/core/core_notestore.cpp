#include "core_notestore.h"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

#ifdef __APPLE__
#include <pwd.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace flick {

static void notify(const std::function<void()> &fn) {
    if (fn) fn();
}

static constexpr int kHistoryCap = 200;
static constexpr std::int64_t kCoalesceMs = 500;
static constexpr int kCoalesceSizeDelta = 20;

static std::int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

NoteStore::NoteStore()
{
    loadNotes();
    if (m_notes.empty()) {
        m_notes.emplace_back();
        m_noteIds.push_back(nextId());
    }
    assertInvariants();
}

NoteStore::NoteId NoteStore::nextId() { return m_nextId++; }

void NoteStore::assertInvariants() const
{
    assert(m_noteIds.size() == m_notes.size());
}

NoteStore::NoteId NoteStore::currentNoteId() const
{
    if (m_currentIndex >= 0 && m_currentIndex < (int)m_noteIds.size())
        return m_noteIds[m_currentIndex];
    return 0;
}

NoteStore::NoteId NoteStore::noteIdAt(int index) const
{
    if (index >= 0 && index < (int)m_noteIds.size())
        return m_noteIds[index];
    return 0;
}

int NoteStore::indexOfNoteId(NoteId id) const
{
    for (int i = 0; i < (int)m_noteIds.size(); ++i)
        if (m_noteIds[i] == id) return i;
    return -1;
}

int NoteStore::noteCount() const { return static_cast<int>(m_notes.size()); }
int NoteStore::currentIndex() const { return m_currentIndex; }

void NoteStore::setCurrentIndex(int index)
{
    if (index < 0 || index >= (int)m_notes.size() || index == m_currentIndex)
        return;
    commitHistory();  // pin outgoing note's pending entry
    m_currentIndex = index;
    notify(onCurrentIndexChanged);
    notify(onCurrentTextChanged);
    notify(onHistoryChanged);
}

std::string NoteStore::currentText() const
{
    if (m_currentIndex >= 0 && m_currentIndex < (int)m_notes.size())
        return m_notes[m_currentIndex];
    return {};
}

void NoteStore::setCurrentText(const std::string &text)
{
    if (m_currentIndex < 0 || m_currentIndex >= (int)m_notes.size())
        return;
    if (m_notes[m_currentIndex] == text)
        return;
    recordPreChange(m_notes[m_currentIndex]);
    m_notes[m_currentIndex] = text;
    notify(onCurrentTextChanged);
    notify(onHistoryChanged);
    saveSingle(m_currentIndex);
}

void NoteStore::recordPreChange(const std::string &oldText)
{
    NoteId id = currentNoteId();
    if (id == 0) return;
    NoteHistory &h = m_history[id];
    // Any fresh edit invalidates redo.
    h.redo.clear();
    std::int64_t t = nowMs();
    std::int64_t elapsed = t - h.lastPushMs;
    int curSize = (int)oldText.size();
    int deltaFromLast = std::abs(curSize - h.lastPushSize);

    if (h.pending && elapsed < kCoalesceMs && deltaFromLast < kCoalesceSizeDelta) {
        // Coalesce: leave existing pending snapshot in place.
        return;
    }

    if ((int)h.undo.size() >= kHistoryCap)
        h.undo.erase(h.undo.begin());
    h.undo.push_back(oldText);
    h.pending = true;
    h.lastPushMs = t;
    h.lastPushSize = curSize;
}

void NoteStore::commitHistory()
{
    NoteId id = currentNoteId();
    if (id == 0) return;
    auto it = m_history.find(id);
    if (it == m_history.end()) return;
    it->second.pending = false;
}

bool NoteStore::canUndo() const
{
    NoteId id = currentNoteId();
    auto it = m_history.find(id);
    return it != m_history.end() && !it->second.undo.empty();
}

bool NoteStore::canRedo() const
{
    NoteId id = currentNoteId();
    auto it = m_history.find(id);
    return it != m_history.end() && !it->second.redo.empty();
}

bool NoteStore::undo()
{
    if (m_currentIndex < 0 || m_currentIndex >= (int)m_notes.size())
        return false;
    NoteId id = currentNoteId();
    auto it = m_history.find(id);
    if (it == m_history.end() || it->second.undo.empty())
        return false;
    NoteHistory &h = it->second;
    std::string prev = std::move(h.undo.back());
    h.undo.pop_back();
    if ((int)h.redo.size() >= kHistoryCap)
        h.redo.erase(h.redo.begin());
    h.redo.push_back(m_notes[m_currentIndex]);
    m_notes[m_currentIndex] = std::move(prev);
    h.pending = false;
    h.lastPushMs = 0;
    h.lastPushSize = (int)m_notes[m_currentIndex].size();
    saveSingle(m_currentIndex);
    notify(onCurrentTextChanged);
    notify(onHistoryChanged);
    return true;
}

bool NoteStore::redo()
{
    if (m_currentIndex < 0 || m_currentIndex >= (int)m_notes.size())
        return false;
    NoteId id = currentNoteId();
    auto it = m_history.find(id);
    if (it == m_history.end() || it->second.redo.empty())
        return false;
    NoteHistory &h = it->second;
    std::string next = std::move(h.redo.back());
    h.redo.pop_back();
    if ((int)h.undo.size() >= kHistoryCap)
        h.undo.erase(h.undo.begin());
    h.undo.push_back(m_notes[m_currentIndex]);
    m_notes[m_currentIndex] = std::move(next);
    h.pending = false;
    h.lastPushMs = 0;
    h.lastPushSize = (int)m_notes[m_currentIndex].size();
    saveSingle(m_currentIndex);
    notify(onCurrentTextChanged);
    notify(onHistoryChanged);
    return true;
}

std::string NoteStore::getText(int index) const
{
    if (index >= 0 && index < (int)m_notes.size())
        return m_notes[index];
    return {};
}

void NoteStore::createNote()
{
    m_notes.insert(m_notes.begin(), std::string());
    m_noteIds.insert(m_noteIds.begin(), nextId());
    m_currentIndex = 0;
    saveAll();
    assertInvariants();
    notify(onNoteCountChanged);
    notify(onCurrentIndexChanged);
    notify(onCurrentTextChanged);
}

void NoteStore::deleteNote(int index)
{
    if (index < 0 || index >= (int)m_notes.size())
        return;
    if (m_notes.size() == 1) {
        m_notes[0].clear();
        // keep m_noteIds[0] — same logical slot, just empty
        notify(onCurrentTextChanged);
        saveAll();
        assertInvariants();
        return;
    }
    m_history.erase(m_noteIds[index]);
    m_notes.erase(m_notes.begin() + index);
    m_noteIds.erase(m_noteIds.begin() + index);

    // Remove the old last file (since we renumber)
    auto dir = storagePath();
    char filename[16];
    snprintf(filename, sizeof(filename), "%03d.txt", (int)m_notes.size() + 1);
    fs::remove(fs::path(dir) / filename);

    if (m_currentIndex >= (int)m_notes.size())
        m_currentIndex = (int)m_notes.size() - 1;
    saveAll();
    assertInvariants();
    notify(onNoteCountChanged);
    notify(onCurrentIndexChanged);
    notify(onCurrentTextChanged);
}

void NoteStore::appendText(const std::string &text)
{
    if (m_currentIndex < 0 || m_currentIndex >= (int)m_notes.size())
        return;
    recordPreChange(m_notes[m_currentIndex]);
    commitHistory();  // append is a discrete op — do not coalesce with typing
    if (m_notes[m_currentIndex].empty())
        m_notes[m_currentIndex] = text;
    else
        m_notes[m_currentIndex] += "\n" + text;
    notify(onCurrentTextChanged);
    notify(onHistoryChanged);
    saveSingle(m_currentIndex);
}

void NoteStore::deleteAllNotes()
{
    auto dir = storagePath();
    for (auto &entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() == ".txt")
            fs::remove(entry.path());
    }
    m_history.clear();
    m_notes.clear();
    m_noteIds.clear();
    m_notes.emplace_back();
    m_noteIds.push_back(nextId());
    m_currentIndex = 0;
    assertInvariants();
    notify(onNoteCountChanged);
    notify(onCurrentIndexChanged);
    notify(onCurrentTextChanged);
    notify(onHistoryChanged);
}

void NoteStore::reload()
{
    m_history.clear();
    m_notes.clear();
    m_noteIds.clear();
    m_currentIndex = 0;
    loadNotes();
    if (m_notes.empty()) {
        m_notes.emplace_back();
        m_noteIds.push_back(nextId());
    }
    assertInvariants();
    notify(onNoteCountChanged);
    notify(onCurrentIndexChanged);
    notify(onCurrentTextChanged);
    notify(onHistoryChanged);
}

void NoteStore::loadNotes()
{
    auto dir = storagePath();
    fs::create_directories(dir);

    std::vector<fs::path> files;
    for (auto &entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt"
            && entry.path().filename().string() != "repo") // skip repo dir
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    for (auto &path : files) {
        std::ifstream ifs(path);
        if (ifs) {
            std::ostringstream ss;
            ss << ifs.rdbuf();
            m_notes.push_back(ss.str());
            m_noteIds.push_back(nextId());
        }
    }
}

void NoteStore::saveAll()
{
    auto dir = storagePath();
    fs::create_directories(dir);
    for (int i = 0; i < (int)m_notes.size(); ++i)
        saveSingle(i);
}

void NoteStore::saveSingle(int index)
{
    auto dir = storagePath();
    fs::create_directories(dir);

    char filename[16];
    snprintf(filename, sizeof(filename), "%03d.txt", index + 1);
    std::ofstream ofs(fs::path(dir) / filename);
    if (ofs)
        ofs << m_notes[index];
}

std::string NoteStore::storagePath() const
{
#ifdef __APPLE__
    // ~/Library/Application Support/flick
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    return std::string(home) + "/Library/Application Support/flick";
#else
    // XDG: ~/.local/share/flick
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg && xdg[0])
        return std::string(xdg) + "/flick";
    const char *home = getenv("HOME");
    if (home)
        return std::string(home) + "/.local/share/flick";
    return "/tmp/flick";
#endif
}

} // namespace flick
