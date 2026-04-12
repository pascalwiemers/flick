#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace flick {

// Holds currency exchange rates expressed as "units of code per 1 unit of base".
// Overrides take precedence over fetched rates.
struct RateStore {
    std::string base = "USD";
    std::unordered_map<std::string, double> rates;     // upper-case code -> rate
    std::unordered_map<std::string, double> overrides; // upper-case code -> rate
    int64_t fetchedAt = 0;                             // unix seconds
    bool fromCache = false;

    bool hasRate(const std::string &code) const;
    double rateOf(const std::string &code) const; // 0 if missing
    bool convert(double amount, const std::string &from, const std::string &to, double &out) const;
};

} // namespace flick
