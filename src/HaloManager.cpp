#include "HaloManager.h"
#include <QList>
#include <QFile>
#include <QString>
#include <cstdio>

inline QList<QBluetoothUuid> uuidsFromFile(const QString& fn)
{
    QFile file(fn);
    if (!file.open(QFile::ReadOnly)) {
        return {};
    }
    const auto preuuids = file.readAll().split('\n');
    file.close();
    QList<QBluetoothUuid> uuids;
    uuids.reserve(preuuids.size());
    for (const auto& pre : preuuids) {
        if (!pre.isEmpty()) {
            uuids.append(QBluetoothUuid(QUuid::fromString(pre.trimmed())));
        }
    }
    return uuids;
}

HaloManager::HaloManager(Options&& options, QObject* parent)
    : QObject(parent), mOptions(std::move(options))
{
    mBluetooth = new HaloBluetooth(locationsFromFile(mOptions.locations), uuidsFromFile(mOptions.devices), this);
    QObject::connect(mBluetooth, &HaloBluetooth::ready, this, &HaloManager::bluetoothReady);
    QObject::connect(mBluetooth, &HaloBluetooth::error, this, &HaloManager::bluetoothError);
    QObject::connect(mBluetooth, &HaloBluetooth::devicesReady, this, &HaloManager::devicesReady);
    mBluetooth->initialize();

    mMqtt = new HaloMqtt(mOptions);
    QObject::connect(mMqtt, &HaloMqtt::stateRequested, this, &HaloManager::mqttStateRequested);
    mMqtt->connect();
}

HaloManager::~HaloManager()
{
    delete mBluetooth;
}

void HaloManager::bluetoothReady()
{
    mBluetooth->startDiscovery();
}

void HaloManager::bluetoothError(HaloBluetooth::Error error)
{
    fprintf(stderr, "bluetooth error 0x%x", static_cast<uint32_t>(error));
}

void HaloManager::devicesReady()
{
    qDebug() << "devices are ready";
    const auto location = mBluetooth->firstLocation();
    for (const auto& dev : location->devices) {
        // mBluetooth->setBrightness(dev.did, 200);
        // mBluetooth->setColorTemperature(dev.did, 5000);
        mMqtt->publishDevice(dev);
        mMqtt->publishDeviceState(dev.did, 200, 5000);
    }
}

void HaloManager::mqttStateRequested(uint8_t deviceId, std::optional<uint8_t> brightness, std::optional<uint32_t> temperature)
{
    if (brightness.has_value()) {
        mBluetooth->setBrightness(deviceId, brightness.value());
    }
    if (temperature.has_value()) {
        mBluetooth->setColorTemperature(deviceId, temperature.value());
    }
}

#include "moc_HaloManager.cpp"
