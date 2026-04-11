#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QVector>
#include <optional>

class MathEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)

public:
    explicit MathEngine(QObject *parent = nullptr);

    QVariantList results() const;
    Q_INVOKABLE void evaluate(const QString &text);

signals:
    void resultsChanged();

private:
    struct VarInfo {
        double value;
        bool hasCurrency;
    };

    enum class EvalStatus { Ok, Undefined, DivByZero, Error };

    struct EvalResult {
        double value = 0;
        EvalStatus status = EvalStatus::Error;
        bool hasCurrency = false;
    };

    // Shunting-yard parser
    enum class TokenType { Number, Op, LParen, RParen, Percent };
    struct Token {
        TokenType type;
        double number = 0;
        QChar op;
    };

    QVector<Token> tokenize(const QString &expr, bool &ok);
    EvalResult parseAndEval(const QString &expr);
    QString substituteVars(const QString &expr, const QMap<QString, VarInfo> &vars,
                           bool &hasCurrency, bool &hasUndefined);

    static QString formatResult(double val, bool currency);

    QVariantList m_results;
};
