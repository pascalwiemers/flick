#include "mathengine.h"
#include <QRegularExpression>
#include <QStringList>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <stack>

MathEngine::MathEngine(QObject *parent)
    : QObject(parent)
{
}

QVariantList MathEngine::results() const
{
    return m_results;
}

// Check if a string is purely non-numeric text (not a valid expression)
static bool isPureText(const QString &s)
{
    // If it contains at least one digit or operator pattern, it might be math
    bool hasDigit = false;
    for (const auto &ch : s) {
        if (ch.isDigit()) { hasDigit = true; break; }
    }
    return !hasDigit;
}

void MathEngine::evaluate(const QString &text)
{
    QVariantList newResults;
    QMap<QString, VarInfo> vars;
    QStringList lines = text.split('\n');

    // Track math blocks for running totals
    struct BlockEntry {
        int line;
        double value;
        bool hasCurrency;
    };
    QVector<BlockEntry> currentBlock;

    auto flushBlock = [&](int afterLine) {
        if (currentBlock.size() >= 2) {
            double total = 0;
            bool blockCurrency = false;
            for (const auto &e : currentBlock) {
                total += e.value;
                if (e.hasCurrency) blockCurrency = true;
            }
            int lastLine = currentBlock.last().line;
            QVariantMap sep;
            sep["line"] = lastLine;
            sep["text"] = QString();
            sep["color"] = "#333333";
            sep["isComment"] = false;
            sep["isSeparator"] = true;
            sep["isTotal"] = false;
            newResults.append(sep);

            QVariantMap tot;
            tot["line"] = lastLine;
            tot["text"] = formatResult(total, blockCurrency);
            tot["color"] = total < 0 ? "#cc6666" : "#5daa5d";
            tot["isComment"] = false;
            tot["isSeparator"] = false;
            tot["isTotal"] = true;
            newResults.append(tot);
        }
        currentBlock.clear();
    };

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();

        // Blank line — breaks math block
        if (line.isEmpty()) {
            flushBlock(i);
            continue;
        }

        // Comment line — does NOT break math block
        if (line.startsWith("//")) {
            QVariantMap entry;
            entry["line"] = i;
            entry["text"] = QString();
            entry["color"] = "#555555";
            entry["isComment"] = true;
            entry["isSeparator"] = false;
            entry["isTotal"] = false;
            newResults.append(entry);
            continue;
        }

        // Check for variable assignment (name : expr)
        int colonPos = line.indexOf(':');
        bool isAssignment = false;
        QString varName;
        QString exprStr;

        if (colonPos > 0) {
            varName = line.left(colonPos).trimmed().toLower();
            exprStr = line.mid(colonPos + 1).trimmed();

            // Check if RHS could be math: has digits, or references known variables,
            // or contains math operators with operands
            if (!isPureText(exprStr)) {
                isAssignment = true;
            } else {
                // Check if RHS contains any known variable names
                QString lowerExpr = exprStr.toLower();
                for (auto it = vars.begin(); it != vars.end(); ++it) {
                    if (lowerExpr.contains(it.key())) {
                        isAssignment = true;
                        break;
                    }
                }
            }
        }

        if (!isAssignment) {
            // Try as bare expression — but only show result if line ends with "="
            exprStr = line;
        }

        // Check for currency in the raw expression
        bool lineCurrency = exprStr.contains('$');
        // Strip $ for evaluation
        QString cleanExpr = exprStr;
        cleanExpr.remove('$');

        // For assignments: always evaluate (to store the variable), never show result
        // For expressions: only evaluate and show if the line ends with "="
        bool wantsResult = false;
        if (!isAssignment) {
            QString stripped = cleanExpr.trimmed();
            if (stripped.endsWith('=')) {
                wantsResult = true;
                cleanExpr = stripped.left(stripped.length() - 1).trimmed();
            }
        }

        // Substitute variables
        bool varCurrency = false;
        bool hasUndefined = false;
        QString substituted = substituteVars(cleanExpr, vars, varCurrency, hasUndefined);

        bool hasCurrency = lineCurrency || varCurrency;

        if (hasUndefined && (isAssignment || wantsResult)) {
            if (wantsResult) {
                QVariantMap entry;
                entry["line"] = i;
                entry["text"] = " ?";
                entry["color"] = "#cc4444";
                entry["isComment"] = false;
                entry["isSeparator"] = false;
                entry["isTotal"] = false;
                newResults.append(entry);
            }
            flushBlock(i);
            continue;
        }

        // Parse and evaluate
        EvalResult result = parseAndEval(substituted);

        // Fallback: if parsing failed on a "wants result" line, try trailing number
        if (result.status == EvalStatus::Error && wantsResult) {
            static QRegularExpression trailingNum("(-?\\$?[\\d,]+\\.?\\d*)\\s*$");
            QRegularExpressionMatch match = trailingNum.match(cleanExpr);
            if (match.hasMatch()) {
                QString numStr = match.captured(1);
                numStr.remove('$').remove(',');
                bool numOk;
                double val = numStr.toDouble(&numOk);
                if (numOk)
                    result = { val, EvalStatus::Ok, false };
            }
        }

        if (result.status == EvalStatus::Ok) {
            if (isAssignment) {
                vars[varName] = { result.value, hasCurrency };
                currentBlock.append({ i, result.value, hasCurrency });
                continue;
            }

            if (wantsResult) {
                QVariantMap entry;
                entry["line"] = i;
                entry["text"] = " " + formatResult(result.value, hasCurrency);
                entry["color"] = result.value < 0 ? "#cc6666" : "#5daa5d";
                entry["isComment"] = false;
                entry["isSeparator"] = false;
                entry["isTotal"] = false;
                newResults.append(entry);
            }

            currentBlock.append({ i, result.value, hasCurrency });
        } else if (result.status == EvalStatus::DivByZero && wantsResult) {
            QVariantMap entry;
            entry["line"] = i;
            entry["text"] = QString::fromUtf8(" ∞");
            entry["color"] = "#cc6666";
            entry["isComment"] = false;
            entry["isSeparator"] = false;
            entry["isTotal"] = false;
            newResults.append(entry);
            flushBlock(i);
        } else if (!isAssignment) {
            flushBlock(i);
        }
    }

    // Flush any remaining block at end of text
    flushBlock(lines.size());

    if (m_results != newResults) {
        m_results = newResults;
        emit resultsChanged();
    }
}

