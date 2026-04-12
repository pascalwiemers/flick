#pragma once

#include "core_ratestore.h"

#include <functional>
#include <unordered_map>

namespace flick {

// Platform-implemented interface for fetching and caching currency rates.
// Core code calls `store()` for lookups and `requestRefresh()` when it needs
// rates. The platform layer fires `onRatesChanged` whenever it updates the
// store so the engine can re-evaluate affected lines.
class RateProvider {
public:
    virtual ~RateProvider() = default;

    virtual const RateStore &store() const = 0;
    virtual void requestRefresh() = 0;
    virtual void setOverrides(std::unordered_map<std::string, double> overrides) = 0;
    virtual void setUpdateDaily(bool enabled) = 0;

    std::function<void()> onRatesChanged;
};

} // namespace flick
