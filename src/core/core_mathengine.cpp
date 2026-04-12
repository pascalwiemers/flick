#include "core_mathengine.h"
#include "core_units.h"
#include "core_rateprovider.h"
#include "core_ratestore.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stack>
#include <sstream>
#include <cctype>
#include <cstring>
#include <regex>

namespace flick {

static void notify(const std::function<void()> &fn) {
    if (fn) fn();
}

static std::string toLower(const std::string &s) {
    std::string r = s;
    for (auto &c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static std::string trim(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> splitLines(const std::string &s) {
    std::vector<std::string> lines;
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line))
        lines.push_back(line);
    // If text ends with \n, getline won't produce a trailing empty entry
    // but if text is non-empty and ends with \n, we need to match Qt's split behavior
    if (!s.empty() && s.back() == '\n')
        lines.emplace_back();
    return lines;
}

static bool isPureText(const std::string &s) {
    for (auto c : s) {
        if (std::isdigit((unsigned char)c)) return false;
    }
    return true;
}

static std::string removeChar(const std::string &s, char ch) {
    std::string r;
    r.reserve(s.size());
    for (auto c : s) {
        if (c != ch) r += c;
    }
    return r;
}

MathEngine::MathEngine() {}

const std::vector<MathResult> &MathEngine::results() const { return m_results; }
const std::vector<std::string> &MathEngine::variableNames() const { return m_variableNames; }

MathEngine::SpecialMode MathEngine::specialMode() const { return m_specialMode; }
const std::string &MathEngine::specialResult() const { return m_specialResult; }

void MathEngine::setRateProvider(RateProvider *p) {
    m_rateProvider = p;
    if (p) {
        p->onRatesChanged = [this]() { reevaluate(); };
    }
}

void MathEngine::setCurrencySettings(const CurrencySettings &s) { m_currencySettings = s; }
const CurrencySettings &MathEngine::currencySettings() const { return m_currencySettings; }

void MathEngine::reevaluate() {
    if (!m_lastText.empty())
        evaluate(m_lastText);
}

// ── Conversion line parser ─────────────────────────────────────────

namespace {

// Format a number with thousands separator and given decimal places.
static std::string formatNumber(double value, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    std::string s = buf;
    // Split integer / fractional
    size_t dot = s.find('.');
    std::string intPart = (dot == std::string::npos) ? s : s.substr(0, dot);
    std::string fracPart = (dot == std::string::npos) ? "" : s.substr(dot);
    bool negative = !intPart.empty() && intPart[0] == '-';
    if (negative) intPart.erase(0, 1);
    // Insert commas every 3 digits from right
    std::string grouped;
    int count = 0;
    for (int i = (int)intPart.size() - 1; i >= 0; --i) {
        if (count && count % 3 == 0) grouped += ',';
        grouped += intPart[i];
        ++count;
    }
    std::reverse(grouped.begin(), grouped.end());
    return (negative ? "-" : "") + grouped + fracPart;
}

// Nearest 1/16 inch, e.g. 22.8125 -> "22 13/16"
static std::string formatInchFraction(double inches) {
    bool neg = inches < 0;
    double a = std::abs(inches);
    long long whole = (long long)std::floor(a);
    double frac = a - whole;
    int sixteenths = (int)std::round(frac * 16.0);
    if (sixteenths == 16) {
        whole += 1;
        sixteenths = 0;
    }
    std::string out;
    if (neg) out += "-";
    out += std::to_string(whole);
    if (sixteenths > 0) {
        int num = sixteenths;
        int den = 16;
        while (num % 2 == 0 && den % 2 == 0) {
            num /= 2;
            den /= 2;
        }
        out += ' ';
        out += std::to_string(num);
        out += '/';
        out += std::to_string(den);
    }
    return out;
}

// Decide decimal places for a value. Default is 2; very small non-zero
// values get more precision so they don't render as "0.00".
static int decimalsFor(const UnitDef &unit, double value) {
    (void)unit;
    double a = std::abs(value);
    if (a == 0) return 2;
    if (a >= 0.01) return 2;
    if (a >= 1e-5) return 6;
    return 8;
}

// Find " to " (case-insensitive, whitespace-bounded) inside [from, stop).
static size_t findToKeyword(const std::string &s, size_t from, size_t stop) {
    for (size_t i = from; i + 1 < stop; ++i) {
        bool leftOk = (i == from) || std::isspace((unsigned char)s[i - 1]);
        if (!leftOk) continue;
        if ((s[i] == 't' || s[i] == 'T') && (s[i + 1] == 'o' || s[i + 1] == 'O')) {
            bool rightOk = (i + 2 >= stop) || std::isspace((unsigned char)s[i + 2]);
            if (rightOk) return i;
        }
    }
    return std::string::npos;
}

static std::string trimStr(const std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // namespace

std::string MathEngine::formatWithUnit(double value, const UnitDef &unit) {
    int decimals = decimalsFor(unit, value);

    // Special case: inches get a fractional form appended.
    if (unit.dim == Dimension::Distance && std::strcmp(unit.canonical, "in") == 0) {
        std::string frac = formatInchFraction(value);
        return frac + "\"";
    }

    std::string num = formatNumber(value, decimals);
    // Trim trailing zeros on fractional part, but keep at least 2 decimals for readability.
    // (Skip this — keep fixed decimals for consistency with spec examples.)

    // Space before unit, unless unit is a symbolic trailing mark like " or '.
    const char *u = unit.canonical;
    if (u[0] == '"' || u[0] == '\'')
        return num + u;
    return num + " " + u;
}

bool MathEngine::tryParseConversionLine(const std::string &line, std::string &outDisplay,
                                         double &outNumeric, bool &outIsError) {
    outIsError = false;
    std::string s = trimStr(line);
    if (s.empty()) return false;

    // Reject if the line contains arithmetic operators other than a leading sign.
    // (Spec: conversions don't compose with arithmetic.)
    // We inspect the raw string; minus only allowed at position 0 (plus the currency-symbol
    // prefix if present).
    auto hasArithOp = [](const std::string &str, size_t from) {
        for (size_t i = from; i < str.size(); ++i) {
            char c = str[i];
            if (c == '+' || c == '*' || c == '/')
                return true;
            if (c == '-' && i > from)
                return true;
        }
        return false;
    };

    size_t pos = 0;
    bool hasSymbolPrefix = false;
    const std::string &sym = m_currencySettings.primarySymbol;
    if (!sym.empty() && s.compare(0, sym.size(), sym) == 0) {
        hasSymbolPrefix = true;
        pos = sym.size();
        while (pos < s.size() && std::isspace((unsigned char)s[pos])) ++pos;
    }

    if (hasArithOp(s, pos)) return false;

    // Parse the number.
    size_t numStart = pos;
    if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
    size_t digitStart = pos;
    while (pos < s.size() && (std::isdigit((unsigned char)s[pos]) || s[pos] == ',' || s[pos] == '.'))
        ++pos;
    if (pos == digitStart) return false;
    std::string rawNum(s.begin() + numStart, s.begin() + pos);
    std::string cleanedNum;
    for (char c : rawNum)
        if (c != ',') cleanedNum += c;
    double value;
    try {
        value = std::stod(cleanedNum);
    } catch (...) {
        return false;
    }

    // Skip whitespace.
    while (pos < s.size() && std::isspace((unsigned char)s[pos])) ++pos;

    // Trim trailing `=` and whitespace.
    size_t end = s.size();
    while (end > pos && std::isspace((unsigned char)s[end - 1])) --end;
    if (end > pos && s[end - 1] == '=') {
        --end;
        while (end > pos && std::isspace((unsigned char)s[end - 1])) --end;
    }

    // Split on " to ".
    std::string unitBefore, unitAfter;
    size_t toPos = findToKeyword(s, pos, end);
    if (toPos != std::string::npos) {
        unitBefore = s.substr(pos, toPos - pos);
        size_t afterStart = toPos + 2;
        while (afterStart < end && std::isspace((unsigned char)s[afterStart])) ++afterStart;
        unitAfter = s.substr(afterStart, end - afterStart);
    } else {
        unitBefore = s.substr(pos, end - pos);
    }
    unitBefore = trimStr(unitBefore);
    unitAfter = trimStr(unitAfter);

    const UnitRegistry &reg = UnitRegistry::instance();
    const UnitDef *fromUnit = nullptr;
    const UnitDef *toUnit = nullptr;

    if (hasSymbolPrefix) {
        if (!unitBefore.empty()) return false; // "$10 km" — invalid
        fromUnit = reg.findCurrency(m_currencySettings.primaryCode);
        if (!fromUnit) return false;
    } else {
        if (unitBefore.empty()) return false; // bare number; not a conversion
        fromUnit = reg.findUnit(unitBefore);
        if (!fromUnit) fromUnit = reg.findCurrency(unitBefore);
        if (!fromUnit) return false;
    }

    if (!unitAfter.empty()) {
        toUnit = reg.findUnit(unitAfter);
        if (!toUnit) toUnit = reg.findCurrency(unitAfter);
        if (!toUnit) return false;
    } else {
        // No explicit target. Only valid for a symbol-prefix line → primary→secondary.
        if (!hasSymbolPrefix) return false;
        toUnit = reg.findCurrency(m_currencySettings.secondaryCode);
        if (!toUnit) return false;
    }

    if (fromUnit->dim != toUnit->dim) {
        outDisplay = "dim mismatch";
        outNumeric = 0;
        outIsError = true;
        return true;
    }

    double result = 0;
    if (fromUnit->dim == Dimension::Currency) {
        if (!m_rateProvider) {
            outDisplay = std::string(toUnit->canonical) + " (no rates)";
            outNumeric = 0;
            outIsError = true;
            return true;
        }
        const RateStore &store = m_rateProvider->store();
        if (!convertCurrency(value, *fromUnit, *toUnit, store, result)) {
            m_rateProvider->requestRefresh();
            outDisplay = std::string(toUnit->canonical) + " (loading…)";
            outNumeric = 0;
            outIsError = true;
            return true;
        }
    } else {
        if (!convertPhysical(value, *fromUnit, *toUnit, result))
            return false;
    }

    outNumeric = result;
    outDisplay = formatWithUnit(result, *toUnit);
    return true;
}

std::vector<double> MathEngine::extractNumbers(const std::string &text, bool &hasCurrency)
{
    std::vector<double> nums;
    hasCurrency = false;
    // Match $1,234.56 or 1234.56 or $1234 etc.
    std::regex numRe("\\$?([\\d,]+\\.?\\d*)");
    auto begin = std::sregex_iterator(text.begin(), text.end(), numRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string full = (*it)[0].str();
        if (full.find('$') != std::string::npos) hasCurrency = true;
        std::string numStr = removeChar(removeChar(full, '$'), ',');
        try {
            double val = std::stod(numStr);
            nums.push_back(val);
        } catch (...) {}
    }
    return nums;
}

void MathEngine::evaluateSpecialMode(const std::string &text, SpecialMode mode)
{
    auto lines = splitLines(text);
    // Gather text from line 1 onwards (skip the prefix line)
    std::string body;
    for (size_t i = 1; i < lines.size(); ++i) {
        if (i > 1) body += '\n';
        body += lines[i];
    }

    bool hasCurrency = false;
    auto nums = extractNumbers(body, hasCurrency);

    std::vector<MathResult> newResults;

    // Mark first line as comment
    MathResult header;
    header.line = 0;
    header.isComment = true;
    header.color = "#555555";
    newResults.push_back(header);

    if (!nums.empty()) {
        double total = 0;
        for (double n : nums) total += n;
        double result = (mode == SpecialMode::Avg) ? (total / (double)nums.size()) : total;

        std::string label = (mode == SpecialMode::Avg) ? "Avg: " : "Total: ";

        // Show on the last line
        int lastLine = (int)lines.size() - 1;
        if (lastLine < 1) lastLine = 1;

        MathResult sep;
        sep.line = lastLine;
        sep.isSeparator = true;
        sep.color = "#333333";
        newResults.push_back(sep);

        MathResult tot;
        tot.line = lastLine;
        tot.text = label + formatResult(result, hasCurrency);
        tot.color = result < 0 ? "#cc6666" : "#5daa5d";
        tot.isTotal = true;
        newResults.push_back(tot);
    }

    m_specialMode = mode;
    m_results = std::move(newResults);
    m_variableNames.clear();
    m_specialResult.clear();
    notify(onResultsChanged);
}

void MathEngine::evaluate(const std::string &text)
{
    m_lastText = text;
    // Check for special modes first
    auto lines = splitLines(text);
    if (!lines.empty()) {
        std::string first = toLower(trim(lines[0]));
        if (first == "total:" || first.substr(0, 6) == "total:") {
            evaluateSpecialMode(text, SpecialMode::Total);
            return;
        }
        if (first == "avg:" || first.substr(0, 4) == "avg:") {
            evaluateSpecialMode(text, SpecialMode::Avg);
            return;
        }
    }
    m_specialMode = SpecialMode::None;
    m_specialResult.clear();

    std::vector<MathResult> newResults;
    std::map<std::string, VarInfo> vars;

    struct BlockEntry { int line; double value; bool hasCurrency; };
    std::vector<BlockEntry> currentBlock;

    auto flushBlock = [&](int) {
        if (currentBlock.size() >= 2) {
            double total = 0;
            bool blockCurrency = false;
            for (auto &e : currentBlock) {
                total += e.value;
                if (e.hasCurrency) blockCurrency = true;
            }
            int lastLine = currentBlock.back().line;

            MathResult sep;
            sep.line = lastLine;
            sep.color = "#333333";
            sep.isSeparator = true;
            newResults.push_back(sep);

            MathResult tot;
            tot.line = lastLine;
            tot.text = formatResult(total, blockCurrency);
            tot.color = total < 0 ? "#cc6666" : "#5daa5d";
            tot.isTotal = true;
            newResults.push_back(tot);
        }
        currentBlock.clear();
    };

    for (int i = 0; i < (int)lines.size(); ++i) {
        std::string line = trim(lines[i]);

        if (line.empty()) {
            flushBlock(i);
            continue;
        }

        if (line.substr(0, 2) == "//" || line == "math:") {
            MathResult entry;
            entry.line = i;
            entry.color = "#555555";
            entry.isComment = true;
            newResults.push_back(entry);
            continue;
        }

        // Variable assignment check
        auto colonPos = line.find(':');
        bool isAssignment = false;
        std::string varName, exprStr;

        if (colonPos != std::string::npos && colonPos > 0) {
            varName = toLower(trim(line.substr(0, colonPos)));
            exprStr = trim(line.substr(colonPos + 1));

            if (!isPureText(exprStr)) {
                isAssignment = true;
            } else {
                std::string lowerExpr = toLower(exprStr);
                for (auto &kv : vars) {
                    if (lowerExpr.find(kv.first) != std::string::npos) {
                        isAssignment = true;
                        break;
                    }
                }
            }
        }

        if (!isAssignment)
            exprStr = line;

        // ── Try conversion parse first ─────────────────────────────
        {
            std::string convInput;
            bool convWantsResult = false;
            if (isAssignment) {
                convInput = exprStr;
                convWantsResult = true;
            } else {
                // Strip trailing '=' for conversion detection only.
                std::string stripped = trim(line);
                if (!stripped.empty() && stripped.back() == '=') {
                    convInput = trim(stripped.substr(0, stripped.size() - 1));
                    convWantsResult = true;
                }
            }

            if (convWantsResult) {
                std::string convDisplay;
                double convNum = 0;
                bool convErr = false;
                if (tryParseConversionLine(convInput, convDisplay, convNum, convErr)) {
                    if (isAssignment && !convErr) {
                        vars[varName] = { convNum, false };
                    }
                    MathResult entry;
                    entry.line = i;
                    entry.text = (isAssignment ? " = " : " ") + convDisplay;
                    entry.color = convErr ? "#cc6666" : "#5daa5d";
                    newResults.push_back(entry);
                    flushBlock(i);
                    continue;
                }
            }
        }

        bool lineCurrency = exprStr.find('$') != std::string::npos;
        std::string cleanExpr = removeChar(exprStr, '$');

        bool wantsResult = false;
        if (!isAssignment) {
            std::string stripped = trim(cleanExpr);
            if (!stripped.empty() && stripped.back() == '=') {
                wantsResult = true;
                cleanExpr = trim(stripped.substr(0, stripped.size() - 1));
            }
        }

        bool varCurrency = false;
        bool hasUndefined = false;
        std::string substituted = substituteVars(cleanExpr, vars, varCurrency, hasUndefined);
        bool hasCurrency = lineCurrency || varCurrency;

        if (hasUndefined && (isAssignment || wantsResult)) {
            if (wantsResult) {
                MathResult entry;
                entry.line = i;
                entry.text = " ?";
                entry.color = "#cc4444";
                newResults.push_back(entry);
            }
            flushBlock(i);
            continue;
        }

        EvalResult result = parseAndEval(substituted);

        // Fallback: trailing number
        if (result.status == EvalStatus::Error && wantsResult) {
            std::regex trailingNum("(-?\\$?[\\d,]+\\.?\\d*)\\s*$");
            std::smatch match;
            if (std::regex_search(cleanExpr, match, trailingNum)) {
                std::string numStr = removeChar(removeChar(match[1].str(), '$'), ',');
                try {
                    double val = std::stod(numStr);
                    result = { val, EvalStatus::Ok, false };
                } catch (...) {}
            }
        }

        if (result.status == EvalStatus::Ok) {
            if (isAssignment) {
                vars[varName] = { result.value, hasCurrency };
                currentBlock.push_back({ i, result.value, hasCurrency });
                continue;
            }
            if (wantsResult) {
                MathResult entry;
                entry.line = i;
                entry.text = " " + formatResult(result.value, hasCurrency);
                entry.color = result.value < 0 ? "#cc6666" : "#5daa5d";
                newResults.push_back(entry);
            }
            currentBlock.push_back({ i, result.value, hasCurrency });
        } else if (result.status == EvalStatus::DivByZero && wantsResult) {
            MathResult entry;
            entry.line = i;
            entry.text = " \xe2\x88\x9e"; // infinity symbol UTF-8
            entry.color = "#cc6666";
            newResults.push_back(entry);
            flushBlock(i);
        } else if (!isAssignment) {
            flushBlock(i);
        }
    }

    flushBlock((int)lines.size());

    if (m_results.size() != newResults.size()) {
        m_results = std::move(newResults);
        notify(onResultsChanged);
    } else {
        // Simple comparison
        bool changed = false;
        for (size_t i = 0; i < m_results.size(); ++i) {
            auto &a = m_results[i];
            auto &b = newResults[i];
            if (a.line != b.line || a.text != b.text || a.color != b.color ||
                a.isComment != b.isComment || a.isSeparator != b.isSeparator || a.isTotal != b.isTotal) {
                changed = true;
                break;
            }
        }
        if (changed) {
            m_results = std::move(newResults);
            notify(onResultsChanged);
        }
    }

    std::vector<std::string> newVarNames;
    for (auto &kv : vars)
        newVarNames.push_back(kv.first);
    if (m_variableNames != newVarNames) {
        m_variableNames = std::move(newVarNames);
        notify(onVariableNamesChanged);
    }
}

std::string MathEngine::substituteVars(const std::string &expr, const std::map<std::string, VarInfo> &vars,
                                        bool &hasCurrency, bool &hasUndefined)
{
    if (vars.empty())
        return expr;

    // Sort names longest-first
    std::vector<std::string> names;
    for (auto &kv : vars)
        names.push_back(kv.first);
    std::sort(names.begin(), names.end(), [](const std::string &a, const std::string &b) {
        return a.size() > b.size();
    });

    std::string result = expr;
    bool anySubstituted = false;

    for (auto &name : names) {
        std::string lower = toLower(result);
        size_t searchFrom = 0;
        while (true) {
            size_t pos = lower.find(name, searchFrom);
            if (pos == std::string::npos) break;

            bool boundaryBefore = (pos == 0) || !std::isalpha((unsigned char)result[pos - 1]);
            bool boundaryAfter = (pos + name.size() >= result.size()) ||
                                 !std::isalpha((unsigned char)result[pos + name.size()]);

            if (boundaryBefore && boundaryAfter) {
                auto &info = vars.at(name);
                if (info.hasCurrency) hasCurrency = true;

                char buf[64];
                snprintf(buf, sizeof(buf), "%.15g", info.value);
                std::string replacement = buf;
                if (info.value < 0)
                    replacement = "(" + replacement + ")";

                result.replace(pos, name.size(), replacement);
                lower = toLower(result);
                searchFrom = pos + replacement.size();
                anySubstituted = true;
            } else {
                searchFrom = pos + 1;
            }
        }
    }

    if (anySubstituted) {
        for (auto c : result) {
            if (std::isalpha((unsigned char)c)) {
                hasUndefined = true;
                break;
            }
        }
    }

    return result;
}

std::vector<MathEngine::Token> MathEngine::tokenize(const std::string &expr, bool &ok)
{
    std::vector<Token> tokens;
    ok = true;
    int i = 0;
    int len = (int)expr.size();

    while (i < len) {
        char ch = expr[i];

        if (std::isspace((unsigned char)ch)) { ++i; continue; }

        if (std::isdigit((unsigned char)ch) || (ch == '.' && i + 1 < len && std::isdigit((unsigned char)expr[i + 1]))) {
            int start = i;
            while (i < len && (std::isdigit((unsigned char)expr[i]) || expr[i] == '.'))
                ++i;
            try {
                double val = std::stod(expr.substr(start, i - start));
                tokens.push_back({ TokenType::Number, val, 0 });
            } catch (...) {
                ok = false;
                return {};
            }
            continue;
        }

        if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
            if ((ch == '-' || ch == '+') &&
                (tokens.empty() || tokens.back().type == TokenType::Op ||
                 tokens.back().type == TokenType::LParen)) {
                if (ch == '-') {
                    tokens.push_back({ TokenType::Number, 0, 0 });
                    tokens.push_back({ TokenType::Op, 0, ch });
                }
                ++i;
                continue;
            }
            tokens.push_back({ TokenType::Op, 0, ch });
            ++i;
            continue;
        }

        if (ch == '%') { tokens.push_back({ TokenType::Percent, 0, 0 }); ++i; continue; }
        if (ch == '(') { tokens.push_back({ TokenType::LParen, 0, 0 }); ++i; continue; }
        if (ch == ')') { tokens.push_back({ TokenType::RParen, 0, 0 }); ++i; continue; }

        ok = false;
        return {};
    }

    return tokens;
}

static int precedence(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/' || op == '%') return 2;
    return 0;
}

MathEngine::EvalResult MathEngine::parseAndEval(const std::string &expr)
{
    bool ok;
    auto tokens = tokenize(expr, ok);
    if (!ok || tokens.empty())
        return { 0, EvalStatus::Error, false };

    // Handle percentage
    for (int i = 0; i < (int)tokens.size(); ++i) {
        if (tokens[i].type == TokenType::Percent) {
            if (i < 1 || tokens[i - 1].type != TokenType::Number)
                return { 0, EvalStatus::Error, false };

            double pctVal = tokens[i - 1].number;

            if (i >= 3 && tokens[i - 2].type == TokenType::Op &&
                (tokens[i - 2].op == '+' || tokens[i - 2].op == '-') &&
                tokens[i - 3].type == TokenType::Number) {
                double base = tokens[i - 3].number;
                tokens[i - 1].number = base * pctVal / 100.0;
                tokens.erase(tokens.begin() + i);
                --i;
            } else {
                tokens[i - 1].number = pctVal / 100.0;
                tokens.erase(tokens.begin() + i);
                --i;
            }
        }
    }

    std::stack<double> output;
    std::stack<char> ops;

    auto applyOp = [&]() -> bool {
        if (output.size() < 2) return false;
        double b = output.top(); output.pop();
        double a = output.top(); output.pop();
        char op = ops.top(); ops.pop();
        switch (op) {
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

    for (auto &token : tokens) {
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
                ops.pop();
                break;
            case TokenType::Percent:
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

std::string MathEngine::formatResult(double val, bool currency)
{
    std::string prefix = currency ? "$" : "";
    if (val == std::floor(val) && std::abs(val) < 1e15) {
        return prefix + std::to_string((long long)val);
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", val);
    return prefix + buf;
}

} // namespace flick
