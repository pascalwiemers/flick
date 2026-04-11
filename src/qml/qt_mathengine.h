#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QStringList>
#include "../core/core_mathengine.h"

class QtMathEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QStringList variableNames READ variableNames NOTIFY variableNamesChanged)

public:
    explicit QtMathEngine(QObject *parent = nullptr);

    QVariantList results() const;
    QStringList variableNames() const;
    Q_INVOKABLE void evaluate(const QString &text);

signals:
    void resultsChanged();
    void variableNamesChanged();

private:
    flick::MathEngine m_core;
    QVariantList m_cachedResults;
    QStringList m_cachedVarNames;
};
