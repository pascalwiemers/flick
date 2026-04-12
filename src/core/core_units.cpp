#include "core_units.h"
#include "core_ratestore.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace flick {

// ── Static unit tables ──────────────────────────────────────────────

namespace {

// Distance (base: meter)
static const UnitDef kDistance[] = {
    {"m", Dimension::Distance, 1.0, 0},
    {"cm", Dimension::Distance, 0.01, 0},
    {"mm", Dimension::Distance, 0.001, 0},
    {"km", Dimension::Distance, 1000.0, 0},
    {"in", Dimension::Distance, 0.0254, 0},
    {"ft", Dimension::Distance, 0.3048, 0},
    {"yd", Dimension::Distance, 0.9144, 0},
    {"mi", Dimension::Distance, 1609.344, 0},
};

// Area (base: m^2)
static const UnitDef kArea[] = {
    {"m²", Dimension::Area, 1.0, 0},
    {"cm²", Dimension::Area, 1e-4, 0},
    {"mm²", Dimension::Area, 1e-6, 0},
    {"km²", Dimension::Area, 1e6, 0},
    {"ft²", Dimension::Area, 0.09290304, 0},
    {"in²", Dimension::Area, 0.00064516, 0},
    {"yd²", Dimension::Area, 0.83612736, 0},
    {"mi²", Dimension::Area, 2589988.110336, 0},
    {"ha", Dimension::Area, 10000.0, 0},
    {"acre", Dimension::Area, 4046.8564224, 0},
    {"are", Dimension::Area, 100.0, 0},
    {"dunam", Dimension::Area, 1000.0, 0},
    {"tsubo", Dimension::Area, 3.305785, 0},
    {"pyeong", Dimension::Area, 3.305785, 0},
    {"bigha", Dimension::Area, 2529.285264, 0}, // Indian standard (2500 sq yd)
    {"cuerda", Dimension::Area, 3930.395625, 0},
    {"rood", Dimension::Area, 1011.7141056, 0},
    {"perch", Dimension::Area, 25.29285264, 0}, // square perch
    {"section", Dimension::Area, 2589988.110336, 0},
    {"township", Dimension::Area, 93239571.972, 0},
};

// Volume (base: m^3)
static const UnitDef kVolume[] = {
    {"L", Dimension::Volume, 1e-3, 0},
    {"mL", Dimension::Volume, 1e-6, 0},
    {"m³", Dimension::Volume, 1.0, 0},
    {"cm³", Dimension::Volume, 1e-6, 0},
    {"mm³", Dimension::Volume, 1e-9, 0},
    {"gal", Dimension::Volume, 0.003785411784, 0},
    {"qt", Dimension::Volume, 0.000946352946, 0},
    {"pt", Dimension::Volume, 0.000473176473, 0},
    {"cup", Dimension::Volume, 0.0002365882365, 0},
    {"fl oz", Dimension::Volume, 2.95735295625e-5, 0},
    {"tbsp", Dimension::Volume, 1.47867647813e-5, 0},
    {"tsp", Dimension::Volume, 4.92892159375e-6, 0},
    {"in³", Dimension::Volume, 1.6387064e-5, 0},
    {"ft³", Dimension::Volume, 0.028316846592, 0},
    {"yd³", Dimension::Volume, 0.764554857984, 0},
    {"UK gal", Dimension::Volume, 0.00454609, 0},
    {"UK qt", Dimension::Volume, 0.0011365225, 0},
    {"UK pt", Dimension::Volume, 0.00056826125, 0},
    {"UK cup", Dimension::Volume, 0.000284130625, 0},
    {"UK fl oz", Dimension::Volume, 2.84130625e-5, 0},
    {"UK tbsp", Dimension::Volume, 1.7758164e-5, 0},
    {"UK tsp", Dimension::Volume, 5.91938802e-6, 0},
};

// Mass (base: kg)
static const UnitDef kMass[] = {
    {"kg", Dimension::Mass, 1.0, 0},
    {"g", Dimension::Mass, 1e-3, 0},
    {"mg", Dimension::Mass, 1e-6, 0},
    {"µg", Dimension::Mass, 1e-9, 0},
    {"t", Dimension::Mass, 1000.0, 0}, // metric ton
    {"lb", Dimension::Mass, 0.45359237, 0},
    {"oz", Dimension::Mass, 0.028349523125, 0},
    {"st", Dimension::Mass, 6.35029318, 0},
    {"short ton", Dimension::Mass, 907.18474, 0},
    {"long ton", Dimension::Mass, 1016.0469088, 0},
};

// Temperature (base: K). base = value * toBase + offset.
static const UnitDef kTemperature[] = {
    {"K", Dimension::Temperature, 1.0, 0},
    {"°C", Dimension::Temperature, 1.0, 273.15},
    {"°F", Dimension::Temperature, 5.0 / 9.0, 459.67 * 5.0 / 9.0},
};

// Currency: toBase is ignored (rates come from RateStore at runtime).
// We need one UnitDef per code, with canonical = the code itself.
// Declared via a helper so we don't hand-type 100+ entries.
struct CurrencyRow {
    const char *code;
};
static const CurrencyRow kCurrencyRows[] = {
    {"1INCH"}, {"AAVE"}, {"ADA"}, {"AED"}, {"AFN"}, {"AGIX"}, {"AKT"}, {"ALGO"},
    {"ALL"}, {"AMD"}, {"AMP"}, {"ANG"}, {"AOA"}, {"APE"}, {"APT"}, {"AR"},
    {"ARB"}, {"ARS"}, {"ATOM"}, {"ATS"}, {"AUD"}, {"AVAX"}, {"AWG"}, {"AXS"},
    {"AZM"}, {"AZN"}, {"BAKE"}, {"BAM"}, {"BAT"}, {"BBD"}, {"BCH"}, {"BDT"},
    {"BEF"}, {"BGN"}, {"BHD"}, {"BIF"}, {"BMD"}, {"BNB"}, {"BND"}, {"BOB"},
    {"BRL"}, {"BSD"}, {"BSV"}, {"BSW"}, {"BTC"}, {"BTG"}, {"BTN"}, {"BTT"},
    {"BUSD"}, {"BWP"}, {"BYN"}, {"BYR"}, {"BZD"}, {"CAD"}, {"CAKE"}, {"CDF"},
    {"CELO"}, {"CFX"}, {"CHF"}, {"CHZ"}, {"CLP"}, {"CNH"}, {"CNY"}, {"COMP"},
    {"COP"}, {"CRC"}, {"CRO"}, {"CRV"}, {"CSPR"}, {"CUC"}, {"CUP"}, {"CVE"},
    {"CVX"}, {"CYP"}, {"CZK"}, {"DAI"}, {"DASH"}, {"DCR"}, {"DEM"}, {"DFI"},
    {"DJF"}, {"DKK"}, {"DOGE"}, {"DOP"}, {"DOT"}, {"DYDX"}, {"DZD"}, {"EEK"},
    {"EGLD"}, {"EGP"}, {"ENJ"}, {"EOS"}, {"ERN"}, {"ESP"}, {"ETB"}, {"ETC"},
    {"ETH"}, {"EUR"}, {"FEI"}, {"FIL"},
    // Common extras not strictly in the spec but often needed:
    {"GBP"}, {"HKD"}, {"JPY"}, {"KRW"}, {"MXN"}, {"NOK"}, {"NTD"}, {"NZD"},
    {"PHP"}, {"PLN"}, {"RUB"}, {"SEK"}, {"SGD"}, {"THB"}, {"TRY"}, {"TWD"},
    {"USD"}, {"ZAR"},
};

// Built once at first use.
static std::vector<UnitDef> &currencyStorage() {
    static std::vector<UnitDef> v = []() {
        std::vector<UnitDef> out;
        const size_t n = sizeof(kCurrencyRows) / sizeof(kCurrencyRows[0]);
        out.reserve(n);
        for (size_t i = 0; i < n; ++i)
            out.push_back({kCurrencyRows[i].code, Dimension::Currency, 1.0, 0.0});
        return out;
    }();
    return v;
}

// ── Normalization helpers ──────────────────────────────────────────

static std::string normalize(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    bool lastSpace = true;
    for (char c : s) {
        unsigned char uc = (unsigned char)c;
        if (std::isspace(uc)) {
            if (!lastSpace) {
                out += ' ';
                lastSpace = true;
            }
        } else {
            // Lowercase ASCII only; leave non-ASCII (°, ², ³, µ, ″, ′, 坪, etc.) alone.
            if (uc < 128)
                out += (char)std::tolower(uc);
            else
                out += c;
            lastSpace = false;
        }
    }
    if (!out.empty() && out.back() == ' ')
        out.pop_back();
    return out;
}

static std::string upper(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        unsigned char uc = (unsigned char)c;
        if (uc < 128)
            out += (char)std::toupper(uc);
        else
            out += c;
    }
    return out;
}

} // namespace

