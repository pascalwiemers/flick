# macOS parity: unit & currency conversion

The core parser for unit and currency conversion lives in `src/core/` and is
already compiled into the macOS target (`CORE_SOURCES` in `CMakeLists.txt`).
Measurement conversions (`10" to cm`, `100 F to C`, `1 gallon to L`, …) work
today on macOS without any additional code — they don't touch the network.

What's missing is the **currency rate fetcher**. On Linux/Qt this is
`QtRateProvider` (`src/qml/qt_rateprovider.{h,cpp}`), which implements the
`flick::RateProvider` interface (`src/core/core_rateprovider.h`) using
`QNetworkAccessManager`. macOS needs an equivalent built on `NSURLSession`.
Until that ships, currency lines on macOS will render as `CODE (no rates)`.

## What to build

Create a new Objective-C++ class `RateProviderManager` that subclasses nothing
on the Obj-C side but *inherits from `flick::RateProvider`* on the C++ side.
Mirror the shape of `GitHubSyncManager.mm` for the `NSURLSession` plumbing.

### Files to add

- `src/macos/RateProviderManager.h`
- `src/macos/RateProviderManager.mm`

### Interface to implement

From `src/core/core_rateprovider.h`:

```cpp
class RateProvider {
public:
    virtual const RateStore &store() const = 0;
    virtual void requestRefresh() = 0;
    virtual void setOverrides(std::unordered_map<std::string,double>) = 0;
    virtual void setUpdateDaily(bool) = 0;
    std::function<void()> onRatesChanged;  // fire after every store update
};
```

`RateStore` lives in `src/core/core_ratestore.h`. All rates are stored as
"units of code per 1 unit of base" with `base = "USD"`. The provider owns one
`RateStore` and keeps it up to date.

### Sketch

```objc
// RateProviderManager.h
#pragma once
#include "../core/core_rateprovider.h"
#include "../core/core_ratestore.h"

class RateProviderManager : public flick::RateProvider {
public:
    RateProviderManager();
    ~RateProviderManager() override;

    const flick::RateStore &store() const override { return m_store; }
    void requestRefresh() override;
    void setOverrides(std::unordered_map<std::string, double>) override;
    void setUpdateDaily(bool enabled) override { m_updateDaily = enabled; }

private:
    void loadCache();
    void saveCache();
    void fetchFiatPrimary();
    void fetchFiatFallback();
    void fetchCrypto();
    void finishRefresh();

    flick::RateStore m_store;
    bool m_updateDaily = true;
    bool m_refreshing = false;
    void *m_session; // NSURLSession*, stored as void* to keep header C++-clean
};
```

```objc
// RateProviderManager.mm
#import <Foundation/Foundation.h>
#import "RateProviderManager.h"

static NSString *cachePath() {
    NSArray<NSString *> *paths = NSSearchPathForDirectoriesInDomains(
        NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString *dir = [paths.firstObject stringByAppendingPathComponent:@"flick"];
    [[NSFileManager defaultManager] createDirectoryAtPath:dir
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];
    return [dir stringByAppendingPathComponent:@"rates.json"];
}

RateProviderManager::RateProviderManager() {
    m_store.base = "USD";
    NSURLSession *session = [NSURLSession sharedSession];
    m_session = (__bridge_retained void *)session;
    loadCache();
}

RateProviderManager::~RateProviderManager() {
    if (m_session) CFRelease((CFTypeRef)m_session);
}

void RateProviderManager::requestRefresh() {
    if (m_refreshing) return;
    if (m_updateDaily && m_store.fetchedAt > 0) {
        int64_t now = (int64_t)[[NSDate date] timeIntervalSince1970];
        if (now - m_store.fetchedAt < 24 * 3600) return;
    }
    m_refreshing = true;
    fetchFiatPrimary();
}

// Fetch frankfurter.app, then open.er-api.com, then coingecko — same
// cascade as QtRateProvider. On the final step, call finishRefresh().
//
// Use NSURLSessionDataTask with a completion handler; inside the handler,
// hop back to the main queue via dispatch_async(dispatch_get_main_queue(), …)
// before touching m_store, then chain the next fetch.
```

