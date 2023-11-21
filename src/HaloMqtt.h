#pragma once

#include "Options.h"
//#include "MqttClient.h"
#include "HaloBluetooth.h"
#include <QObject>

class HaloMqtt : public QObject
{
    Q_OBJECT
public:
    HaloMqtt(Options&& options, QObject* parent = nullptr);
    ~HaloMqtt();

private slots:
    void bluetoothReady();
    void bluetoothError(HaloBluetooth::Error error);
    void deviceReady(const QBluetoothDeviceInfo& info);

private:
    Options mOptions;
    HaloBluetooth* mBluetooth = nullptr;
};
