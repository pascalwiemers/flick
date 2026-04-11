#include "core_notestore.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

#ifdef __APPLE__
#include <pwd.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace flick {

static void notify(const std::function<void()> &fn) {
    if (fn) fn();
}

NoteStore::NoteStore()
{
    loadNotes();
    if (m_notes.empty())
        m_notes.emplace_back();
}

int NoteStore::noteCount() const { return static_cast<int>(m_notes.size()); }
int NoteStore::currentIndex() const { return m_currentIndex; }

void NoteStore::setCurrentIndex(int index)
{
    if (index < 0 || index >= (int)m_notes.size() || index == m_currentIndex)
        return;
    m_currentIndex = index;
    notify(onCurrentIndexChanged);
    notify(onCurrentTextChanged);
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
    m_notes[m_currentIndex] = text;
    notify(onCurrentTextChanged);
    saveSingle(m_currentIndex);
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
    m_currentIndex = 0;
    saveAll();
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
        notify(onCurrentTextChanged);
        saveAll();
        return;
    }
    m_notes.erase(m_notes.begin() + index);

    // Remove the old last file (since we renumber)
    auto dir = storagePath();
    char filename[16];
    snprintf(filename, sizeof(filename), "%03d.txt", (int)m_notes.size() + 1);
    fs::remove(fs::path(dir) / filename);

    if (m_currentIndex >= (int)m_notes.size())
        m_currentIndex = (int)m_notes.size() - 1;
    saveAll();
    notify(onNoteCountChanged);
    notify(onCurrentIndexChanged);
    notify(onCurrentTextChanged);
}

void NoteStore::appendText(const std::string &text)
{
    if (m_currentIndex < 0 || m_currentIndex >= (int)m_notes.size())
        return;
    if (m_notes[m_currentIndex].empty())
        m_notes[m_currentIndex] = text;
    else
        m_notes[m_currentIndex] += "\n" + text;
    notify(onCurrentTextChanged);
    saveSingle(m_currentIndex);
}

void NoteStore::deleteAllNotes()
{
    auto dir = storagePath();
    for (auto &entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() == ".txt")
            fs::remove(entry.path());
    }
    m_notes.clear();
    m_notes.emplace_back();
    m_currentIndex = 0;
    notify(onNoteCountChanged);
    notify(onCurrentIndexChanged);
    notify(onCurrentTextChanged);
}

void NoteStore::reload()
{
    m_notes.clear();
    m_currentIndex = 0;
    loadNotes();
    if (m_notes.empty())
        m_notes.emplace_back();
    notify(onNoteCountChanged);
    notify(onCurrentIndexChanged);
    notify(onCurrentTextChanged);
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
