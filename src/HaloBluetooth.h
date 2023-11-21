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

    HaloBluetooth(Locations&& locations, QList<QBluetoothUuid>&& approved, QObject* parent);
    ~HaloBluetooth();

    void initialize();
    void startDiscovery();

signals:
    void error(Error error);
    void ready();

    void deviceReady(const QBluetoothDeviceInfo& info);

public slots:
    void setBrightness(uint8_t deviceId, uint8_t brightness);

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

private:
    void writePacket(const QByteArray& packet);
    void writePacket(const QBluetoothUuid& uuid, const QByteArray& packet);
    void addDevice(const QBluetoothDeviceInfo& info);
    uint32_t randomSeq();

    struct Device
    {
        QBluetoothDeviceInfo info;
        QLowEnergyController* controller = nullptr;
        QLowEnergyService* service = nullptr;
        QLowEnergyCharacteristic low = {}, high = {};
    };

    void writePacket(Device* device, const QByteArray& packet);

private:
    Locations mLocations;
    QRandomGenerator mRandom;
    QByteArray mKey;
    QList<QBluetoothUuid> mApprovedDevices;
    QBluetoothDeviceDiscoveryAgent* mDiscoveryAgent = nullptr;

    QList<Device> mDevices;
};
