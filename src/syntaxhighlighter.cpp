#include "syntaxhighlighter.h"
#include <QRegularExpression>

SyntaxHighlighter::SyntaxHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
    m_varNameFormat.setForeground(QColor("#6b8aad"));
    m_colonFormat.setForeground(QColor("#555555"));
}

QQuickTextDocument *SyntaxHighlighter::document() const
{
    return m_document;
}

void SyntaxHighlighter::setDocument(QQuickTextDocument *doc)
{
    m_document = doc;
    if (doc)
        QSyntaxHighlighter::setDocument(doc->textDocument());
    else
        QSyntaxHighlighter::setDocument(nullptr);
}

bool SyntaxHighlighter::mathMode() const
{
    return m_mathMode;
}

void SyntaxHighlighter::setMathMode(bool enabled)
{
    if (m_mathMode == enabled)
        return;
    m_mathMode = enabled;
    rehighlight();
}

QStringList SyntaxHighlighter::variableNames() const
{
    return m_variableNames;
}

void SyntaxHighlighter::setVariableNames(const QStringList &names)
{
    if (m_variableNames == names)
        return;
    m_variableNames = names;
    rehighlight();
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    if (!m_mathMode)
        return;

    QString trimmed = text.trimmed();

    if (trimmed.isEmpty() || trimmed.startsWith("//"))
        return;

    // Highlight variable assignment definition: "name : expr"
    int colonPos = text.indexOf(':');
    if (colonPos > 0) {
        QString before = text.left(colonPos).trimmed();
        static QRegularExpression validName("^[a-zA-Z][a-zA-Z0-9_ ]*$");
        if (validName.match(before).hasMatch() && before != "math") {
            // Highlight the variable name
            setFormat(0, colonPos, m_varNameFormat);
            // Highlight the colon
            setFormat(colonPos, 1, m_colonFormat);
        }
    }

    // Highlight variable references anywhere in the line
    QString lowerText = text.toLower();
    for (const QString &var : m_variableNames) {
        int searchFrom = 0;
        // For assignment lines, search only after the colon
        if (colonPos > 0) {
            QString before = text.left(colonPos).trimmed().toLower();
            if (before == var)
                searchFrom = colonPos + 1;
        }

        while (searchFrom < lowerText.length()) {
            int idx = lowerText.indexOf(var, searchFrom);
            if (idx < 0)
                break;

            // Check word boundaries — don't highlight partial matches
            bool leftOk = (idx == 0) || !lowerText[idx - 1].isLetterOrNumber();
            int end = idx + var.length();
            bool rightOk = (end >= lowerText.length()) || !lowerText[end].isLetterOrNumber();

            if (leftOk && rightOk) {
                setFormat(idx, var.length(), m_varNameFormat);
            }

            searchFrom = idx + var.length();
        }
    }
}
