#pragma once

#include <QString>
#include <cstdint>

struct Options
{
    QString locations;
    QString devices;
    QString mqttUser;
    QString mqttPassword;
    QString mqttHost;
    uint16_t mqttPort;
    uint32_t deviceDelay;
};
