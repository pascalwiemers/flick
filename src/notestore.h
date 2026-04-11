#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class NoteStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(int noteCount READ noteCount NOTIFY noteCountChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentText READ currentText WRITE setCurrentText NOTIFY currentTextChanged)

public:
    explicit NoteStore(QObject *parent = nullptr);

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

signals:
    void noteCountChanged();
    void currentIndexChanged();
    void currentTextChanged();

private:
    void loadNotes();
    void saveAll();
    void saveSingle(int index);
    QString storagePath() const;

    QVector<QString> m_notes;
    int m_currentIndex = 0;
};
