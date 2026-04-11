#pragma once

#include <QObject>
#include <QElapsedTimer>
#include "notestore.h"

class AutoPaste : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit AutoPaste(NoteStore *store, QObject *parent = nullptr);

    bool active() const;
    void setActive(bool on);

signals:
    void activeChanged();

private slots:
    void onClipboardChanged();

private:
    NoteStore *m_store;
    bool m_active = false;
    QString m_lastCaptured;
    QElapsedTimer m_debounceTimer;
};