// ── Registry init ───────────────────────────────────────────────────

UnitRegistry::UnitRegistry() {
    auto add = [this](const UnitDef *def, std::initializer_list<const char *> aliases) {
        for (const char *a : aliases)
            m_units[normalize(a)] = def;
    };

    // Distance
    add(&kDistance[0], {"m", "meter", "metre", "meters", "metres"});
    add(&kDistance[1], {"cm", "centimeter", "centimetre", "centimeters", "centimetres"});
    add(&kDistance[2], {"mm", "millimeter", "millimetre", "millimeters", "millimetres"});
    add(&kDistance[3], {"km", "kilometer", "kilometre", "kilometers", "kilometres"});
    add(&kDistance[4], {"in", "inch", "inches", "\"", "\xe2\x80\xb3" /* ″ */});
    add(&kDistance[5], {"ft", "foot", "feet", "'", "\xe2\x80\xb2" /* ′ */});
    add(&kDistance[6], {"yd", "yard", "yards"});
    add(&kDistance[7], {"mi", "mile", "miles"});

    // Area
    add(&kArea[0], {"sqm", "m²", "m^2", "sq m", "square meter", "square meters",
                    "sq meters", "sq meter", "square m"});
    add(&kArea[1], {"sqcm", "cm²", "cm^2", "sq cm", "square centimeter", "square centimeters",
                    "sq centimeters", "sq centimeter", "square cm"});
    add(&kArea[2], {"sqmm", "mm²", "mm^2", "sq mm", "square millimeter", "square millimeters",
                    "sq millimeters", "sq millimeter", "square mm"});
    add(&kArea[3], {"sqkm", "km²", "km^2", "sq km", "square kilometer", "square kilometers",
                    "sq kilometers", "sq kilometer", "square km"});
    add(&kArea[4], {"sqft", "ft²", "ft^2", "sq ft", "square foot", "square feet",
                    "sq feet", "sq foot", "square ft"});
    add(&kArea[5], {"sqin", "in²", "in^2", "sq in", "square inch", "square inches",
                    "sq inches", "sq inch", "square in"});
    add(&kArea[6], {"sqyd", "yd²", "yd^2", "sq yd", "square yard", "square yards",
                    "sq yards", "sq yard", "square yd"});
    add(&kArea[7], {"sqmi", "mi²", "mi^2", "sq mi", "square mile", "square miles",
                    "sq miles", "sq mile", "square mi"});
    add(&kArea[8], {"ha", "hectare", "hectares"});
    add(&kArea[9], {"acre", "acres"});
    add(&kArea[10], {"are", "ares"});
    add(&kArea[11], {"dunam", "dunams", "donum", "donums"});
    add(&kArea[12], {"tsubo", "tsubos", "\xe5\x9d\xaa" /* 坪 */, "\xe5\x9d\xaa\xe6\x95\xb0" /* 坪数 */});
    add(&kArea[13], {"pyeong", "pyong", "ping", "pings"});
    add(&kArea[14], {"bigha", "bighas"});
    add(&kArea[15], {"cuerda", "cuerdas"});
    add(&kArea[16], {"rood", "roods"});
    add(&kArea[17], {"perch", "perches"});
    add(&kArea[18], {"section", "sections"});
    add(&kArea[19], {"township", "townships"});

    // Volume
    add(&kVolume[0], {"l", "ls", "liter", "liters", "litre", "litres"});
    add(&kVolume[1], {"ml", "mls", "milliliter", "milliliters"});
    add(&kVolume[2], {"m³", "m^3", "cubicmeter", "cubic meter", "cubic meters"});
    add(&kVolume[3], {"cm³", "cm^3", "cc", "cubiccentimeter", "cubic centimeter",
                      "cubic centimeters", "cubic cm"});
    add(&kVolume[4], {"mm³", "mm^3", "cubicmillimeter", "cubic millimeter",
                      "cubic millimeters", "cubic mm"});
    add(&kVolume[5], {"gal", "gals", "gallon", "gallons", "us gallon", "us gallons"});
    add(&kVolume[6], {"qt", "qts", "quart", "quarts"});
    add(&kVolume[7], {"pt", "pts", "pint", "pints"});
    add(&kVolume[8], {"cup", "cups"});
    add(&kVolume[9], {"fl oz", "fl ozs", "fluidounce", "fluid ounce", "fluid ounces", "floz"});
    add(&kVolume[10], {"tbsp", "tbsps", "tablespoon", "tablespoons"});
    add(&kVolume[11], {"tsp", "tsps", "teaspoon", "teaspoons"});
    add(&kVolume[12], {"in³", "in^3", "cubicinch", "cubic inch", "cubic inches", "cu in"});
    add(&kVolume[13], {"ft³", "ft^3", "cubicfoot", "cubic foot", "cubic feet", "cu ft"});
    add(&kVolume[14], {"yd³", "yd^3", "cubicyard", "cubic yard", "cubic yards", "cu yd"});
    add(&kVolume[15], {"uk gal", "ukgallon", "imperial gallon", "uk gallon", "uk gallons",
                       "imperial gallons"});
    add(&kVolume[16], {"uk qt", "ukquart", "imperial quart", "uk quart", "uk quarts",
                       "imperial quarts"});
    add(&kVolume[17], {"uk pt", "ukpint", "imperial pint", "uk pint", "uk pints",
                       "imperial pints"});
    add(&kVolume[18], {"uk cup", "ukcup", "imperial cup", "uk cup", "uk cups", "imperial cups"});
    add(&kVolume[19], {"uk fl oz", "ukfluidounce", "imperial fluid ounce", "uk fluid ounce",
                       "uk fluid ounces", "imperial fluid ounces"});
    add(&kVolume[20], {"uk tbsp", "uktablespoon", "imperial tablespoon", "uk tablespoon",
                       "uk tablespoons", "imperial tablespoons"});
    add(&kVolume[21], {"uk tsp", "ukteaspoon", "imperial teaspoon", "uk teaspoon",
                       "uk teaspoons", "imperial teaspoons"});

    // Mass
    add(&kMass[0], {"kg", "kgs", "kilogram", "kilograms"});
    add(&kMass[1], {"g", "gs", "gram", "grams"});
    add(&kMass[2], {"mg", "mgs", "milligram", "milligrams"});
    add(&kMass[3], {"µg", "ug", "mcg", "microgram", "micrograms"});
    add(&kMass[4], {"t", "tonne", "tonnes", "metric ton", "metric tons"});
    add(&kMass[5], {"lb", "lbs", "pound", "pounds"});
    add(&kMass[6], {"oz", "ozs", "ounce", "ounces"});
    add(&kMass[7], {"st", "stone", "stones"});
    add(&kMass[8], {"ton", "tons", "short ton", "short tons", "us ton", "us tons"});
    add(&kMass[9], {"long ton", "long tons", "uk ton", "uk tons", "imperial ton", "imperial tons"});

    // Temperature
    add(&kTemperature[0], {"k", "kelvin", "kelvins", "degrees k"});
    add(&kTemperature[1], {"c", "°c", "celsius", "centigrade", "degrees celsius", "degrees c"});
    add(&kTemperature[2], {"f", "°f", "fahrenheit", "degrees fahrenheit", "degrees f"});

    // Currencies
    auto &store = currencyStorage();
    for (auto &def : store)
        m_currencies[def.canonical] = &def;
}

const UnitRegistry &UnitRegistry::instance() {
    static UnitRegistry reg;
    return reg;
}

const UnitDef *UnitRegistry::findUnit(std::string_view token) const {
    auto it = m_units.find(normalize(token));
    return (it != m_units.end()) ? it->second : nullptr;
}

const UnitDef *UnitRegistry::findCurrency(std::string_view code) const {
    auto it = m_currencies.find(upper(code));
    return (it != m_currencies.end()) ? it->second : nullptr;
}

// ── Conversion math ────────────────────────────────────────────────

bool convertPhysical(double value, const UnitDef &from, const UnitDef &to, double &out) {
    if (from.dim != to.dim || from.dim == Dimension::Currency)
        return false;
    // base = value * from.toBase + from.offset
    // out  = (base - to.offset) / to.toBase
    double base = value * from.toBase + from.offset;
    out = (base - to.offset) / to.toBase;
    return true;
}

bool convertCurrency(double value, const UnitDef &from, const UnitDef &to,
                     const RateStore &store, double &out) {
    if (from.dim != Dimension::Currency || to.dim != Dimension::Currency)
        return false;
    return store.convert(value, from.canonical, to.canonical, out);
}

} // namespace flick
