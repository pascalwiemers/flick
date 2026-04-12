#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace flick {

class RateProvider;
struct UnitDef;

struct MathResult {
    int line = 0;
    std::string text;
    std::string color;
    bool isComment = false;
    bool isSeparator = false;
    bool isTotal = false;
};

struct CurrencySettings {
    std::string primarySymbol = "$";
    std::string primaryCode = "USD";
    std::string secondaryCode = "EUR";
};

class MathEngine {
public:
    MathEngine();

    const std::vector<MathResult> &results() const;
    const std::vector<std::string> &variableNames() const;
    void evaluate(const std::string &text);
    void reevaluate(); // re-run with previously-evaluated text (used on rate refresh)

    // Currency / rate wiring
    void setRateProvider(RateProvider *p);
    void setCurrencySettings(const CurrencySettings &s);
    const CurrencySettings &currencySettings() const;

    // Special modes: extract numbers from natural text
    enum class SpecialMode { None, Total, Avg };
    SpecialMode specialMode() const;
    const std::string &specialResult() const;

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

    void evaluateSpecialMode(const std::string &text, SpecialMode mode);
    static std::vector<double> extractNumbers(const std::string &text, bool &hasCurrency);

    // Try to parse a single line as a unit or currency conversion.
    // Returns true if the line is a conversion (even an errored one).
    // On success, outDisplay is the text to show (e.g. "25.40 cm"),
    // outNumeric is the raw number to store in a variable, and outIsError
    // indicates a dimension mismatch or unavailable rate.
    bool tryParseConversionLine(const std::string &line, std::string &outDisplay,
                                double &outNumeric, bool &outIsError);

    static std::string formatWithUnit(double value, const UnitDef &unit);

    std::vector<MathResult> m_results;
    std::vector<std::string> m_variableNames;
    SpecialMode m_specialMode = SpecialMode::None;
    std::string m_specialResult;

    RateProvider *m_rateProvider = nullptr;
    CurrencySettings m_currencySettings;
    std::string m_lastText; // for reevaluate()
};

} // namespace flick
