#pragma once

#include <QList>
#include <QString>
#include <cstdint>

struct Device
{
    uint32_t did = 0;
    QString mac = {};
    QString name = {};
    QString pid = {};
};

struct Location
{
    uint32_t id = 0;
    QString name = {};
    QString passphrase = {};
    QList<Device> devices = {};
};

using Locations = QList<Location>;

Locations locationsFromFile(const QString& file);
