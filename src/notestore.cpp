#include "notestore.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include <algorithm>

NoteStore::NoteStore(QObject *parent)
    : QObject(parent)
{
    loadNotes();
    if (m_notes.isEmpty()) {
        m_notes.append(QString());
    }
}

int NoteStore::noteCount() const
{
    return m_notes.size();
}

int NoteStore::currentIndex() const
{
    return m_currentIndex;
}

void NoteStore::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_notes.size() || index == m_currentIndex)
        return;
    m_currentIndex = index;
    emit currentIndexChanged();
    emit currentTextChanged();
}

QString NoteStore::currentText() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_notes.size())
        return m_notes[m_currentIndex];
    return QString();
}

void NoteStore::setCurrentText(const QString &text)
{
    if (m_currentIndex < 0 || m_currentIndex >= m_notes.size())
        return;
    if (m_notes[m_currentIndex] == text)
        return;
    m_notes[m_currentIndex] = text;
    emit currentTextChanged();
    saveSingle(m_currentIndex);
}

QString NoteStore::getText(int index) const
{
    if (index >= 0 && index < m_notes.size())
        return m_notes[index];
    return QString();
}

void NoteStore::createNote()
{
    m_notes.prepend(QString());
    m_currentIndex = 0;
    saveAll();
    emit noteCountChanged();
    emit currentIndexChanged();
    emit currentTextChanged();
}

void NoteStore::deleteNote(int index)
{
    if (index < 0 || index >= m_notes.size())
        return;
    // Don't delete the last remaining note — just clear it
    if (m_notes.size() == 1) {
        m_notes[0] = QString();
        emit currentTextChanged();
        saveAll();
        return;
    }
    m_notes.removeAt(index);
    // Remove the old last file (since we renumber)
    QDir dir(storagePath());
    QString oldFile = QString("%1.txt").arg(m_notes.size() + 1, 3, 10, QChar('0'));
    dir.remove(oldFile);

    if (m_currentIndex >= m_notes.size())
        m_currentIndex = m_notes.size() - 1;
    saveAll();
    emit noteCountChanged();
    emit currentIndexChanged();
    emit currentTextChanged();
}

void NoteStore::appendText(const QString &text)
{
    if (m_currentIndex < 0 || m_currentIndex >= m_notes.size())
        return;
    if (m_notes[m_currentIndex].isEmpty())
        m_notes[m_currentIndex] = text;
    else
        m_notes[m_currentIndex] += "\n" + text;
    emit currentTextChanged();
    saveSingle(m_currentIndex);
}

void NoteStore::deleteAllNotes()
{
    QDir dir(storagePath());
    for (const auto &entry : dir.entryList({"*.txt"}, QDir::Files))
        dir.remove(entry);
    m_notes.clear();
    m_notes.append(QString());
    m_currentIndex = 0;
    emit noteCountChanged();
    emit currentIndexChanged();
    emit currentTextChanged();
}

void NoteStore::loadNotes()
{
    QDir dir(storagePath());
    if (!dir.exists())
        dir.mkpath(".");

    QStringList files = dir.entryList({"*.txt"}, QDir::Files, QDir::Name);
    for (const auto &filename : files) {
        QFile file(dir.filePath(filename));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            m_notes.append(in.readAll());
        }
    }
}

void NoteStore::saveAll()
{
    QDir dir(storagePath());
    if (!dir.exists())
        dir.mkpath(".");

    for (int i = 0; i < m_notes.size(); ++i)
        saveSingle(i);
}

void NoteStore::saveSingle(int index)
{
    QDir dir(storagePath());
    if (!dir.exists())
        dir.mkpath(".");

    QString filename = QString("%1.txt").arg(index + 1, 3, 10, QChar('0'));
    QFile file(dir.filePath(filename));
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << m_notes[index];
    }
}

QString NoteStore::storagePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/flick";
}
