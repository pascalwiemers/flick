#pragma once

#include <QObject>
#include <QQuickWindow>

class WindowDragger : public QObject {
    Q_OBJECT
public:
    explicit WindowDragger(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void startDrag(QQuickWindow *window);
};
