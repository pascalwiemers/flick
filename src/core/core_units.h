#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace flick {

struct RateStore; // fwd

enum class Dimension {
    Distance,
    Area,
    Volume,
    Mass,
    Temperature,
    Currency,
};

struct UnitDef {
    const char *canonical; // display name
    Dimension dim;
    double toBase; // multiplier to base unit (m, m^2, m^3, kg, K, or 1 for currency)
    double offset; // additive, temperature only
};

class UnitRegistry {
public:
    static const UnitRegistry &instance();

    // Look up any non-currency unit by token (handles aliases, case, whitespace).
    const UnitDef *findUnit(std::string_view token) const;

    // Look up a currency by ISO-ish code (case-insensitive).
    const UnitDef *findCurrency(std::string_view code) const;

private:
    UnitRegistry();
    std::unordered_map<std::string, const UnitDef *> m_units;      // normalized alias -> def
    std::unordered_map<std::string, const UnitDef *> m_currencies; // upper-case code -> def
};

// Physical (non-currency) conversion. Returns false on dimension mismatch.
bool convertPhysical(double value, const UnitDef &from, const UnitDef &to, double &out);

// Currency conversion via a RateStore. Returns false if either code is missing.
bool convertCurrency(double value, const UnitDef &from, const UnitDef &to,
                     const RateStore &store, double &out);

} // namespace flick
