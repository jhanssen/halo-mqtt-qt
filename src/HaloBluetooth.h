#pragma once

#include "Locations.h"
#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QRandomGenerator>
#include <cstdint>

class HaloBluetooth : public QObject
{
    Q_OBJECT
public:
    enum class Error { PermissionError };

    HaloBluetooth(uint32_t deviceDelay, Locations&& locations, QList<QBluetoothUuid>&& approved, QObject* parent);
    ~HaloBluetooth();

    void initialize();
    void startDiscovery();

    const Locations& locations() const;
    const Location* firstLocation() const;

signals:
    void error(Error error);
    void ready();

    void devicesReady();

public slots:
    void setBrightness(uint8_t deviceId, uint8_t brightness);
    void setColorTemperature(uint8_t deviceId, uint16_t temperature);

private slots:
    void deviceDiscovered(const QBluetoothDeviceInfo& info);
    void deviceConnected();
    void deviceDisconnected();
    void deviceErrorOccurred(QLowEnergyController::Error error);
    void deviceServiceDiscovered(const QBluetoothUuid& service);
    void serviceCharacteristicChanged(const QLowEnergyCharacteristic& characteristic, const QByteArray& value);
    void serviceDescriptorWritten(const QLowEnergyDescriptor& descriptor, const QByteArray& value);
    void serviceErrorOccurred(QLowEnergyService::ServiceError error);
    void serviceStateChanged(QLowEnergyService::ServiceState state);

private slots:
    void writeNextPacket();

private:
    void writePacketInternal(const QByteArray& packet);
    void writePacket(const QByteArray& packet);
    void addDevice(const QBluetoothDeviceInfo& info);
    uint32_t randomSeq();

    struct InternalDevice
    {
        QBluetoothDeviceInfo info;
        QLowEnergyController* controller = nullptr;
        QLowEnergyService* service = nullptr;
        QLowEnergyCharacteristic low = {}, high = {};
        uint32_t connectCount = 0;
        bool connected = false, connecting = false, ready = false;
    };

    void writePendingPackets();
    void scheduleNextPacket();

private:
    uint32_t mDeviceDelay;
    Locations mLocations;
    QRandomGenerator mRandom;
    QByteArray mKey;
    QList<QBluetoothUuid> mApprovedDevices;
    QBluetoothDeviceDiscoveryAgent* mDiscoveryAgent = nullptr;
    QList<InternalDevice> mDevices;
    QList<QByteArray> mPendingPackets;
    bool mWritingPacket = false, mScheduledPacket = false;
};

inline const Locations& HaloBluetooth::locations() const
{
    return mLocations;
}

inline const Location* HaloBluetooth::firstLocation() const
{
    if (mLocations.isEmpty()) {
        return nullptr;
    }
    return &mLocations[0];
}
