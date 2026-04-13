#include "qt_notestore.h"

QtNoteStore::QtNoteStore(QObject *parent)
    : QObject(parent)
{
    m_core.onNoteCountChanged = [this]() { emit noteCountChanged(); };
    m_core.onCurrentIndexChanged = [this]() { emit currentIndexChanged(); };
    m_core.onCurrentTextChanged = [this]() { emit currentTextChanged(); };
    m_core.onHistoryChanged = [this]() { emit historyChanged(); };
}

bool QtNoteStore::canUndo() const { return m_core.canUndo(); }
bool QtNoteStore::canRedo() const { return m_core.canRedo(); }
bool QtNoteStore::undo() { return m_core.undo(); }
bool QtNoteStore::redo() { return m_core.redo(); }
void QtNoteStore::commitHistory() { m_core.commitHistory(); }

int QtNoteStore::noteCount() const { return m_core.noteCount(); }
int QtNoteStore::currentIndex() const { return m_core.currentIndex(); }
void QtNoteStore::setCurrentIndex(int index) { m_core.setCurrentIndex(index); }
QString QtNoteStore::currentText() const { return QString::fromStdString(m_core.currentText()); }
void QtNoteStore::setCurrentText(const QString &text) { m_core.setCurrentText(text.toStdString()); }
QString QtNoteStore::getText(int index) const { return QString::fromStdString(m_core.getText(index)); }
void QtNoteStore::createNote() { m_core.createNote(); }
void QtNoteStore::deleteNote(int index) { m_core.deleteNote(index); }
void QtNoteStore::deleteAllNotes() { m_core.deleteAllNotes(); }
void QtNoteStore::appendText(const QString &text) { m_core.appendText(text.toStdString()); }
void QtNoteStore::reload() { m_core.reload(); }
