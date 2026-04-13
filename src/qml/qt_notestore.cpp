#include "qt_notestore.h"
#include <QVariantMap>

QtNoteStore::QtNoteStore(QObject *parent)
    : QObject(parent)
{
    m_core.onNoteCountChanged = [this]() { emit noteCountChanged(); };
    m_core.onCurrentIndexChanged = [this]() { emit currentIndexChanged(); };
    m_core.onCurrentTextChanged = [this]() { emit currentTextChanged(); };
    m_core.onHistoryChanged = [this]() { emit historyChanged(); };
    m_core.onTrashChanged = [this]() { emit trashChanged(); };
}

QVariantList QtNoteStore::trashEntries() {
    QVariantList out;
    for (auto &e : m_core.listTrash()) {
        QVariantMap m;
        m["id"] = QString::fromStdString(e.id);
        m["preview"] = QString::fromStdString(e.preview);
        m["deletedAt"] = (qlonglong)e.deletedAt;
        out.push_back(m);
    }
    return out;
}

bool QtNoteStore::restoreFromTrash(const QString &id) {
    return m_core.restoreFromTrash(id.toStdString());
}

bool QtNoteStore::purgeFromTrash(const QString &id) {
    return m_core.purgeFromTrash(id.toStdString());
}

void QtNoteStore::emptyTrash() { m_core.emptyTrash(); }

int QtNoteStore::trashCount() { return (int)m_core.listTrash().size(); }

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
