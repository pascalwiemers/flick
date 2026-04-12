#include "core_ratestore.h"

#include <cctype>

namespace flick {

static std::string upper(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        unsigned char uc = (unsigned char)c;
        out += (uc < 128) ? (char)std::toupper(uc) : c;
    }
    return out;
}

bool RateStore::hasRate(const std::string &code) const {
    std::string u = upper(code);
    if (u == base) return true;
    if (overrides.count(u)) return true;
    return rates.count(u) > 0;
}

double RateStore::rateOf(const std::string &code) const {
    std::string u = upper(code);
    if (u == base) return 1.0;
    auto ov = overrides.find(u);
    if (ov != overrides.end()) return ov->second;
    auto it = rates.find(u);
    return it != rates.end() ? it->second : 0.0;
}

bool RateStore::convert(double amount, const std::string &from, const std::string &to,
                        double &out) const {
    double fromRate = rateOf(from);
    double toRate = rateOf(to);
    if (fromRate <= 0 || toRate <= 0) return false;
    // amount in `from` → base → `to`: base = amount / fromRate; out = base * toRate
    out = amount * toRate / fromRate;
    return true;
}

} // namespace flick
