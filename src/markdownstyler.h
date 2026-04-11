#pragma once

#include <QObject>
#include <QQuickTextDocument>
#include <QVariantList>

class MarkdownStyler : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList regions READ regions NOTIFY regionsChanged)
public:
    explicit MarkdownStyler(QObject *parent = nullptr);

    Q_INVOKABLE void styleDocument(QQuickTextDocument *doc);
    QVariantList regions() const;

signals:
    void regionsChanged();

private:
    QVariantList m_regions;
};
