#include "HaloMqtt.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>

static const char* commandTopic = "halomqtt/light/command";
static const char* stateTopic = "halomqtt/light/state";
static const char* devicePrefix = "halomqtt_";

HaloMqtt::HaloMqtt(const Options& options, QObject* parent)
    : QObject(parent), mOptions(options)
{
    recreateClient();
}

HaloMqtt::~HaloMqtt()
{
    delete mClient;
}

void HaloMqtt::recreateClient()
{
    if (mClient) {
        mClient->deleteLater();
    }
    mClient = new QMqttClient(this);
    mClient->setHostname(mOptions.mqttHost);
    mClient->setUsername(mOptions.mqttUser);
    mClient->setPassword(mOptions.mqttPassword);
    mClient->setPort(mOptions.mqttPort);
    QObject::connect(mClient, &QMqttClient::connected, this, &HaloMqtt::mqttConnected);
    QObject::connect(mClient, &QMqttClient::disconnected, this, &HaloMqtt::mqttDisconnected);
    QObject::connect(mClient, &QMqttClient::errorChanged, this, &HaloMqtt::mqttErrorChanged);
    QObject::connect(mClient, &QMqttClient::messageSent, this, &HaloMqtt::mqttMessageSent);
}

void HaloMqtt::connect()
{
    mClient->connectToHost();
}

void HaloMqtt::publishDevice(uint32_t locationId, const Device& device)
{
    if (device.did >= mInfos.size()) {
        mInfos.resize(device.did + 1);
    }

    const QByteArray baDeviceId = devicePrefix + QByteArray::number(locationId) + '_' + QByteArray::number(device.did);
    const QByteArray discovery =
    "{\"name\":\"" + device.name.toUtf8() + "\","
    "\"command_topic\":\"" + QByteArray(commandTopic) + "/" + baDeviceId + "\","
    "\"state_topic\":\"" + QByteArray(stateTopic) + "/" + baDeviceId + "\","
    "\"object_id\":\"" + baDeviceId + "\","
    "\"unique_id\":\"" + baDeviceId + "\","
    "\"brightness\":true,"
    "\"color_mode\":true,"
    "\"supported_color_modes\":[\"color_temp\"],"
    "\"max_mireds\":370,"
    "\"min_mireds\":200,"
    "\"schema\":\"json\"}";

    const QByteArray deviceTopic = "homeassistant/light/" + baDeviceId + "/config";
    if (!mConnected) {
        qDebug() << "not connected";
        mPendingPublish.append(std::make_pair(QString::fromUtf8(deviceTopic), discovery));
        return;
    }

    auto id = mClient->publish(QString::fromUtf8(deviceTopic), discovery, 1, true);
    mPendingSends.append(id);
}

void HaloMqtt::unpublishDevice(uint32_t locationId, uint8_t deviceId)
{
    const QByteArray baDeviceId = devicePrefix + QByteArray::number(locationId) + '_' + QByteArray::number(deviceId);
    const QByteArray deviceTopic = "homeassistant/light/" + baDeviceId + "/config";
    if (!mConnected) {
        mPendingPublish.append(std::make_pair(QString::fromUtf8(deviceTopic), QByteArray()));
        qDebug() << "not connected";
        return;
    }

    auto id = mClient->publish(QString::fromUtf8(deviceTopic), QByteArray(), 1, true);
    mPendingSends.append(id);
}

void HaloMqtt::publishDeviceState(uint32_t locationId, uint8_t deviceId, uint8_t brightness, uint32_t temperature)
{
    const QByteArray baDeviceId = devicePrefix + QByteArray::number(locationId) + '_' + QByteArray::number(deviceId);
    const QByteArray state =
    "{\"state\":\"" + QByteArray(brightness > 0 ? "ON" : "OFF") + "\","
    "\"color_temp\":" + QByteArray::number(static_cast<uint32_t>(1000000.f / temperature)) + ","
    "\"brightness\":" + QByteArray::number(brightness) + ","
    "\"color_mode\":\"color_temp\"}";

    const QByteArray deviceTopic = QByteArray(stateTopic) + "/" + baDeviceId;

    if (deviceId >= mInfos.size()) {
        mInfos.resize(deviceId + 1);
    }
    mInfos[deviceId] = {
        brightness,
        temperature
    };

    if (!mConnected) {
        qDebug() << "not connected";
        mPendingPublish.append(std::make_pair(QString::fromUtf8(deviceTopic), state));
        return;
    }

    qDebug() << "publishing device state" << locationId << deviceId << state;

    auto id = mClient->publish(QString::fromUtf8(deviceTopic), state, 1, true);
    mPendingSends.append(id);
}