### Data sources (same as Qt side)

1. **Fiat primary**: `https://api.frankfurter.app/latest?from=USD` — ECB daily,
   no key.
2. **Fiat fallback**: `https://open.er-api.com/v6/latest/USD` — fills codes
   frankfurter doesn't cover. Don't overwrite entries the primary already set.
3. **Crypto**: `https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum,…&vs_currencies=usd`.
   Store each coin rate as `1.0 / usdPrice`. Copy the `kCrypto` mapping table
   verbatim from `src/qml/qt_rateprovider.cpp` so both builds know the same
   set of coins.

### Cache format

Write `~/Library/Application Support/flick/rates.json` with the same shape
as the Qt provider:

```json
{
  "base": "USD",
  "fetchedAt": 1728765432,
  "rates": { "EUR": 0.93, "JPY": 149.28, "BTC": 0.0000158, ... }
}
```

On startup, `loadCache()` populates `m_store` and sets `m_store.fromCache = true`
so the engine can evaluate currency lines offline immediately.

### Re-evaluation on refresh

After `saveCache()`, call `onRatesChanged()` if it's set. `MathEngine::setRateProvider`
wires this to `MathEngine::reevaluate()`, which re-runs the last text through the
parser so currency placeholders (`JPY (loading…)`) get replaced with real numbers.

Because `NSURLSession` completion handlers run on a background queue, hop to
the main queue before calling `onRatesChanged()` — the editor touches UIKit
state from that callback chain.

## Wiring into the app

**`src/macos/AppDelegate.mm` (around line 9)** — create and own the provider
alongside the `MathEngine`:

```objc
#include "RateProviderManager.h"

// In applicationDidFinishLaunching (or wherever MathEngine is created):
self.mathEngine = new flick::MathEngine();
self.rateProvider = new RateProviderManager();           // add property
self.mathEngine->setRateProvider(self.rateProvider);
```

Add a matching `@property (nonatomic) RateProviderManager *rateProvider;`
to `AppDelegate`, and `delete self.rateProvider;` in `dealloc` (after the
`MathEngine` is torn down so the callback can't fire into a dangling pointer).

No changes are needed inside `EditorViewController.mm` — the engine calls
`requestRefresh()` automatically whenever it hits a currency conversion with
missing or stale rates, and `MathEngine::reevaluate()` handles the re-render.

## CMake

Add the new files to the macOS target in `CMakeLists.txt`:

```cmake
add_executable(flick MACOSX_BUNDLE
    ${CORE_SOURCES}
    src/macos/main.mm
    src/macos/AppDelegate.mm
    src/macos/EditorViewController.mm
    src/macos/GitHubSyncManager.mm
    src/macos/RateProviderManager.mm   # new
)
```

No new frameworks needed — `Foundation` (already linked) provides `NSURLSession`.

## Verification

On a macOS build, try each line and confirm it matches the Linux output:

| Input                  | Expected                    |
|------------------------|-----------------------------|
| `10" to cm =`          | `25.40 cm`                  |
| `5 km to mi =`         | `3.11 mi`                   |
| `100 F to C =`         | `37.78 °C`                  |
| `58 cm to in =`        | `22 13/16"`                 |
| `1 tsubo to sqm =`     | `3.31 m²`                   |
| `10 USD to JPY =`      | live value (e.g. `1,492.84 JPY`) |
| `$10 =`                | live value in EUR (default secondary) |
| `1 BTC to USD =`       | live value from CoinGecko   |

Kill the network, relaunch, and confirm the cached rates still produce numbers
(timestamps on disk verify `~/Library/Application Support/flick/rates.json`
exists).

## Out of scope for this follow-up

- **Settings UI**: primary symbol, primary/secondary code, custom overrides,
  and the daily-update toggle aren't persisted or surfaced in Preferences on
  either platform yet. That's a separate cross-platform task and should land
  in the core `MathEngine` settings API, not inside the platform layer.
- **Dark-mode theming of conversion results**: result color comes from the
  engine (`#5daa5d` / `#cc6666`) and already matches the rest of the math
  engine output, so no additional macOS work is needed.
