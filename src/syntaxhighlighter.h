#pragma once

#include <QSyntaxHighlighter>
#include <QQuickTextDocument>
#include <QTextCharFormat>
#include <QStringList>

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument* document READ document WRITE setDocument)
    Q_PROPERTY(bool mathMode READ mathMode WRITE setMathMode)
    Q_PROPERTY(QStringList variableNames READ variableNames WRITE setVariableNames)

public:
    explicit SyntaxHighlighter(QObject *parent = nullptr);

    QQuickTextDocument *document() const;
    void setDocument(QQuickTextDocument *doc);

    bool mathMode() const;
    void setMathMode(bool enabled);

    QStringList variableNames() const;
    void setVariableNames(const QStringList &names);

protected:
    void highlightBlock(const QString &text) override;

private:
    QQuickTextDocument *m_document = nullptr;
    bool m_mathMode = false;
    QStringList m_variableNames;
    QTextCharFormat m_varNameFormat;
    QTextCharFormat m_colonFormat;
};
