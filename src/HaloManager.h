#pragma once

#include "Options.h"
//#include "MqttClient.h"
#include "HaloBluetooth.h"
#include <QObject>

class HaloManager : public QObject
{
    Q_OBJECT
public:
    HaloManager(Options&& options, QObject* parent = nullptr);
    ~HaloManager();

private slots:
    void bluetoothReady();
    void bluetoothError(HaloBluetooth::Error error);
    void devicesReady();

private:
    Options mOptions;
    HaloBluetooth* mBluetooth = nullptr;
};