QString MathEngine::substituteVars(const QString &expr, const QMap<QString, VarInfo> &vars,
                                    bool &hasCurrency, bool &hasUndefined)
{
    if (vars.isEmpty())
        return expr;

    // Sort variable names longest-first for greedy matching
    QVector<QString> names;
    names.reserve(vars.size());
    for (auto it = vars.begin(); it != vars.end(); ++it)
        names.append(it.key());
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        return a.length() > b.length();
    });

    QString result = expr;
    bool anySubstituted = false;

    for (const auto &name : names) {
        // Case-insensitive search and replace
        QString lower = result.toLower();
        int searchFrom = 0;
        while (true) {
            int pos = lower.indexOf(name, searchFrom);
            if (pos < 0) break;

            // Check word boundaries: the character before and after should not be a letter
            bool boundaryBefore = (pos == 0) || !result[pos - 1].isLetter();
            bool boundaryAfter = (pos + name.length() >= result.length()) ||
                                 !result[pos + name.length()].isLetter();

            if (boundaryBefore && boundaryAfter) {
                const VarInfo &info = vars[name];
                if (info.hasCurrency) hasCurrency = true;

                QString replacement = QString::number(info.value, 'g', 15);
                // Wrap negative values in parens to keep parsing correct
                if (info.value < 0)
                    replacement = "(" + replacement + ")";

                result.replace(pos, name.length(), replacement);
                lower = result.toLower();
                searchFrom = pos + replacement.length();
                anySubstituted = true;
            } else {
                searchFrom = pos + 1;
            }
        }
    }

    // Only flag undefined if we tried to do substitutions (vars exist)
    // AND some letters remain after substitution — means a variable name
    // wasn't resolved. But if NOTHING was substituted, this is likely
    // just plain text, not a failed variable reference.
    if (anySubstituted) {
        bool hasLetters = false;
        for (const auto &ch : result) {
            if (ch.isLetter()) { hasLetters = true; break; }
        }
        if (hasLetters)
            hasUndefined = true;
    }

    return result;
}

QVector<MathEngine::Token> MathEngine::tokenize(const QString &expr, bool &ok)
{
    QVector<Token> tokens;
    ok = true;
    int i = 0;
    int len = expr.length();

    while (i < len) {
        QChar ch = expr[i];

        if (ch.isSpace()) {
            ++i;
            continue;
        }

        // Number
        if (ch.isDigit() || (ch == '.' && i + 1 < len && expr[i + 1].isDigit())) {
            int start = i;
            while (i < len && (expr[i].isDigit() || expr[i] == '.'))
                ++i;
            bool numOk;
            double val = expr.mid(start, i - start).toDouble(&numOk);
            if (!numOk) { ok = false; return {}; }
            tokens.append({ TokenType::Number, val, {} });
            continue;
        }

        // Operators
        if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
            // Detect unary minus/plus
            if ((ch == '-' || ch == '+') &&
                (tokens.isEmpty() || tokens.last().type == TokenType::Op ||
                 tokens.last().type == TokenType::LParen)) {
                // Unary: read the next number
                if (ch == '-') {
                    // Push 0 and then subtract, or handle in tokenizer
                    // Simpler: push a 0 and then the minus operator
                    tokens.append({ TokenType::Number, 0, {} });
                    tokens.append({ TokenType::Op, 0, ch });
                } else {
                    // Unary plus — just skip
                }
                ++i;
                continue;
            }
            tokens.append({ TokenType::Op, 0, ch });
            ++i;
            continue;
        }

        if (ch == '%') {
            tokens.append({ TokenType::Percent, 0, {} });
            ++i;
            continue;
        }

        if (ch == '(') {
            tokens.append({ TokenType::LParen, 0, {} });
            ++i;
            continue;
        }

        if (ch == ')') {
            tokens.append({ TokenType::RParen, 0, {} });
            ++i;
            continue;
        }

        // Unknown character — fail
        ok = false;
        return {};
    }

    return tokens;
}

