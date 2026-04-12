#include "qt_mathengine.h"
#include "qt_rateprovider.h"
#include <QVariantMap>

QtMathEngine::~QtMathEngine() = default;

QtMathEngine::QtMathEngine(QObject *parent)
    : QObject(parent)
    , m_rateProvider(std::make_unique<QtRateProvider>(this))
{
    m_core.setRateProvider(m_rateProvider.get());
    m_core.onResultsChanged = [this]() {
        m_cachedResults.clear();
        for (auto &r : m_core.results()) {
            QVariantMap m;
            m["line"] = r.line;
            m["text"] = QString::fromStdString(r.text);
            m["color"] = QString::fromStdString(r.color);
            m["isComment"] = r.isComment;
            m["isSeparator"] = r.isSeparator;
            m["isTotal"] = r.isTotal;
            m_cachedResults.append(m);
        }
        emit resultsChanged();
    };
    m_core.onVariableNamesChanged = [this]() {
        m_cachedVarNames.clear();
        for (auto &n : m_core.variableNames())
            m_cachedVarNames.append(QString::fromStdString(n));
        emit variableNamesChanged();
    };
}

QVariantList QtMathEngine::results() const { return m_cachedResults; }
QStringList QtMathEngine::variableNames() const { return m_cachedVarNames; }

void QtMathEngine::evaluate(const QString &text) {
    m_core.evaluate(text.toStdString());
}
