#include "HaloBluetooth.h"
#include "Crypto.h"
#include <QCoreApplication.h>
#include <QPermissions>
#include <QTimer>
#include <QDebug>
#include <cassert>

HaloBluetooth::HaloBluetooth(uint32_t deviceDelay, Locations&& locations, QList<QBluetoothUuid>&& approved, QObject* parent)
    : QObject(parent), mDeviceDelay(deviceDelay), mLocations(std::move(locations)), mApprovedDevices(std::move(approved))
{
    qDebug() << "device delay" << mDeviceDelay;
    mDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect(mDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &HaloBluetooth::deviceDiscovered);

    if (mLocations.size() > 0) {
        // ### should improve this
        mKey = crypto::generateKey(mLocations[0].passphrase.toUtf8() + QByteArray::fromHex("004d4350"));
        // qDebug() << "key" << mKey.toHex();
    }
}

HaloBluetooth::~HaloBluetooth()
{
    delete mDiscoveryAgent;
}

void HaloBluetooth::initialize()
{
    auto app = QCoreApplication::instance();

    QBluetoothPermission bluetoothPermission;
    bluetoothPermission.setCommunicationModes(QBluetoothPermission::Access);
    switch (app->checkPermission(bluetoothPermission)) {
    case Qt::PermissionStatus::Undetermined:
    case Qt::PermissionStatus::Denied:
        // ask for permission
        app->requestPermission(bluetoothPermission, this, [this](const QPermission& permission) {
            switch (permission.status()) {
            case Qt::PermissionStatus::Denied:
                emit error(Error::PermissionError);
                break;
            case Qt::PermissionStatus::Granted:
                emit ready();
                break;
            default:
                // should never happen
                assert(false && "Impossible impossibility");
                break;
            }
        });
        break;
    case Qt::PermissionStatus::Granted:
        emit ready();
        break;
    }
}

