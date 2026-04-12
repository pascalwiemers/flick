#include "qt_statsengine.h"

QtStatsEngine::QtStatsEngine(QObject *parent)
    : QObject(parent)
{
    m_core.onResultChanged = [this]() {
        emit resultChanged();
    };
}

bool QtStatsEngine::active() const { return m_core.active(); }
int QtStatsEngine::items() const { return m_core.result().items; }
int QtStatsEngine::words() const { return m_core.result().words; }
int QtStatsEngine::characters() const { return m_core.result().characters; }
int QtStatsEngine::sentences() const { return m_core.result().sentences; }
double QtStatsEngine::fleschReadingEase() const { return m_core.result().fleschReadingEase; }
double QtStatsEngine::fleschKincaidGrade() const { return m_core.result().fleschKincaidGrade; }

void QtStatsEngine::evaluate(const QString &text) {
    m_core.evaluate(text.toStdString());
}
