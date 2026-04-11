#include "githubsync.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QClipboard>
#include <QDebug>

GitHubSync::GitHubSync(QObject *parent)
    : QObject(parent)
    , m_settings("flick", "flick")
{
    m_token = m_settings.value("github/token").toString();
    m_username = m_settings.value("github/username").toString();

    connect(&m_pollTimer, &QTimer::timeout, this, &GitHubSync::pollForToken);

    if (!m_token.isEmpty() && !m_username.isEmpty()) {
        m_repoReady = false;
        setStatus("connected");
        emit authenticatedChanged();
    }
}

QString GitHubSync::status() const { return m_status; }
bool GitHubSync::authenticated() const { return !m_token.isEmpty(); }
QString GitHubSync::userCode() const { return m_userCode; }
QString GitHubSync::verificationUrl() const { return m_verificationUrl; }
QString GitHubSync::errorMessage() const { return m_errorMessage; }

void GitHubSync::setStatus(const QString &s)
{
    if (m_status == s) return;
    m_status = s;
    emit statusChanged();
}

void GitHubSync::setError(const QString &msg)
{
    m_errorMessage = msg;
    emit errorMessageChanged();
    setStatus("error");
}

// --- Auth ---

void GitHubSync::startAuth()
{
    if (!m_token.isEmpty()) return;
    setStatus("authenticating");

    QNetworkRequest req(QUrl("https://github.com/login/device/code"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setRawHeader("Accept", "application/json");

    QUrlQuery body;
    body.addQueryItem("client_id", CLIENT_ID);
    body.addQueryItem("scope", "repo");

    auto *reply = m_net.post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setError("Failed to start auth: " + reply->errorString());
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto obj = doc.object();

        m_deviceCode = obj["device_code"].toString();
        m_userCode = obj["user_code"].toString();
        m_verificationUrl = obj["verification_uri"].toString();
        m_pollInterval = obj["interval"].toInt(5);

        emit userCodeChanged();
        emit verificationUrlChanged();

        QDesktopServices::openUrl(QUrl(m_verificationUrl));

        m_pollTimer.start(m_pollInterval * 1000);
    });
}

void GitHubSync::cancelAuth()
{
    m_pollTimer.stop();
    m_deviceCode.clear();
    m_userCode.clear();
    m_verificationUrl.clear();
    emit userCodeChanged();
    emit verificationUrlChanged();
    setStatus("disconnected");
}

void GitHubSync::logout()
{
    m_pollTimer.stop();
    m_token.clear();
    m_username.clear();
    m_repoReady = false;
    m_settings.remove("github/token");
    m_settings.remove("github/username");
    emit authenticatedChanged();
    setStatus("disconnected");
}

void GitHubSync::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void GitHubSync::pollForToken()
{
    QNetworkRequest req(QUrl("https://github.com/login/oauth/access_token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setRawHeader("Accept", "application/json");

    QUrlQuery body;
    body.addQueryItem("client_id", CLIENT_ID);
    body.addQueryItem("device_code", m_deviceCode);
    body.addQueryItem("grant_type", "urn:ietf:params:oauth:grant-type:device_code");

    auto *reply = m_net.post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        auto data = reply->readAll();
        qDebug() << "Poll response:" << reply->error() << reply->errorString() << data;
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Poll network error:" << reply->error() << reply->errorString();
            return;
        }
        auto doc = QJsonDocument::fromJson(data);
        auto obj = doc.object();

        if (obj.contains("access_token")) {
            m_pollTimer.stop();
            onTokenReceived(obj["access_token"].toString());
        } else {
            QString error = obj["error"].toString();
            qDebug() << "Poll status:" << error;
            if (error == "slow_down") {
                m_pollInterval += 5;
                m_pollTimer.setInterval(m_pollInterval * 1000);
            } else if (error != "authorization_pending") {
                m_pollTimer.stop();
                setError("Auth failed: " + error);
            }
        }
    });
}

void GitHubSync::onTokenReceived(const QString &token)
{
    m_token = token;
    m_settings.setValue("github/token", m_token);
    m_userCode.clear();
    m_verificationUrl.clear();
    emit userCodeChanged();
    emit verificationUrlChanged();
    emit authenticatedChanged();

    fetchUsername();
}

void GitHubSync::fetchUsername()
{
    QNetworkRequest req(QUrl("https://api.github.com/user"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setRawHeader("Accept", "application/json");

    auto *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        qDebug() << "fetchUsername response:" << reply->error() << reply->errorString();
        if (reply->error() != QNetworkReply::NoError) {
            setError("Failed to get user info");
            return;
        }
        auto obj = QJsonDocument::fromJson(reply->readAll()).object();
        m_username = obj["login"].toString();
        qDebug() << "Logged in as:" << m_username;
        m_settings.setValue("github/username", m_username);
        setStatus("connected");
    });
}

// --- Repo ---

void GitHubSync::ensureRepo()
{
    if (m_repoReady) {
        doSync();
        return;
    }

    QNetworkRequest req(QUrl("https://api.github.com/repos/" + m_username + "/flick-notes"));
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setRawHeader("Accept", "application/json");

    auto *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (status == 200) {
            cloneOrPull();
        } else if (status == 404) {
            // Create repo
            QNetworkRequest createReq(QUrl("https://api.github.com/user/repos"));
            createReq.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
            createReq.setRawHeader("Accept", "application/json");
            createReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

            QJsonObject body;
            body["name"] = "flick-notes";
            body["private"] = true;
            body["description"] = "Flick notes backup";
            body["auto_init"] = true;

            auto *createReply = m_net.post(createReq, QJsonDocument(body).toJson());
            connect(createReply, &QNetworkReply::finished, this, [this, createReply]() {
                createReply->deleteLater();
                int code = createReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (code == 201) {
                    cloneOrPull();
                } else {
                    setError("Failed to create repo");
                }
            });
        } else {
            setError("Failed to check repo");
        }
    });
}

