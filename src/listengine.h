#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class ListEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY itemsChanged)
    Q_PROPERTY(QString title READ title NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)

public:
    explicit ListEngine(QObject *parent = nullptr);

    bool active() const;
    QString title() const;
    QVariantList items() const;

    Q_INVOKABLE void evaluate(const QString &text);
    Q_INVOKABLE QString toggleCheck(const QString &text, int lineIndex);

signals:
    void itemsChanged();

private:
    bool m_active = false;
    QString m_title;
    QVariantList m_items;
};
