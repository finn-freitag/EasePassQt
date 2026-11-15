#pragma once

#include <QObject>
#include <QNetworkRequest>
#include <QByteArray>
#include <QString>
#include <functional>
#include <memory>

namespace EasePass::Core {

struct NetworkResponse {
    int statusCode = 0;
    QByteArray data;
    QString errorString;
    bool isSuccess() const { return statusCode == 200 && errorString.isEmpty(); }
};

class INetworkClient : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~INetworkClient() = default;

    /// Optional request attribute (qint64): the transfer is aborted with an error as soon as
    /// the response is larger than this many bytes. Not set or <= 0: no limit.
    static constexpr QNetworkRequest::Attribute MaxResponseBytesAttribute =
        QNetworkRequest::Attribute(int(QNetworkRequest::User) + 1);

    using ResponseCallback = std::function<void(const NetworkResponse &response)>;
    virtual void get(const QNetworkRequest &request, ResponseCallback callback) = 0;
};

class QtNetworkClient : public INetworkClient {
    Q_OBJECT
public:
    explicit QtNetworkClient(QObject *parent = nullptr);
    ~QtNetworkClient() override;

    void get(const QNetworkRequest &request, ResponseCallback callback) override;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace EasePass::Core
