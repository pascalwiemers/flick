#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace flick {

class NoteStore {
public:
    using NoteId = std::uint64_t;

    struct TrashEntry {
        std::string id;            // filename under trash/
        std::string preview;       // first line, truncated
        std::int64_t deletedAt;    // unix seconds
    };

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

    NoteId currentNoteId() const;
    NoteId noteIdAt(int index) const;
    int indexOfNoteId(NoteId id) const;

    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();
    void commitHistory();

    std::vector<TrashEntry> listTrash();
    bool restoreFromTrash(const std::string &id);
    bool purgeFromTrash(const std::string &id);
    void emptyTrash();

    // Callbacks for UI notification
    std::function<void()> onNoteCountChanged;
    std::function<void()> onCurrentIndexChanged;
    std::function<void()> onCurrentTextChanged;
    std::function<void()> onHistoryChanged;
    std::function<void()> onTrashChanged;

private:
    void loadNotes();
    void saveAll();
    void saveSingle(int index);
    std::string storagePath() const;
    std::string trashPath() const;
    std::string makeTrashId(const std::string &content) const;
    void moveToTrash(const std::string &content);
    void pruneTrash();
    NoteId nextId();
    void assertInvariants() const;

    struct NoteHistory {
        std::vector<std::string> undo;   // oldest → newest, latest = pending pre-change
        std::vector<std::string> redo;
        std::int64_t lastPushMs = 0;
        int lastPushSize = 0;            // m_notes[current] size at last push
        bool pending = false;            // top of undo is a coalescable pending entry
    };

    void recordPreChange(const std::string &oldText);

    std::vector<std::string> m_notes;
    std::vector<NoteId> m_noteIds;
    std::unordered_map<NoteId, NoteHistory> m_history;
    NoteId m_nextId = 1;
    int m_currentIndex = 0;
};

} // namespace flick
