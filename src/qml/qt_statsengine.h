#pragma once

#include <QObject>
#include <QString>
#include "../core/core_statsengine.h"

class QtStatsEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY resultChanged)
    Q_PROPERTY(int items READ items NOTIFY resultChanged)
    Q_PROPERTY(int words READ words NOTIFY resultChanged)
    Q_PROPERTY(int characters READ characters NOTIFY resultChanged)
    Q_PROPERTY(int sentences READ sentences NOTIFY resultChanged)
    Q_PROPERTY(double fleschReadingEase READ fleschReadingEase NOTIFY resultChanged)
    Q_PROPERTY(double fleschKincaidGrade READ fleschKincaidGrade NOTIFY resultChanged)

public:
    explicit QtStatsEngine(QObject *parent = nullptr);

    bool active() const;
    int items() const;
    int words() const;
    int characters() const;
    int sentences() const;
    double fleschReadingEase() const;
    double fleschKincaidGrade() const;

    Q_INVOKABLE void evaluate(const QString &text);

signals:
    void resultChanged();

private:
    flick::StatsEngine m_core;
};
