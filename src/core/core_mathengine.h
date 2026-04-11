#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace flick {

struct MathResult {
    int line = 0;
    std::string text;
    std::string color;
    bool isComment = false;
    bool isSeparator = false;
    bool isTotal = false;
};

class MathEngine {
public:
    MathEngine();

    const std::vector<MathResult> &results() const;
    const std::vector<std::string> &variableNames() const;
    void evaluate(const std::string &text);

    std::function<void()> onResultsChanged;
    std::function<void()> onVariableNamesChanged;

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

    enum class TokenType { Number, Op, LParen, RParen, Percent };
    struct Token {
        TokenType type;
        double number = 0;
        char op = 0;
    };

    std::vector<Token> tokenize(const std::string &expr, bool &ok);
    EvalResult parseAndEval(const std::string &expr);
    std::string substituteVars(const std::string &expr, const std::map<std::string, VarInfo> &vars,
                               bool &hasCurrency, bool &hasUndefined);
    static std::string formatResult(double val, bool currency);

    std::vector<MathResult> m_results;
    std::vector<std::string> m_variableNames;
};

} // namespace flick
