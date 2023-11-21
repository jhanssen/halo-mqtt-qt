#include "HaloBluetooth.h"
#include "Crypto.h"
#include <QCoreApplication.h>
#include <QPermissions>
#include <QDebug>
#include <cassert>

HaloBluetooth::HaloBluetooth(Locations&& locations, QObject* parent)
    : QObject(parent), mLocations(std::move(locations))
{
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

void HaloBluetooth::startDiscovery()
{
    // qDebug() << "discovering";
    mDiscoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void HaloBluetooth::addDevice(const QBluetoothDeviceInfo& info)
{
    auto it = std::find_if(mDevices.begin(), mDevices.end(),
                           [&info](const auto& other) {
                               return info.deviceUuid() == other.info.deviceUuid();
                           });
    if (it != mDevices.end()) {
        // already added?
        return;
    }

    Device dev = {
        info,
        nullptr,
        nullptr,
        {},
        {}
    };

    dev.controller = QLowEnergyController::createCentral(info, this);
    QObject::connect(dev.controller, &QLowEnergyController::serviceDiscovered,
                     this, &HaloBluetooth::deviceServiceDiscovered);
    QObject::connect(dev.controller, &QLowEnergyController::errorOccurred,
                     this, &HaloBluetooth::deviceErrorOccurred);
    QObject::connect(dev.controller, &QLowEnergyController::connected,
                     this, &HaloBluetooth::deviceConnected);
    QObject::connect(dev.controller, &QLowEnergyController::disconnected,
                     this, &HaloBluetooth::deviceDisconnected);
    dev.controller->connectToDevice();

    mDevices.append(std::move(dev));
}

void HaloBluetooth::deviceConnected()
{
    // qDebug() << "device connected";

    auto controller = static_cast<QLowEnergyController*>(sender());
    controller->discoverServices();
}

void HaloBluetooth::deviceDisconnected()
{
    //qDebug() << "device disconnected";
}

void HaloBluetooth::deviceErrorOccurred(QLowEnergyController::Error error)
{
    //qDebug() << "device error" << error;
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

        connect(serviceObject, &QLowEnergyService::stateChanged, this, &HaloBluetooth::serviceStateChanged);
        connect(serviceObject, &QLowEnergyService::errorOccurred, this, &HaloBluetooth::serviceErrorOccurred);
        connect(serviceObject, &QLowEnergyService::characteristicChanged, this, &HaloBluetooth::serviceCharacteristicChanged);
        connect(serviceObject, &QLowEnergyService::descriptorWritten, this, &HaloBluetooth::serviceDescriptorWritten);
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
    // qDebug() << "service error" << error;
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

            emit deviceReady(it->info);
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

uint32_t HaloBluetooth::randomSeq()
{
    return mRandom.bounded(1, 16777215);
}

void HaloBluetooth::writePacket(Device* device, const QByteArray& packet)
{
    if (!device->low.isValid() || !device->high.isValid()) {
        qDebug() << "characteristic not valid" << device->low.isValid() << device->high.isValid();
        return;
    }

    const auto& csrpacket = crypto::makePacket(mKey, randomSeq(), packet);
    const auto& csrlow = csrpacket.mid(0, 20);
    const auto& csrhigh = csrpacket.mid(20);

    // qDebug() << "writing csr" << csrpacket.size();
    device->service->writeCharacteristic(device->low, csrlow, QLowEnergyService::WriteWithoutResponse);
    device->service->writeCharacteristic(device->high, csrhigh, QLowEnergyService::WriteWithoutResponse);
}

void HaloBluetooth::writePacket(const QBluetoothUuid& uuid, const QByteArray& packet)
{
    auto it = std::find_if(mDevices.begin(), mDevices.end(),
                           [&uuid](const auto& other) {
                               return uuid == other.info.deviceUuid();
                           });
    if (it == mDevices.end()) {
        qDebug() << "no device to write to";
        return;
    }

    writePacket(&*it, packet);
}

void HaloBluetooth::writePacket(const QByteArray& packet)
{
    for (auto& device : mDevices) {
        writePacket(&device, packet);
    }
}

#include "moc_HaloBluetooth.cpp"