void GitHubSync::cloneOrPull()
{
    QDir repoDir(repoPath());
    if (repoDir.exists(".git")) {
        // Set remote URL (in case token changed) then pull
        runGit({"remote", "set-url", "origin", remoteUrl()}, [this](int, const QString &) {
            runGit({"pull", "--rebase", "--autostash"}, [this](int code, const QString &output) {
                Q_UNUSED(output);
                if (code != 0 && code != 1) {
                    setError("Git pull failed");
                    return;
                }
                m_repoReady = true;
                doSync();
            });
        });
    } else {
        // Clone
        repoDir.mkpath(".");
        QStringList args = {"clone", remoteUrl(), repoPath()};
        QProcess *proc = new QProcess(this);
        proc->setWorkingDirectory(QDir::tempPath());
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc](int code, QProcess::ExitStatus) {
            proc->deleteLater();
            if (code != 0) {
                setError("Git clone failed");
                return;
            }
            m_repoReady = true;
            restoreFromRepo();
            doSync();
        });
        proc->start("git", args);
    }
}

// --- Sync ---

void GitHubSync::sync()
{
    if (m_token.isEmpty() || m_username.isEmpty()) return;
    if (m_status == "syncing" || m_status == "authenticating") {
        m_syncPending = true;
        return;
    }

    setStatus("syncing");
    ensureRepo();
}

void GitHubSync::restoreFromRepo()
{
    QDir repo(repoPath());
    QDir notes(notesPath());
    QStringList repoFiles = repo.entryList({"*.txt"}, QDir::Files, QDir::Name);
    if (repoFiles.isEmpty())
        return;

    // Check if local notes are just a single empty note (fresh install)
    QStringList localFiles = notes.entryList({"*.txt"}, QDir::Files);
    bool localIsEmpty = localFiles.isEmpty();
    if (!localIsEmpty && localFiles.size() == 1) {
        QFile f(notes.filePath(localFiles.first()));
        if (f.open(QIODevice::ReadOnly)) {
            localIsEmpty = f.readAll().trimmed().isEmpty();
        }
    }

    if (!localIsEmpty)
        return; // don't overwrite existing notes

    // Copy repo notes into local notes directory
    for (const auto &f : localFiles)
        notes.remove(f);
    for (const auto &f : repoFiles)
        QFile::copy(repo.filePath(f), notes.filePath(f));

    emit notesRestored();
}

void GitHubSync::syncOnQuit()
{
    if (m_token.isEmpty() || m_username.isEmpty() || !m_repoReady)
        return;

    QDir notes(notesPath());
    QDir repo(repoPath());

    for (const auto &f : repo.entryList({"*.txt"}, QDir::Files))
        repo.remove(f);
    for (const auto &f : notes.entryList({"*.txt"}, QDir::Files))
        QFile::copy(notes.filePath(f), repo.filePath(f));

    QProcess git;
    git.setWorkingDirectory(repoPath());

    git.start("git", {"add", "-A"});
    git.waitForFinished(5000);

    git.start("git", {"commit", "-m", "sync"});
    git.waitForFinished(5000);
    if (git.exitCode() != 0)
        return; // nothing to commit

    git.start("git", {"push"});
    git.waitForFinished(10000);
}

void GitHubSync::doSync()
{
    // Copy notes into repo
    QDir notes(notesPath());
    QDir repo(repoPath());

    // Remove old note files from repo
    for (const auto &f : repo.entryList({"*.txt"}, QDir::Files))
        repo.remove(f);

    // Copy current notes
    for (const auto &f : notes.entryList({"*.txt"}, QDir::Files)) {
        QFile::copy(notes.filePath(f), repo.filePath(f));
    }

    // Git add, commit, push
    runGit({"add", "-A"}, [this](int, const QString &) {
        runGit({"commit", "-m", "sync"}, [this](int code, const QString &) {
            if (code != 0) {
                // Nothing to commit — that's fine
                setStatus("connected");
                if (m_syncPending) {
                    m_syncPending = false;
                    sync();
                }
                return;
            }
            runGit({"push"}, [this](int code, const QString &) {
                if (code != 0) {
                    setError("Push failed");
                    return;
                }
                setStatus("connected");
                if (m_syncPending) {
                    m_syncPending = false;
                    sync();
                }
            });
        });
    });
}

void GitHubSync::runGit(const QStringList &args, std::function<void(int, const QString &)> callback)
{
    QProcess *proc = new QProcess(this);
    proc->setWorkingDirectory(repoPath());
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [proc, callback](int code, QProcess::ExitStatus) {
        QString output = proc->readAllStandardOutput() + proc->readAllStandardError();
        proc->deleteLater();
        if (callback) callback(code, output);
    });
    proc->start("git", args);
}

QString GitHubSync::repoPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/flick/repo";
}

QString GitHubSync::notesPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/flick";
}

QString GitHubSync::remoteUrl() const
{
    return "https://x-access-token:" + m_token + "@github.com/" + m_username + "/flick-notes.git";
}
