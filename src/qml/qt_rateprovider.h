#pragma once

#include "../core/core_rateprovider.h"

#include <QObject>
#include <QNetworkAccessManager>

class QtRateProvider : public QObject, public flick::RateProvider {
    Q_OBJECT
public:
    explicit QtRateProvider(QObject *parent = nullptr);
    ~QtRateProvider() override;

    // flick::RateProvider
    const flick::RateStore &store() const override { return m_store; }
    void requestRefresh() override;
    void setOverrides(std::unordered_map<std::string, double> overrides) override;
    void setUpdateDaily(bool enabled) override;

private:
    void loadCache();
    void saveCache();

    void fetchFiatPrimary();     // frankfurter.app
    void fetchFiatFallback();    // open.er-api.com
    void fetchCrypto();          // coingecko
    void finishRefresh();

    QNetworkAccessManager m_net;
    flick::RateStore m_store;
    bool m_updateDaily = true;
    bool m_refreshing = false;
};
