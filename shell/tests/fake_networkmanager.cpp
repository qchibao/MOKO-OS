#include "systemcontroltypes.h"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QThread>
#include <QVariantMap>

#include <memory>
#include <vector>

using VariantMapList = QList<QVariantMap>;
using MokoSystemControl::DbusInterfaceMap;

Q_DECLARE_METATYPE(VariantMapList)

namespace {

const QString managerPath = QStringLiteral("/org/freedesktop/NetworkManager");
const QString devicePath = QStringLiteral("/org/freedesktop/NetworkManager/Devices/1");
const QString ip4ConfigPath = QStringLiteral("/org/freedesktop/NetworkManager/IP4Config/1");
const QString activeConnectionPath =
    QStringLiteral("/org/freedesktop/NetworkManager/ActiveConnection/1");

struct FakeNetworkState
{
    bool connectionReady = true;
    QString activeAccessPointPath;
};

class FakeNetworkManager final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager")
    Q_PROPERTY(bool WirelessEnabled READ wirelessEnabled)
    Q_PROPERTY(uint Connectivity READ connectivity)

public:
    explicit FakeNetworkManager(FakeNetworkState *state)
        : m_state(state)
    {
    }

    bool wirelessEnabled() const
    {
        delay();
        return true;
    }

    uint connectivity() const
    {
        delay();
        return m_state->connectionReady ? 4U
            : m_state->activeAccessPointPath.isEmpty() ? 1U : 2U;
    }

    void setDevices(const QList<QDBusObjectPath> &devices) { m_devices = devices; }

public slots:
    QList<QDBusObjectPath> GetDevices() const { return m_devices; }
    void SetTestDelay(int milliseconds) { m_delayMs = qMax(0, milliseconds); }
    void SetConnectionReady(bool ready) { m_state->connectionReady = ready; }
    QDBusObjectPath AddAndActivateConnection(const DbusInterfaceMap &settings,
                                             const QDBusObjectPath &device,
                                             const QDBusObjectPath &specificObject,
                                             QDBusObjectPath &activeConnection)
    {
        if (device.path() != devicePath || specificObject.path().isEmpty())
            return QDBusObjectPath(QStringLiteral("/"));
        const QByteArray ssid = settings.value(QStringLiteral("802-11-wireless"))
                                    .value(QStringLiteral("ssid"))
                                    .toByteArray();
        if (ssid.isEmpty())
            return QDBusObjectPath(QStringLiteral("/"));
        m_state->activeAccessPointPath = specificObject.path();
        m_state->connectionReady = false;
        activeConnection = QDBusObjectPath(activeConnectionPath);
        return QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/Settings/1"));
    }

private:
    void delay() const
    {
        if (m_delayMs > 0)
            QThread::msleep(static_cast<unsigned long>(m_delayMs));
    }

    FakeNetworkState *m_state = nullptr;
    QList<QDBusObjectPath> m_devices;
    int m_delayMs = 0;
};

class FakeDeviceAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.Device")
    Q_PROPERTY(uint DeviceType READ deviceType)
    Q_PROPERTY(uint State READ state)
    Q_PROPERTY(QDBusObjectPath Ip4Config READ ip4Config)
    Q_PROPERTY(QDBusObjectPath Ip6Config READ ip6Config)
    Q_PROPERTY(QDBusObjectPath ActiveConnection READ activeConnection)

public:
    FakeDeviceAdaptor(QObject *parent, FakeNetworkState *state)
        : QDBusAbstractAdaptor(parent)
        , m_state(state)
    {
    }

    uint deviceType() const { return 2U; }
    uint state() const { return m_state->activeAccessPointPath.isEmpty() ? 30U : 100U; }
    QDBusObjectPath ip4Config() const
    {
        return QDBusObjectPath(m_state->activeAccessPointPath.isEmpty()
                                   ? QStringLiteral("/") : ip4ConfigPath);
    }
    QDBusObjectPath ip6Config() const { return QDBusObjectPath(QStringLiteral("/")); }
    QDBusObjectPath activeConnection() const
    {
        return QDBusObjectPath(m_state->activeAccessPointPath.isEmpty()
                                   ? QStringLiteral("/") : activeConnectionPath);
    }

public slots:
    void Disconnect()
    {
        m_state->activeAccessPointPath.clear();
        m_state->connectionReady = false;
    }

private:
    FakeNetworkState *m_state = nullptr;
};

class FakeWirelessAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.Device.Wireless")
    Q_PROPERTY(QList<QDBusObjectPath> AccessPoints READ accessPoints)
    Q_PROPERTY(QDBusObjectPath ActiveAccessPoint READ activeAccessPoint)

public:
    FakeWirelessAdaptor(QObject *parent, FakeNetworkState *state)
        : QDBusAbstractAdaptor(parent)
        , m_state(state)
    {
    }

    void setAccessPoints(const QList<QDBusObjectPath> &accessPoints)
    {
        m_accessPoints = accessPoints;
    }

    QList<QDBusObjectPath> accessPoints() const { return m_accessPoints; }
    QDBusObjectPath activeAccessPoint() const
    {
        return QDBusObjectPath(m_state->activeAccessPointPath.isEmpty()
                                   ? QStringLiteral("/") : m_state->activeAccessPointPath);
    }

public slots:
    void RequestScan(const QVariantMap &) {}

private:
    FakeNetworkState *m_state = nullptr;
    QList<QDBusObjectPath> m_accessPoints;
};

