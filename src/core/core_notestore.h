#pragma once

#include <string>
#include <vector>
#include <functional>

namespace flick {

class NoteStore {
public:
    NoteStore();

    int noteCount() const;
    int currentIndex() const;
    void setCurrentIndex(int index);
    std::string currentText() const;
    void setCurrentText(const std::string &text);

    std::string getText(int index) const;
    void createNote();
    void deleteNote(int index);
    void deleteAllNotes();
    void appendText(const std::string &text);
    void reload();

    // Callbacks for UI notification
    std::function<void()> onNoteCountChanged;
    std::function<void()> onCurrentIndexChanged;
    std::function<void()> onCurrentTextChanged;

private:
    void loadNotes();
    void saveAll();
    void saveSingle(int index);
    std::string storagePath() const;

    std::vector<std::string> m_notes;
    int m_currentIndex = 0;
};

} // namespace flick