static int precedence(QChar op)
{
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/' || op == '%') return 2;
    return 0;
}

MathEngine::EvalResult MathEngine::parseAndEval(const QString &expr)
{
    bool ok;
    QVector<Token> tokens = tokenize(expr, ok);
    if (!ok || tokens.isEmpty())
        return { 0, EvalStatus::Error, false };

    // Handle percentage: find patterns like [Number] [+/-] [Number] [%]
    // Transform N% after +/- into a multiplication
    for (int i = 0; i < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::Percent) {
            // The preceding token should be a number
            if (i < 1 || tokens[i - 1].type != TokenType::Number) {
                return { 0, EvalStatus::Error, false };
            }

            double pctVal = tokens[i - 1].number;

            // Check if there's a + or - before the percentage number
            // Pattern: ... value +/- number %
            if (i >= 3 && tokens[i - 2].type == TokenType::Op &&
                (tokens[i - 2].op == '+' || tokens[i - 2].op == '-') &&
                tokens[i - 3].type == TokenType::Number) {
                // X + N% means X + (X * N / 100)
                // X - N% means X - (X * N / 100)
                // Replace: remove the percent token and change the number to (base * pct / 100)
                // But base might be a complex expression... for simplicity, just handle the immediate case
                double base = tokens[i - 3].number;
                tokens[i - 1].number = base * pctVal / 100.0;
                tokens.removeAt(i); // remove %
                --i;
            } else {
                // Standalone percentage: N% → N/100
                tokens[i - 1].number = pctVal / 100.0;
                tokens.removeAt(i);
                --i;
            }
        }
    }

    // Shunting-yard algorithm
    std::stack<double> output;
    std::stack<QChar> ops;

    auto applyOp = [&]() -> bool {
        if (output.size() < 2) return false;
        double b = output.top(); output.pop();
        double a = output.top(); output.pop();
        QChar op = ops.top(); ops.pop();
        switch (op.unicode()) {
            case '+': output.push(a + b); break;
            case '-': output.push(a - b); break;
            case '*': output.push(a * b); break;
            case '/':
                if (b == 0) { output.push(0); return false; }
                output.push(a / b);
                break;
            default: return false;
        }
        return true;
    };

    for (const auto &token : tokens) {
        switch (token.type) {
            case TokenType::Number:
                output.push(token.number);
                break;
            case TokenType::Op:
                while (!ops.empty() && ops.top() != '(' &&
                       precedence(ops.top()) >= precedence(token.op)) {
                    if (!applyOp()) return { 0, EvalStatus::Error, false };
                }
                ops.push(token.op);
                break;
            case TokenType::LParen:
                ops.push('(');
                break;
            case TokenType::RParen:
                while (!ops.empty() && ops.top() != '(') {
                    if (output.size() >= 1 && ops.top() == '/' && output.top() == 0)
                        return { 0, EvalStatus::DivByZero, false };
                    if (!applyOp()) return { 0, EvalStatus::Error, false };
                }
                if (ops.empty()) return { 0, EvalStatus::Error, false };
                ops.pop(); // pop '('
                break;
            case TokenType::Percent:
                // Already handled above
                break;
        }
    }

    while (!ops.empty()) {
        if (ops.top() == '(') return { 0, EvalStatus::Error, false };
        if (output.size() >= 1 && ops.top() == '/' && output.top() == 0)
            return { 0, EvalStatus::DivByZero, false };
        if (!applyOp()) return { 0, EvalStatus::Error, false };
    }

    if (output.size() != 1)
        return { 0, EvalStatus::Error, false };

    return { output.top(), EvalStatus::Ok, false };
}

QString MathEngine::formatResult(double val, bool currency)
{
    QString prefix = currency ? "$" : "";

    // Check if integer
    if (val == std::floor(val) && std::abs(val) < 1e15) {
        return prefix + QString::number(static_cast<long long>(val));
    }

    return prefix + QString::number(val, 'f', 2);
}
