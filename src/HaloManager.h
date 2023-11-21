#pragma once

#include "Options.h"
#include "HaloMqtt.h"
#include "HaloBluetooth.h"
#include <QObject>
#include <cstdint>

class HaloManager : public QObject
{
    Q_OBJECT
public:
    HaloManager(Options&& options, QObject* parent = nullptr);
    ~HaloManager();

    void quit();

private slots:
    void bluetoothReady();
    void bluetoothError(HaloBluetooth::Error error);
    void devicesReady();
    void mqttStateRequested(uint32_t locationId, uint8_t deviceId, std::optional<uint8_t> brightness, std::optional<uint32_t> temperature);
    void mqttIdle();

private:
    Options mOptions;
    HaloBluetooth* mBluetooth = nullptr;
    HaloMqtt* mMqtt = nullptr;
    bool mQuitting = false;
};