void HaloBluetooth::rediscover()
{
    if (mDiscoveryAgent) {
        QObject::disconnect(mDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
                            this, &HaloBluetooth::deviceDiscovered);
        mDiscoveryAgent->deleteLater();
    }
    mDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    QObject::connect(mDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
                     this, &HaloBluetooth::deviceDiscovered);
    mDiscoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void HaloBluetooth::startDiscovery()
{
    // qDebug() << "discovering";
    mDiscoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void HaloBluetooth::addDevice(const QBluetoothDeviceInfo& info)
{
    auto recreateController = [this](InternalDevice& idev) {
        idev.controller = QLowEnergyController::createCentral(idev.info, this);
        QObject::connect(idev.controller, &QLowEnergyController::serviceDiscovered,
                         this, &HaloBluetooth::deviceServiceDiscovered);
        QObject::connect(idev.controller, &QLowEnergyController::errorOccurred,
                         this, &HaloBluetooth::deviceErrorOccurred);
        QObject::connect(idev.controller, &QLowEnergyController::connected,
                         this, &HaloBluetooth::deviceConnected);
        QObject::connect(idev.controller, &QLowEnergyController::disconnected,
                         this, &HaloBluetooth::deviceDisconnected);
    };

    auto dit = std::find_if(mDevices.begin(), mDevices.end(),
                            [&info](const auto& other) {
                                return info.deviceUuid() == other.info.deviceUuid();
                            });
    if (dit != mDevices.end()) {
        // already added?
        if (!dit->controller) {
            recreateController(*dit);
            dit->connecting = true;
            dit->controller->connectToDevice();
        }
        return;
    }

    auto ait = std::find(mApprovedDevices.begin(), mApprovedDevices.end(), info.deviceUuid());
    if (ait == mApprovedDevices.end()) {
        // not approved
        qDebug() << "device not approved" << info.deviceUuid();
        return;
    }

    InternalDevice dev = {
        info,
    };

    recreateController(dev);
    dev.connecting = true;
    dev.controller->connectToDevice();

    mDevices.append(std::move(dev));
}

void HaloBluetooth::deviceConnected()
{
    auto controller = static_cast<QLowEnergyController*>(sender());

    auto it = std::find_if(mDevices.begin(), mDevices.end(),
                           [controller](const auto& other) {
                               return controller == other.controller;
                           });
    if (it == mDevices.end()) {
        qDebug() << "no device for connected?";
        return;
    }

    controller->discoverServices();
    ++it->connectCount;
    it->connecting = false;
    it->connected = true;
    it->connectBackoff = 0;
}

void HaloBluetooth::deviceDisconnected()
{
    auto controller = static_cast<QLowEnergyController*>(sender());
    qDebug() << "device disconnected" << controller->remoteDeviceUuid();

    auto it = std::find_if(mDevices.begin(), mDevices.end(),
                           [controller](const auto& other) {
                               return controller == other.controller;
                           });
    if (it == mDevices.end()) {
        qDebug() << "no device for disconnected?";
        return;
    }

    it->ready = it->connecting = it->connected = false;

    QObject::disconnect(it->service, &QLowEnergyService::stateChanged, this, &HaloBluetooth::serviceStateChanged);
    QObject::disconnect(it->service, &QLowEnergyService::errorOccurred, this, &HaloBluetooth::serviceErrorOccurred);
    QObject::disconnect(it->service, &QLowEnergyService::characteristicChanged, this, &HaloBluetooth::serviceCharacteristicChanged);
    QObject::disconnect(it->service, &QLowEnergyService::descriptorWritten, this, &HaloBluetooth::serviceDescriptorWritten);
    it->service->deleteLater();
    it->service = nullptr;

    QObject::disconnect(it->controller, &QLowEnergyController::serviceDiscovered,
                        this, &HaloBluetooth::deviceServiceDiscovered);
    QObject::disconnect(it->controller, &QLowEnergyController::errorOccurred,
                        this, &HaloBluetooth::deviceErrorOccurred);
    QObject::disconnect(it->controller, &QLowEnergyController::connected,
                        this, &HaloBluetooth::deviceConnected);
    QObject::disconnect(it->controller, &QLowEnergyController::disconnected,
                        this, &HaloBluetooth::deviceDisconnected);
    it->controller->deleteLater();
    it->controller = nullptr;
}

void HaloBluetooth::deviceErrorOccurred(QLowEnergyController::Error error)
{
    auto controller = static_cast<QLowEnergyController*>(sender());
    qDebug() << "device error" << error;

    auto it = std::find_if(mDevices.begin(), mDevices.end(),
                           [controller](const auto& other) {
                               return controller == other.controller;
                           });
    if (it == mDevices.end()) {
        qDebug() << "no device for error?";
        return;
    }
    if (it->connecting && !it->connected) {
        it->connecting = false;
        // reconnect later
        it->connectBackoff = std::min<uint32_t>(30000, it->connectBackoff ? it->connectBackoff * 5 : 100);
        QTimer::singleShot(it->connectBackoff, [this, controller]() {
            auto sit = std::find_if(mDevices.begin(), mDevices.end(),
                                    [controller](const auto& other) {
                                        return controller == other.controller;
                                    });
            if (sit == mDevices.end()) {
                qDebug() << "no device for backoff reconnect?";
                return;
            }
            if (!sit->connecting && !sit->connected) {
                addDevice(sit->info);
                if (!sit->connecting) {
                    sit->connecting = true;
                    controller->connectToDevice();
                }
            }
        });
    }
}

void HaloBluetooth::deviceServiceDiscovered(const QBluetoothUuid& service)
{
    // qDebug() << "device new service" << service;
    static const QUuid aviOnService = QUuid::fromString("0000fef1-0000-1000-8000-00805f9b34fb");
    if (service.operator==(aviOnService)) {
        auto controller = static_cast<QLowEnergyController*>(sender());
        auto it = std::find_if(mDevices.begin(), mDevices.end(),
                               [controller](const auto& other) {
                                   return controller == other.controller;
                               });
        if (it == mDevices.end()) {
            qDebug() << "no device for service?";
            return;
        }

        auto serviceObject = controller->createServiceObject(service, this);
        if (serviceObject == nullptr) {
            qDebug() << "no service for avi-on service uuid";
            return;
        }

        QObject::connect(serviceObject, &QLowEnergyService::stateChanged, this, &HaloBluetooth::serviceStateChanged);
        QObject::connect(serviceObject, &QLowEnergyService::errorOccurred, this, &HaloBluetooth::serviceErrorOccurred);
        QObject::connect(serviceObject, &QLowEnergyService::characteristicChanged, this, &HaloBluetooth::serviceCharacteristicChanged);
        QObject::connect(serviceObject, &QLowEnergyService::descriptorWritten, this, &HaloBluetooth::serviceDescriptorWritten);
        serviceObject->discoverDetails();

        it->service = serviceObject;
    }
}

void HaloBluetooth::deviceDiscovered(const QBluetoothDeviceInfo& info)
{
    if (!(info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)) {
        return;
    }
    // qDebug() << "discovered" << info.deviceUuid() << info.name();
    static const QString aviOn = "Avi-on";
    if (info.name() == aviOn) {
        addDevice(info);
    }
}

void HaloBluetooth::serviceCharacteristicChanged(const QLowEnergyCharacteristic& characteristic, const QByteArray& value)
{
    // qDebug() << "service char changed" << characteristic.uuid() << characteristic.name() << value;
}

void HaloBluetooth::serviceDescriptorWritten(const QLowEnergyDescriptor& descriptor, const QByteArray& value)
{
    // qDebug() << "service descr written" << descriptor.uuid() << descriptor.name() << value;
}

void HaloBluetooth::serviceErrorOccurred(QLowEnergyService::ServiceError error)
{
    qDebug() << "service error" << error;
}

void HaloBluetooth::serviceStateChanged(QLowEnergyService::ServiceState state)
{
    auto service = static_cast<QLowEnergyService*>(sender());
    switch (state) {
    case QLowEnergyService::RemoteServiceDiscovering:
        // qDebug() << "service discovering" << service;
        break;
    case QLowEnergyService::RemoteServiceDiscovered: {
        auto it = std::find_if(mDevices.begin(), mDevices.end(),
                               [service](const auto& other) {
                                   return service == other.service;
                               });
        if (it == mDevices.end()) {
            qDebug() << "no device for characteristic?";
            return;
        }

        // for (const auto& charr : service->characteristics()) {
        //     qDebug() << charr.name() << charr.uuid();
        // }

        static const QUuid characteristicLow = QUuid::fromString("c4edc000-9daf-11e3-8003-00025b000b00");
        static const QUuid characteristicHigh = QUuid::fromString("c4edc000-9daf-11e3-8004-00025b000b00");
        const auto low = service->characteristic(characteristicLow);
        const auto high = service->characteristic(characteristicHigh);
        if (low.isValid() && high.isValid()) {
            it->low = low;
            it->high = high;
            it->ready = true;
            qDebug() << "device ready" << it->info.deviceUuid();

            if (mDevices.size() == firstLocation()->devices.size()) {
                bool allReady = true;
                for (const auto& dev : mDevices) {
                    if (!dev.ready) {
                        allReady = false;
                        break;
                    }
                }
                if (allReady) {
                    writePendingPackets();
                    // can this get to >1 if the device disconnects during initialization?
                    if (it->connectCount == 1) {
                        emit devicesReady();
                    }
                }
            }
        }
        // qDebug() << "service discovered" << service << low.isValid() << high.isValid();
        break; }
    default:
        break;
    }
}

void HaloBluetooth::setBrightness(uint8_t deviceId, uint8_t brightness)
{
    QByteArray packet = QByteArray::fromHex("808073000A0000000000000000");
    packet[0] += deviceId;
    packet[8] = brightness;
    qDebug() << "wanting to write brightness" << packet.toHex();

    writePacket(packet);
}

void HaloBluetooth::setColorTemperature(uint8_t deviceId, uint16_t temperature)
{
    uint8_t tempBuf[2];
    memcpy(tempBuf, &temperature, 2);

    QByteArray packet = QByteArray::fromHex("808073001D0000000100000000");
    packet[0] += deviceId;
    // big endian
    packet[9] += tempBuf[1];
    packet[10] += tempBuf[0];
    qDebug() << "wanting to write temperature" << packet.toHex();

    writePacket(packet);
}

uint32_t HaloBluetooth::randomSeq()
{
    return mRandom.bounded(1, 16777215);
}

void HaloBluetooth::writePendingPackets()
{
    QList<QByteArray> pendingPackets;
    std::swap(pendingPackets, mPendingPackets);
    for (const auto& packet : pendingPackets) {
        writePacket(packet);
    }
}

void HaloBluetooth::scheduleNextPacket()
{
    if (mScheduledPacket) {
        return;
    }
    mScheduledPacket = true;
    QTimer::singleShot(mDeviceDelay, this, &HaloBluetooth::writeNextPacket);
}

void HaloBluetooth::writeNextPacket()
{
    mWritingPacket = false;
    mScheduledPacket = false;
    if (mPendingPackets.isEmpty()) {
        return;
    }
    // this is not pretty
    QList<QByteArray> pendingPackets;
    std::swap(pendingPackets, mPendingPackets);
    for (const auto& packet : pendingPackets) {
        writePacket(packet);
    }
}

void HaloBluetooth::writePacketInternal(const QByteArray& packet)
{
    //qDebug() << "num devices" << mDevices.size();
    for (auto& device : mDevices) {
        if (!device.ready) {
            return;
        }
        if (!device.low.isValid() || !device.high.isValid()) {
            //qDebug() << "characteristic not valid" << device.low.isValid() << device.high.isValid();
            return;
        }

        const auto& csrpacket = crypto::makePacket(mKey, randomSeq(), packet);
        const auto& csrlow = csrpacket.mid(0, 20);
        const auto& csrhigh = csrpacket.mid(20);

        // qDebug() << "writing csr" << csrpacket.size();
        device.service->writeCharacteristic(device.low, csrlow, QLowEnergyService::WriteWithoutResponse);
        device.service->writeCharacteristic(device.high, csrhigh, QLowEnergyService::WriteWithoutResponse);
    }
}

void HaloBluetooth::writePacket(const QByteArray& packet)
{
    bool allReady = true, anyConnected = false;
    for (auto& dev : mDevices) {
        if (!dev.connected) {
            qDebug() << "reconnecting" << dev.info.deviceUuid();
            if (!dev.connecting) {
                addDevice(dev.info);
                if (!dev.connecting) {
                    dev.connecting = true;
                    dev.controller->connectToDevice();
                }
            }
            allReady = false;
        } else if (!dev.ready) {
            allReady = false;
            anyConnected = true;
        } else {
            anyConnected = true;
        }
    }
    if (!anyConnected) {
        rediscover();
    }
    if (!allReady || mWritingPacket) {
        mPendingPackets.append(packet);
        if (allReady) {
            scheduleNextPacket();
        }
        return;
    }
    mWritingPacket = true;
    writePacketInternal(packet);
}

#include "moc_HaloBluetooth.cpp"