class FakeAccessPoint final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.AccessPoint")
    Q_PROPERTY(QByteArray Ssid READ ssid)
    Q_PROPERTY(uchar Strength READ strength)
    Q_PROPERTY(uint Flags READ flags)
    Q_PROPERTY(uint WpaFlags READ wpaFlags)
    Q_PROPERTY(uint RsnFlags READ rsnFlags)

public:
    FakeAccessPoint(QString name, int strength, bool secure)
        : m_ssid(std::move(name).toUtf8())
        , m_strength(static_cast<uchar>(qBound(0, strength, 100)))
        , m_secure(secure)
    {
    }

    QByteArray ssid() const { return m_ssid; }
    uchar strength() const { return m_strength; }
    uint flags() const { return m_secure ? 1U : 0U; }
    uint wpaFlags() const { return m_secure ? 0x100U : 0U; }
    uint rsnFlags() const { return m_secure ? 0x100U : 0U; }

private:
    QByteArray m_ssid;
    uchar m_strength = 0;
    bool m_secure = false;
};

class FakeIp4Config final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.IP4Config")
    Q_PROPERTY(VariantMapList AddressData READ addressData)
    Q_PROPERTY(VariantMapList RouteData READ routeData)
    Q_PROPERTY(VariantMapList NameserverData READ nameserverData)
    Q_PROPERTY(QString Gateway READ gateway)

public:
    explicit FakeIp4Config(FakeNetworkState *state)
        : m_state(state)
    {
    }

    VariantMapList addressData() const
    {
        return m_state->connectionReady
            ? VariantMapList{{{QStringLiteral("address"), QStringLiteral("192.0.2.10")},
                              {QStringLiteral("prefix"), 24U}}}
            : VariantMapList{};
    }

    VariantMapList routeData() const
    {
        return m_state->connectionReady
            ? VariantMapList{{{QStringLiteral("dest"), QStringLiteral("0.0.0.0")},
                              {QStringLiteral("prefix"), 0U},
                              {QStringLiteral("next-hop"), QStringLiteral("192.0.2.1")}}}
            : VariantMapList{};
    }

    VariantMapList nameserverData() const
    {
        return m_state->connectionReady
            ? VariantMapList{{{QStringLiteral("address"), QStringLiteral("192.0.2.53")}}}
            : VariantMapList{};
    }

    QString gateway() const
    {
        return m_state->connectionReady ? QStringLiteral("192.0.2.1") : QString{};
    }

private:
    FakeNetworkState *m_state = nullptr;
};

class FakeActiveConnection final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.NetworkManager.Connection.Active")
    Q_PROPERTY(uint State READ state)

public:
    uint state() const { return 2U; }
};

bool registerObject(QDBusConnection &bus,
                    const QString &path,
                    QObject *object,
                    QDBusConnection::RegisterOptions options)
{
    return bus.registerObject(path, object, options);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    qDBusRegisterMetaType<QList<QDBusObjectPath>>();
    qDBusRegisterMetaType<DbusInterfaceMap>();
    qDBusRegisterMetaType<VariantMapList>();

    QDBusConnection bus = QDBusConnection::systemBus();
    FakeNetworkState state;
    FakeNetworkManager manager(&state);
    QObject device;
    auto *deviceAdaptor = new FakeDeviceAdaptor(&device, &state);
    Q_UNUSED(deviceAdaptor)
    auto *wirelessAdaptor = new FakeWirelessAdaptor(&device, &state);
    FakeIp4Config ip4Config(&state);
    FakeActiveConnection activeConnection;

    constexpr int uniqueNetworkCount = 180;
    std::vector<std::unique_ptr<FakeAccessPoint>> accessPointObjects;
    QList<QDBusObjectPath> accessPointPaths;
    accessPointObjects.reserve(uniqueNetworkCount + 1);
    for (int index = 0; index <= uniqueNetworkCount; ++index) {
        const QString path = QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/%1")
                                 .arg(index + 1);
        const bool active = index == 0;
        const bool duplicateActive = index == uniqueNetworkCount;
        const QString ssid = active || duplicateActive
            ? QStringLiteral("MOKO Lab")
            : QStringLiteral("Test Network %1").arg(index, 3, 10, QLatin1Char('0'));
        auto accessPoint = std::make_unique<FakeAccessPoint>(
            ssid, duplicateActive ? 100 : active ? 62 : (index * 17) % 101, index % 3 != 0);
        if (!registerObject(bus,
                            path,
                            accessPoint.get(),
                            QDBusConnection::ExportAllProperties)) {
            return 1;
        }
        accessPointPaths.append(QDBusObjectPath(path));
        accessPointObjects.push_back(std::move(accessPoint));
    }
    wirelessAdaptor->setAccessPoints(accessPointPaths);
    state.activeAccessPointPath = accessPointPaths.constFirst().path();
    manager.setDevices({QDBusObjectPath(devicePath)});

    if (!bus.registerService(QStringLiteral("org.freedesktop.NetworkManager"))
        || !registerObject(bus,
                           managerPath,
                           &manager,
                           QDBusConnection::ExportAllContents)
        || !registerObject(bus, devicePath, &device, QDBusConnection::ExportAdaptors)
        || !registerObject(bus,
                           ip4ConfigPath,
                           &ip4Config,
                           QDBusConnection::ExportAllProperties)
        || !registerObject(bus,
                           activeConnectionPath,
                           &activeConnection,
                           QDBusConnection::ExportAllProperties)) {
        return 1;
    }
    return app.exec();
}

#include "fake_networkmanager.moc"
