// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Common.hpp"
#include "common/network/NetworkCommon.hpp"
#include "util/DebugCount.hpp"

#include <QHttpMultiPart>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QUrl>  // satorino: log URL redaction

#include <chrono>  // satorino: explicit timeout dependency
#include <memory>
#include <optional>

class QNetworkReply;

namespace chatterino {

class NetworkResult;

class NetworkRequester : public QObject
{
    Q_OBJECT

Q_SIGNALS:
    void requestUrl();
};

class NetworkData
{
public:
    NetworkData();
    ~NetworkData();
    NetworkData(const NetworkData &) = delete;
    NetworkData(NetworkData &&) = delete;
    NetworkData &operator=(const NetworkData &) = delete;
    NetworkData &operator=(NetworkData &&) = delete;

    QNetworkRequest request;
    bool hasCaller{};
    QPointer<QObject> caller;
    bool cache{};
    bool executeConcurrently{};
    /// Hide the request body in logs
    bool hideRequestBody = false;
    // satorino: all log paths use a redacted URL for private requests.
    bool sensitive = false;
    QUrl logUrl() const
    {
        return this->sensitive ? QUrl("redacted:private-request")
                               : this->request.url();
    }

    NetworkSuccessCallback onSuccess;
    NetworkErrorCallback onError;
    NetworkFinallyCallback finally;

    NetworkRequestType requestType = NetworkRequestType::Get;

    QByteArray payload;
    std::unique_ptr<QHttpMultiPart, DeleteLater> multiPartPayload;

    std::optional<std::chrono::milliseconds> timeout{};
#ifndef NDEBUG
    bool ignoreSslErrors = false;
#endif

    QString getHash();

    void emitSuccess(NetworkResult &&result);
    void emitError(NetworkResult &&result);
    void emitFinally();

    QString typeString() const;

private:
    QString hash_;
};

void load(std::shared_ptr<NetworkData> &&data);

}  // namespace chatterino
