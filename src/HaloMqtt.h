#pragma once

#include "Options.h"
#include "Locations.h"
#include <QObject>
#include <QMqttClient>
#include <QMqttMessage>
#include <QMqttSubscription>
#include <QList>
#include <QString>
#include <cstdint>
#include <optional>

class HaloMqtt : public QObject
{
    Q_OBJECT
public:
    HaloMqtt(const Options& options, QObject* parent = nullptr);
    ~HaloMqtt();

    void connect();
    bool isConnected() const { return mConnected; }

    void publishDevice(uint32_t locationId, const Device& device);
    void unpublishDevice(uint32_t locationId, uint8_t deviceId);
    void publishDeviceState(uint32_t locationId, uint8_t deviceId, uint8_t brightness, uint32_t temperature);

signals:
    void idle();
    void stateRequested(uint32_t locationId, uint8_t deviceId, std::optional<uint8_t> brightness, std::optional<uint32_t> temperature);
    void connected();

private slots:
    void mqttConnected();
    void mqttDisconnected();
    void mqttMessageReceived(const QMqttMessage& message);
    void mqttErrorChanged(QMqttClient::ClientError error);
    void mqttMessageSent(qint32 id);
    void reconnectNow();

private:
    void recreateClient();
    void sendPendingPublishes();

private:
    struct DeviceInfo
    {
        uint8_t brightness = 0;
        uint32_t colorTemp = 0;
    };

    Options mOptions;
    QMqttClient* mClient = nullptr;
    QMqttSubscription* mSubscription = nullptr;
    QList<DeviceInfo> mInfos;
    QList<std::pair<QString, QByteArray>> mPendingPublish;
    QList<qint32> mPendingSends;
    bool mConnected = false;
    uint32_t mConnectBackoff = 0;
};
