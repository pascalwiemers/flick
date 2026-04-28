#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTimer>
#include <QProcess>

class GitHubSync : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticatedChanged)
    Q_PROPERTY(QString userCode READ userCode NOTIFY userCodeChanged)
    Q_PROPERTY(QString verificationUrl READ verificationUrl NOTIFY verificationUrlChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit GitHubSync(QObject *parent = nullptr);

    QString status() const;
    bool authenticated() const;
    QString userCode() const;
    QString verificationUrl() const;
    QString errorMessage() const;

    Q_INVOKABLE void startAuth();
    Q_INVOKABLE void cancelAuth();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void sync();
    Q_INVOKABLE void syncOnQuit();
    Q_INVOKABLE void copyToClipboard(const QString &text);

signals:
    void statusChanged();
    void authenticatedChanged();
    void userCodeChanged();
    void verificationUrlChanged();
    void errorMessageChanged();
    void notesRestored();

private:
    void setStatus(const QString &s);
    void setError(const QString &msg);
    void pollForToken();
    void onTokenReceived(const QString &token);
    void fetchUsername();
    void ensureRepo();
    void cloneOrPull();
    void restoreFromRepo();
    void restoreTrashFromRepo();
    void doSync();
    void copyTrashToRepo();
    void runGit(const QStringList &args, std::function<void(int, const QString &)> callback);
    QString repoPath() const;
    QString notesPath() const;
    QString remoteUrl() const;

    static constexpr const char *CLIENT_ID = "Ov23li5Q9HZMR3fdyzsp";

    QNetworkAccessManager m_net;
    QSettings m_settings;
    QTimer m_pollTimer;
    QString m_status = "disconnected";
    QString m_token;
    QString m_username;
    QString m_deviceCode;
    QString m_userCode;
    QString m_verificationUrl;
    QString m_errorMessage;
    int m_pollInterval = 5;
    bool m_repoReady = false;
    bool m_syncPending = false;
};