void HaloMqtt::mqttConnected()
{
    qDebug() << "mqtt connected";

    mConnected = true;
    mSubscription = mClient->subscribe(QLatin1String(commandTopic) + "/+");
    QObject::connect(mSubscription, &QMqttSubscription::messageReceived, this, &HaloMqtt::mqttMessageReceived);

    emit connected();
}

void HaloMqtt::mqttDisconnected()
{
    if (mSubscription) {
        QObject::disconnect(mSubscription, &QMqttSubscription::messageReceived, this, &HaloMqtt::mqttMessageReceived);
    }

    const bool wasConnected = mConnected;
    mConnected = false;
    mSubscription = nullptr;

    qDebug() << "mqtt disconnected";

    mPendingSends.clear();
    if (wasConnected) {
        mConnectBackoff = 0;
    }

    mConnectBackoff = std::min<uint32_t>(10000, mConnectBackoff ? (mConnectBackoff * 5) : 100);
    QTimer::singleShot(mConnectBackoff, this, &HaloMqtt::reconnectNow);
}

void HaloMqtt::sendPendingPublishes()
{
    mPendingSends.reserve(mPendingSends.size() + mPendingPublish.size());
    for (const auto& to : mPendingPublish) {
        auto id = mClient->publish(to.first, to.second, 1, true);
        mPendingSends.append(id);
    }
    mPendingPublish.clear();
}

void HaloMqtt::reconnectNow()
{
    qDebug() << "attempting to reconnect";
    recreateClient();
    mClient->connectToHost();
}

void HaloMqtt::mqttMessageSent(qint32 id)
{
    auto idx = mPendingSends.indexOf(id);
    // qDebug() << "msg sent" << mPendingSends << id << idx;
    if (idx == -1) {
        // bad
        return;
    }
    mPendingSends.remove(idx);
    if (mPendingSends.isEmpty()) {
        emit idle();
    }
}

void HaloMqtt::mqttErrorChanged(QMqttClient::ClientError error)
{
    qDebug() << "mqtt error" << error;
}

void HaloMqtt::mqttMessageReceived(const QMqttMessage& message)
{
    auto doc = QJsonDocument::fromJson(message.payload());
    if (doc.isObject()) {
        // parse location and device ids from topic name
        static const QByteArray baDeviceTopic = QByteArray(commandTopic) + "/" + devicePrefix;
        const auto topic = message.topic().name().toUtf8();
        if (!topic.startsWith(baDeviceTopic)) {
            // not for us
            return;
        }

        const auto underscore = topic.indexOf('_', baDeviceTopic.size());
        if (underscore == -1) {
            // nope
            return;
        }

        const QByteArrayView locationView(topic.constData() + baDeviceTopic.size(), topic.constData() + underscore);
        const QByteArrayView deviceView(topic.constData() + underscore + 1, topic.constData() + topic.size());

        bool ok;
        const auto locationId = locationView.toInt(&ok);
        if (!ok) {
            // nope
            return;
        }
        const auto deviceId = deviceView.toInt(&ok);
        if (!ok) {
            // nope
            return;
        }

        if (locationId < 0) {
            qDebug() << "unknown location" << locationId;
            return;
        }

        if (deviceId < 0 || deviceId > 255 || deviceId >= mInfos.size()) {
            qDebug() << "unknown device" << deviceId;
            return;
        }

        // qDebug() << "device" << locationId << deviceId;

        auto msgobj = doc.object();
        std::optional<bool> state;
        std::optional<uint8_t> brightness;
        std::optional<uint32_t> colorTemp;
        if (msgobj.contains("state")) {
            state = msgobj.value("state").toString() == "ON";
        }
        if (msgobj.contains("brightness")) {
            brightness = static_cast<uint8_t>(msgobj.value("brightness").toInt());
        }
        if (msgobj.contains("color_temp")) {
            colorTemp = static_cast<uint32_t>(1000000.f / msgobj.value("color_temp").toDouble());
        }
        qDebug() << "mqtt message" << doc.toJson() << "for" << locationId << deviceId;

        auto& info = mInfos[deviceId];
        if (state.value_or(false) && !brightness.has_value() && info.brightness == 0) {
            brightness = 255;
        } else if (state.has_value() && !state.value() && !brightness.has_value() && info.brightness > 0) {
            brightness = 0;
        }

        if (brightness.has_value()) {
            info.brightness = brightness.value();
        }
        if (colorTemp.has_value()) {
            info.colorTemp = colorTemp.value();
        }
        publishDeviceState(static_cast<uint32_t>(locationId), static_cast<uint8_t>(deviceId), info.brightness, info.colorTemp);
        emit stateRequested(static_cast<uint32_t>(locationId), static_cast<uint8_t>(deviceId), brightness, colorTemp);
    }
}

#include "moc_HaloMqtt.cpp"
