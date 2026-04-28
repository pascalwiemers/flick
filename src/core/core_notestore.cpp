#include "core_notestore.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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
        if (!m_notes[0].empty()) {
            moveToTrash(m_notes[0]);
            notify(onTrashChanged);
        }
        m_notes[0].clear();
        // keep m_noteIds[0] — same logical slot, just empty
        notify(onCurrentTextChanged);
        saveAll();
        assertInvariants();
        return;
    }
    if (!m_notes[index].empty()) {
        moveToTrash(m_notes[index]);
        notify(onTrashChanged);
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
    bool trashed = false;
    for (auto &note : m_notes) {
        if (!note.empty()) {
            moveToTrash(note);
            trashed = true;
        }
    }
    auto dir = storagePath();
    for (auto &entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() == ".txt" && entry.is_regular_file())
            fs::remove(entry.path());
    }
    if (trashed)
        notify(onTrashChanged);
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

std::string NoteStore::trashPath() const
{
    return storagePath() + "/trash";
}

std::string NoteStore::makeTrashId(const std::string &content) const
{
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;

    char ts[32];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d-%02d-%02d-%03lld",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             (long long)ms);

    std::string slug;
    for (char c : content) {
        if (c == '\n' || c == '\r') break;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '_' || c == '-')
            slug += c;
        else if (c == ' ')
            slug += '_';
        if ((int)slug.size() >= 40) break;
    }
    if (slug.empty()) slug = "note";

    return std::string(ts) + "_" + slug + ".txt";
}

void NoteStore::moveToTrash(const std::string &content)
{
    auto dir = trashPath();
    fs::create_directories(dir);
    std::string id = makeTrashId(content);
    // Guard against collisions if multiple notes are trashed within the same ms.
    fs::path path = fs::path(dir) / id;
    int suffix = 1;
    while (fs::exists(path)) {
        id = makeTrashId(content);
        size_t dot = id.rfind('.');
        if (dot != std::string::npos)
            id.insert(dot, "_" + std::to_string(suffix));
        else
            id += "_" + std::to_string(suffix);
        path = fs::path(dir) / id;
        ++suffix;
        if (suffix > 1000) return; // give up, something is wrong
    }
    std::ofstream ofs(path);
    if (ofs) ofs << content;
}

static std::int64_t parseTrashTimestamp(const std::string &filename)
{
    // Filenames start with "YYYY-MM-DDTHH-MM-SS-mmm_..."
    if (filename.size() < 19) return 0;
    std::tm tm{};
    int ms = 0;
    if (sscanf(filename.c_str(), "%4d-%2d-%2dT%2d-%2d-%2d-%3d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &ms) != 7)
        return 0;
    tm.tm_year -= 1900;
    tm.tm_mon  -= 1;
#ifdef _WIN32
    std::time_t t = _mkgmtime(&tm);
#else
    std::time_t t = timegm(&tm);
#endif
    return (std::int64_t)t;
}

static std::string trashPreview(const std::string &content, size_t cap = 280)
{
    std::string out;
    out.reserve(std::min(content.size(), cap));
    for (char c : content) {
        if (c == '\r') continue;
        out += c;
        if (out.size() >= cap) break;
    }
    return out;
}

void NoteStore::pruneTrash()
{
    auto dir = trashPath();
    if (!fs::exists(dir)) return;

    struct Entry { fs::path path; std::int64_t ts; };
    std::vector<Entry> entries;
    for (auto &e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file() || e.path().extension() != ".txt") continue;
        entries.push_back({e.path(), parseTrashTimestamp(e.path().filename().string())});
    }

    // Age-based: drop > 30 days old.
    using namespace std::chrono;
    std::int64_t now = duration_cast<seconds>(
            system_clock::now().time_since_epoch()).count();
    constexpr std::int64_t kMaxAgeSec = 30LL * 24 * 3600;
    for (auto it = entries.begin(); it != entries.end(); ) {
        if (it->ts > 0 && (now - it->ts) > kMaxAgeSec) {
            fs::remove(it->path);
            it = entries.erase(it);
        } else {
            ++it;
        }
    }

    // Count-based: cap at 100, drop oldest.
    constexpr size_t kMaxCount = 100;
    if (entries.size() > kMaxCount) {
        std::sort(entries.begin(), entries.end(),
                  [](const Entry &a, const Entry &b) { return a.ts < b.ts; });
        size_t toRemove = entries.size() - kMaxCount;
        for (size_t i = 0; i < toRemove; ++i)
            fs::remove(entries[i].path);
    }
}

std::vector<NoteStore::TrashEntry> NoteStore::listTrash()
{
    pruneTrash();
    std::vector<TrashEntry> result;
    auto dir = trashPath();
    if (!fs::exists(dir)) return result;

    for (auto &e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file() || e.path().extension() != ".txt") continue;
        TrashEntry te;
        te.id = e.path().filename().string();
        te.deletedAt = parseTrashTimestamp(te.id);
        std::ifstream ifs(e.path());
        if (ifs) {
            std::ostringstream ss;
            ss << ifs.rdbuf();
            te.preview = trashPreview(ss.str());
        }
        result.push_back(std::move(te));
    }

    // Newest first
    std::sort(result.begin(), result.end(),
              [](const TrashEntry &a, const TrashEntry &b) { return a.deletedAt > b.deletedAt; });
    return result;
}

bool NoteStore::restoreFromTrash(const std::string &id)
{
    auto path = fs::path(trashPath()) / id;
    if (!fs::exists(path)) return false;
    std::ifstream ifs(path);
    if (!ifs) return false;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();

    m_notes.insert(m_notes.begin(), std::move(content));
    m_noteIds.insert(m_noteIds.begin(), nextId());
    m_currentIndex = 0;
    fs::remove(path);
    saveAll();
    assertInvariants();
    notify(onNoteCountChanged);
    notify(onCurrentIndexChanged);
    notify(onCurrentTextChanged);
    notify(onHistoryChanged);
    notify(onTrashChanged);
    return true;
}

bool NoteStore::purgeFromTrash(const std::string &id)
{
    auto path = fs::path(trashPath()) / id;
    if (!fs::exists(path)) return false;
    fs::remove(path);
    notify(onTrashChanged);
    return true;
}

void NoteStore::emptyTrash()
{
    auto dir = trashPath();
    if (!fs::exists(dir)) return;
    for (auto &e : fs::directory_iterator(dir)) {
        if (e.is_regular_file() && e.path().extension() == ".txt")
            fs::remove(e.path());
    }
    notify(onTrashChanged);
}

} // namespace flick
