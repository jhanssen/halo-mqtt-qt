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

    void publishDevice(const Device& device);
    void unpublishDevice(uint8_t deviceId);

    void publishDeviceState(uint8_t deviceId, uint8_t brightness, uint32_t temperature);

signals:
    void stateRequested(uint8_t deviceId, std::optional<uint8_t> brightness, std::optional<uint32_t> temperature);

private slots:
    void mqttConnected();
    void mqttDisconnected();
    void mqttMessageReceived(const QMqttMessage& message);
    void mqttErrorChanged(QMqttClient::ClientError error);
    void reconnect();

private:
    void sendPendingPublishes();

private:
    struct DeviceInfo
    {
        uint8_t brightness = 0;
        uint32_t colorTemp = 0;
    };

    QMqttClient* mClient;
    QMqttSubscription* mSubscription = nullptr;
    QList<DeviceInfo> mInfos;
    QList<std::pair<QString, QByteArray>> mPendingPublish;
    bool mConnected = false;
    bool mPendingConnect = false;
    uint32_t mConnectBackoff = 0;
};
