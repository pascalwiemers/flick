#pragma once

#include <QObject>
#include <QString>
#include "../core/core_notestore.h"

class QtNoteStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(int noteCount READ noteCount NOTIFY noteCountChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentText READ currentText WRITE setCurrentText NOTIFY currentTextChanged)

public:
    explicit QtNoteStore(QObject *parent = nullptr);

    int noteCount() const;
    int currentIndex() const;
    void setCurrentIndex(int index);
    QString currentText() const;
    void setCurrentText(const QString &text);

    Q_INVOKABLE QString getText(int index) const;
    Q_INVOKABLE void createNote();
    Q_INVOKABLE void deleteNote(int index);
    Q_INVOKABLE void deleteAllNotes();
    Q_INVOKABLE void appendText(const QString &text);
    Q_INVOKABLE void reload();

    flick::NoteStore &core() { return m_core; }

signals:
    void noteCountChanged();
    void currentIndexChanged();
    void currentTextChanged();

private:
    flick::NoteStore m_core;
};
