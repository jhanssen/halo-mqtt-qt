#include "HaloMqtt.h"
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

HaloMqtt::HaloMqtt(Options&& options, QObject* parent)
    : QObject(parent), mOptions(std::move(options))
{
    mBluetooth = new HaloBluetooth(locationsFromFile(mOptions.locations), uuidsFromFile(mOptions.devices), this);
    QObject::connect(mBluetooth, &HaloBluetooth::ready, this, &HaloMqtt::bluetoothReady);
    QObject::connect(mBluetooth, &HaloBluetooth::error, this, &HaloMqtt::bluetoothError);
    QObject::connect(mBluetooth, &HaloBluetooth::deviceReady, this, &HaloMqtt::deviceReady);
    mBluetooth->initialize();
}

HaloMqtt::~HaloMqtt()
{
    delete mBluetooth;
}

void HaloMqtt::bluetoothReady()
{
    mBluetooth->startDiscovery();
}

void HaloMqtt::bluetoothError(HaloBluetooth::Error error)
{
    fprintf(stderr, "bluetooth error 0x%x", static_cast<uint32_t>(error));
}

void HaloMqtt::deviceReady(const QBluetoothDeviceInfo& info)
{
    qDebug() << "device is ready" << info.deviceUuid();
    mBluetooth->setBrightness(0, 0);
}

#include "moc_HaloMqtt.cpp"
