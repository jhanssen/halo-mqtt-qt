#include "HaloMqtt.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>

static const char* commandTopic = "halomqtt/light/command";
static const char* stateTopic = "halomqtt/light/state";
static const char* devicePrefix = "halomqtt_";

HaloMqtt::HaloMqtt(const Options& options, QObject* parent)
    : QObject(parent)
{
    mClient = new QMqttClient(this);
    mClient->setHostname(options.mqttHost);
    mClient->setUsername(options.mqttUser);
    mClient->setPassword(options.mqttPassword);
    mClient->setPort(1883);
    QObject::connect(mClient, &QMqttClient::connected, this, &HaloMqtt::mqttConnected);
    QObject::connect(mClient, &QMqttClient::disconnected, this, &HaloMqtt::mqttDisconnected);
    QObject::connect(mClient, &QMqttClient::errorChanged, this, &HaloMqtt::mqttErrorChanged);
}

HaloMqtt::~HaloMqtt()
{
    delete mClient;
}

void HaloMqtt::connect()
{
    mClient->connectToHost();
}

void HaloMqtt::publishDevice(const Device& device)
{
    if (device.did >= mInfos.size()) {
        mInfos.resize(device.did + 1);
    }

    const QByteArray baDeviceId = devicePrefix + QByteArray::number(device.did);
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

    mClient->publish(QString::fromUtf8(deviceTopic), discovery, 0, true);
}

void HaloMqtt::unpublishDevice(uint8_t deviceId)
{
    const QByteArray baDeviceId = devicePrefix + QByteArray::number(deviceId);
    const QByteArray deviceTopic = "homeassistant/light/" + baDeviceId + "/config";
    if (!mConnected) {
        mPendingPublish.append(std::make_pair(QString::fromUtf8(deviceTopic), QByteArray()));
        qDebug() << "not connected";
        return;
    }

    mClient->publish(QString::fromUtf8(deviceTopic), QByteArray(), 0, true);
}

void HaloMqtt::publishDeviceState(uint8_t deviceId, uint8_t brightness, uint32_t temperature)
{
    const QByteArray baDeviceId = devicePrefix + QByteArray::number(deviceId);
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

    mClient->publish(QString::fromUtf8(deviceTopic), state, 0, true);
}

void HaloMqtt::mqttConnected()
{
    qDebug() << "mqtt connected";

    mConnected = true;
    mSubscription = mClient->subscribe(QLatin1String(commandTopic) + "/+");
    QObject::connect(mSubscription, &QMqttSubscription::messageReceived, this, &HaloMqtt::mqttMessageReceived);
}

void HaloMqtt::mqttDisconnected()
{
    if (mSubscription) {
        QObject::disconnect(mSubscription, &QMqttSubscription::messageReceived, this, &HaloMqtt::mqttMessageReceived);
    }

    mConnected = false;
    mSubscription = nullptr;

    qDebug() << "mqtt disconnected";

    mConnectBackoff = 0;
    reconnect();
}

void HaloMqtt::sendPendingPublishes()
{
    for (const auto& to : mPendingPublish) {
        mClient->publish(to.first, to.second, 0, true);
    }
    mPendingPublish.clear();
}

void HaloMqtt::reconnect()
{
    if (mPendingConnect) {
        return;
    }
    mPendingConnect = true;
    mClient->connectToHost();
}

void HaloMqtt::mqttErrorChanged(QMqttClient::ClientError error)
{
    qDebug() << "mqtt error" << error;
    // try to reconnect?
    if (mPendingConnect) {
        mPendingConnect = false;
        // increase the backoff
        mConnectBackoff = std::min<uint32_t>(10000, mConnectBackoff ? (mConnectBackoff * mConnectBackoff) : 100);
        QTimer::singleShot(mConnectBackoff, this, &HaloMqtt::reconnect);
    }
}

void HaloMqtt::mqttMessageReceived(const QMqttMessage& message)
{
    auto doc = QJsonDocument::fromJson(message.payload());
    if (doc.isObject()) {
        // parse device id from topic name
        static const QByteArray baDeviceTopic = QByteArray(commandTopic) + "/" + devicePrefix;
        const auto topic = message.topic().name().toUtf8();
        if (!topic.startsWith(baDeviceTopic)) {
            // not for us
            return;
        }
        bool ok;
        const auto deviceId = QByteArrayView(topic.constData() + baDeviceTopic.size(), topic.constData() + topic.size()).toInt(&ok);
        if (!ok) {
            // failed to parse
            return;
        }
        if (deviceId < 0 || deviceId > 255 || deviceId >= mInfos.size()) {
            qDebug() << "unknown device" << deviceId;
        }

        qDebug() << "device" << deviceId;

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
        qDebug() << "mqtt message" << state.value_or(false) << brightness.value_or(0) << colorTemp.value_or(0);
        qDebug() << "mqtt message" << doc.toJson();

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

        emit stateRequested(static_cast<uint8_t>(deviceId), brightness, colorTemp);
    }
}

#include "moc_HaloMqtt.cpp"
