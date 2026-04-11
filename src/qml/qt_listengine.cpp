#include "qt_listengine.h"
#include <QVariantMap>

QtListEngine::QtListEngine(QObject *parent)
    : QObject(parent)
{
    m_core.onItemsChanged = [this]() {
        m_cachedItems.clear();
        for (auto &item : m_core.items()) {
            QVariantMap m;
            m["line"] = item.line;
            m["type"] = QString::fromStdString(item.type);
            m["level"] = item.level;
            m["checked"] = item.checked;
            m_cachedItems.append(m);
        }
        emit itemsChanged();
    };
}

bool QtListEngine::active() const { return m_core.active(); }
QString QtListEngine::title() const { return QString::fromStdString(m_core.title()); }
QVariantList QtListEngine::items() const { return m_cachedItems; }

void QtListEngine::evaluate(const QString &text) {
    m_core.evaluate(text.toStdString());
}

QString QtListEngine::toggleCheck(const QString &text, int lineIndex) {
    return QString::fromStdString(m_core.toggleCheck(text.toStdString(), lineIndex));
}
