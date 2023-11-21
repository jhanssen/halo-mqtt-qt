#include "Locations.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QFile>

Locations locationsFromFile(const QString& file)
{
    QFile qfile(file);
    if (!qfile.open(QFile::ReadOnly)) {
        return {};
    }
    const auto doc = QJsonDocument::fromJson(qfile.readAll());
    qfile.close();
    if (doc.isArray()) {
        Locations locations;
        const auto jsonLocations = doc.array();
        for (const auto& jsonLocation : jsonLocations) {
            if (jsonLocation.isObject()) {
                Location location = {};
                const auto& jsonLocationObject = jsonLocation.toObject();
                for (auto jsonLocationIt = jsonLocationObject.begin(), jsonLocationEnd = jsonLocationObject.end();
                     jsonLocationIt != jsonLocationEnd; ++jsonLocationIt) {
                    const auto& jsonLocationKey = jsonLocationIt.key();
                    const auto& jsonLocationValue = jsonLocationIt.value();
                    if (jsonLocationKey == "id" && jsonLocationValue.isDouble()) {
                        location.id = static_cast<uint32_t>(jsonLocationValue.toInteger());
                    } else if (jsonLocationKey == "name" && jsonLocationValue.isString()) {
                        location.name = jsonLocationValue.toString();
                    } else if (jsonLocationKey == "passphrase" && jsonLocationValue.isString()) {
                        location.passphrase = jsonLocationValue.toString();
                    } else if (jsonLocationKey == "devices" && jsonLocationValue.isArray()) {
                        const auto jsonDevices = jsonLocationValue.toArray();
                        for (const auto& jsonDevice : jsonDevices) {
                            if (jsonDevice.isObject()) {
                                Device device = {};
                                const auto& jsonDeviceObject = jsonDevice.toObject();
                                for (auto jsonDeviceIt = jsonDeviceObject.begin(), jsonDeviceEnd = jsonDeviceObject.end();
                                     jsonDeviceIt != jsonDeviceEnd; ++jsonDeviceIt) {
                                    const auto& jsonDeviceKey = jsonDeviceIt.key();
                                    const auto& jsonDeviceValue = jsonDeviceIt.value();
                                    if (jsonDeviceKey == "did" && jsonDeviceValue.isDouble()) {
                                        device.did = static_cast<uint32_t>(jsonDeviceValue.toInteger());
                                    } else if (jsonDeviceKey == "mac" && jsonDeviceValue.isString()) {
                                        device.mac = jsonDeviceValue.toString();
                                    } else if (jsonDeviceKey == "name" && jsonDeviceValue.isString()) {
                                        device.name = jsonDeviceValue.toString();
                                    } else if (jsonDeviceKey == "pid" && jsonDeviceValue.isString()) {
                                        device.pid = jsonDeviceValue.toString();
                                    }
                                }
                                location.devices.append(std::move(device));
                            }
                        }
                    }
                }
                locations.append(std::move(location));
            }
        }
        return locations;
    }
    return {};
}
