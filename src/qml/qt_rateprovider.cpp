#include "qt_rateprovider.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QDebug>

namespace {

QString cachePath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/flick";
    QDir().mkpath(dir);
    return dir + "/rates.json";
}

// Subset of spec crypto codes with known CoinGecko ids.
// Unknown codes simply won't be fetched from CoinGecko but still live
// in the unit registry — a user can set a manual override for them.
struct CryptoMapping {
    const char *code;
    const char *coingeckoId;
};
static const CryptoMapping kCrypto[] = {
    {"1INCH", "1inch"},
    {"AAVE", "aave"},
    {"ADA", "cardano"},
    {"AGIX", "singularitynet"},
    {"AKT", "akash-network"},
    {"ALGO", "algorand"},
    {"AMP", "amp-token"},
    {"APE", "apecoin"},
    {"APT", "aptos"},
    {"AR", "arweave"},
    {"ARB", "arbitrum"},
    {"ATOM", "cosmos"},
    {"AVAX", "avalanche-2"},
    {"AXS", "axie-infinity"},
    {"BAKE", "bakerytoken"},
    {"BAT", "basic-attention-token"},
    {"BCH", "bitcoin-cash"},
    {"BNB", "binancecoin"},
    {"BSV", "bitcoin-cash-sv"},
    {"BSW", "biswap"},
    {"BTC", "bitcoin"},
    {"BTG", "bitcoin-gold"},
    {"BTT", "bittorrent"},
    {"BUSD", "binance-usd"},
    {"CAKE", "pancakeswap-token"},
    {"CELO", "celo"},
    {"CFX", "conflux-token"},
    {"CHZ", "chiliz"},
    {"COMP", "compound-governance-token"},
    {"CRO", "crypto-com-chain"},
    {"CRV", "curve-dao-token"},
    {"CSPR", "casper-network"},
    {"CVX", "convex-finance"},
    {"DAI", "dai"},
    {"DASH", "dash"},
    {"DCR", "decred"},
    {"DFI", "defichain"},
    {"DOGE", "dogecoin"},
    {"DOT", "polkadot"},
    {"DYDX", "dydx"},
    {"EGLD", "elrond-erd-2"},
    {"ENJ", "enjincoin"},
    {"EOS", "eos"},
    {"ETC", "ethereum-classic"},
    {"ETH", "ethereum"},
    {"FEI", "fei-usd"},
    {"FIL", "filecoin"},
};

} // namespace

QtRateProvider::QtRateProvider(QObject *parent) : QObject(parent) {
    m_store.base = "USD";
    loadCache();
}

QtRateProvider::~QtRateProvider() = default;

void QtRateProvider::setOverrides(std::unordered_map<std::string, double> overrides) {
    m_store.overrides = std::move(overrides);
    if (onRatesChanged) onRatesChanged();
}

void QtRateProvider::setUpdateDaily(bool enabled) { m_updateDaily = enabled; }

void QtRateProvider::loadCache() {
    QFile f(cachePath());
    if (!f.open(QIODevice::ReadOnly)) return;
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    auto obj = doc.object();
    m_store.base = obj.value("base").toString("USD").toStdString();
    m_store.fetchedAt = (int64_t)obj.value("fetchedAt").toDouble(0);
    m_store.fromCache = true;
    auto rates = obj.value("rates").toObject();
    for (auto it = rates.begin(); it != rates.end(); ++it) {
        m_store.rates[it.key().toStdString()] = it.value().toDouble();
    }
}

void QtRateProvider::saveCache() {
    QJsonObject obj;
    obj["base"] = QString::fromStdString(m_store.base);
    obj["fetchedAt"] = (double)m_store.fetchedAt;
    QJsonObject rates;
    for (auto &kv : m_store.rates)
        rates[QString::fromStdString(kv.first)] = kv.second;
    obj["rates"] = rates;
    QFile f(cachePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void QtRateProvider::requestRefresh() {
    if (m_refreshing) return;
    if (m_updateDaily && m_store.fetchedAt > 0) {
        int64_t now = QDateTime::currentSecsSinceEpoch();
        if (now - m_store.fetchedAt < 24 * 3600) return;
    }
    m_refreshing = true;
    fetchFiatPrimary();
}

void QtRateProvider::fetchFiatPrimary() {
    QNetworkRequest req(QUrl("https://api.frankfurter.app/latest?from=USD"));
    req.setRawHeader("Accept", "application/json");
    auto *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            auto obj = QJsonDocument::fromJson(reply->readAll()).object();
            auto rates = obj.value("rates").toObject();
            for (auto it = rates.begin(); it != rates.end(); ++it)
                m_store.rates[it.key().toStdString()] = it.value().toDouble();
            m_store.rates["USD"] = 1.0;
        } else {
            qDebug() << "frankfurter error:" << reply->errorString();
        }
        fetchFiatFallback();
    });
}

void QtRateProvider::fetchFiatFallback() {
    QNetworkRequest req(QUrl("https://open.er-api.com/v6/latest/USD"));
    req.setRawHeader("Accept", "application/json");
    auto *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            auto obj = QJsonDocument::fromJson(reply->readAll()).object();
            auto rates = obj.value("rates").toObject();
            // Don't overwrite entries the primary already filled in — it's more authoritative.
            for (auto it = rates.begin(); it != rates.end(); ++it) {
                auto code = it.key().toStdString();
                if (m_store.rates.find(code) == m_store.rates.end())
                    m_store.rates[code] = it.value().toDouble();
            }
        } else {
            qDebug() << "open.er-api error:" << reply->errorString();
        }
        fetchCrypto();
    });
}

void QtRateProvider::fetchCrypto() {
    QStringList ids;
    for (const auto &m : kCrypto)
        ids << QString::fromLatin1(m.coingeckoId);
    QUrl url("https://api.coingecko.com/api/v3/simple/price");
    QString query = "ids=" + ids.join(',') + "&vs_currencies=usd";
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    auto *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            auto obj = QJsonDocument::fromJson(reply->readAll()).object();
            for (const auto &m : kCrypto) {
                auto entry = obj.value(QString::fromLatin1(m.coingeckoId)).toObject();
                double usdPrice = entry.value("usd").toDouble(0);
                if (usdPrice > 0) {
                    // rate = units of coin per 1 USD
                    m_store.rates[m.code] = 1.0 / usdPrice;
                }
            }
        } else {
            qDebug() << "coingecko error:" << reply->errorString();
        }
        finishRefresh();
    });
}

void QtRateProvider::finishRefresh() {
    m_store.fetchedAt = QDateTime::currentSecsSinceEpoch();
    m_store.fromCache = false;
    m_refreshing = false;
    saveCache();
    if (onRatesChanged) onRatesChanged();
}
