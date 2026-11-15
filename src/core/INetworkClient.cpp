#include "INetworkClient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace EasePass::Core {

class QtNetworkClient::Impl {
public:
    QNetworkAccessManager manager;
};

QtNetworkClient::QtNetworkClient(QObject *parent) : INetworkClient(parent), m_impl(std::make_unique<Impl>()) {}

QtNetworkClient::~QtNetworkClient() = default;

void QtNetworkClient::get(const QNetworkRequest &request, ResponseCallback callback) {
    QNetworkReply *reply = m_impl->manager.get(request);
    const qint64 maxBytes = request.attribute(MaxResponseBytesAttribute).toLongLong();
    if (maxBytes > 0) {
        connect(reply, &QNetworkReply::downloadProgress, reply, [reply, maxBytes](qint64 received, qint64 total) {
            if (received > maxBytes || total > maxBytes) {
                reply->setProperty("easepassTooLarge", true);
                reply->abort();
            }
        });
    }
    connect(reply, &QNetworkReply::finished, this, [reply, callback = std::move(callback)]() {
        reply->deleteLater();
        NetworkResponse resp;
        resp.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        resp.data = reply->readAll();
        if (reply->property("easepassTooLarge").toBool()) {
            resp.data.clear();
            resp.errorString = QStringLiteral("response too large");
        } else if (reply->error() != QNetworkReply::NoError) {
            resp.errorString = reply->errorString();
        }
        if (callback) {
            callback(resp);
        }
    });
}

} // namespace EasePass::Core
