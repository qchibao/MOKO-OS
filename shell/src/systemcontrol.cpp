#include "systemcontrol.h"

#include "systemcontrolutils.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QTimeZone>

#include <algorithm>
#include <unistd.h>

using DbusInterfaceMap = QMap<QString, QVariantMap>;
using DbusManagedObjects = QMap<QDBusObjectPath, DbusInterfaceMap>;
using NetworkSettings = QMap<QString, QVariantMap>;
using VariantMapList = QList<QVariantMap>;

Q_DECLARE_METATYPE(DbusInterfaceMap)
Q_DECLARE_METATYPE(DbusManagedObjects)
Q_DECLARE_METATYPE(VariantMapList)

namespace {

const QString networkManagerService = QStringLiteral("org.freedesktop.NetworkManager");
const QString networkManagerPath = QStringLiteral("/org/freedesktop/NetworkManager");
const QString networkManagerInterface = QStringLiteral("org.freedesktop.NetworkManager");
const QString networkDeviceInterface = QStringLiteral("org.freedesktop.NetworkManager.Device");
const QString wirelessDeviceInterface = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
const QString accessPointInterface = QStringLiteral("org.freedesktop.NetworkManager.AccessPoint");
const QString bluezService = QStringLiteral("org.bluez");
const QString bluezAdapterInterface = QStringLiteral("org.bluez.Adapter1");
const QString bluezDeviceInterface = QStringLiteral("org.bluez.Device1");
const QString logindService = QStringLiteral("org.freedesktop.login1");
const QString logindPath = QStringLiteral("/org/freedesktop/login1");
const QString logindInterface = QStringLiteral("org.freedesktop.login1.Manager");

QVariant unwrapped(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QDBusVariant>())
        return value.value<QDBusVariant>().variant();
    return value;
}

QVariant dbusProperty(const QString &service,
                      const QString &path,
                      const QString &interface,
                      const QString &name)
{
    QDBusInterface object(service, path, interface, QDBusConnection::systemBus());
    object.setTimeout(1500);
    return object.isValid() ? unwrapped(object.property(name.toUtf8().constData())) : QVariant{};
}

bool setDbusProperty(const QString &service,
                     const QString &path,
                     const QString &interface,
                     const QString &name,
                     const QVariant &value,
                     QString *errorMessage)
{
    QDBusInterface properties(service,
                              path,
                              QStringLiteral("org.freedesktop.DBus.Properties"),
                              QDBusConnection::systemBus());
    properties.setTimeout(3000);
    const QDBusMessage reply = properties.call(QStringLiteral("Set"),
                                                interface,
                                                name,
                                                QVariant::fromValue(QDBusVariant(value)));
    if (reply.type() == QDBusMessage::ReplyMessage)
        return true;
    if (errorMessage != nullptr)
        *errorMessage = reply.errorMessage();
    return false;
}

QString networkDeviceState(uint state)
{
    switch (state) {
    case 20:
        return QStringLiteral("Unavailable");
    case 30:
        return QStringLiteral("Disconnected");
    case 40:
    case 50:
    case 70:
    case 80:
    case 90:
        return QStringLiteral("Connecting");
    case 60:
        return QStringLiteral("Password required");
    case 100:
        return QStringLiteral("Connected");
    case 110:
        return QStringLiteral("Disconnecting");
    case 120:
        return QStringLiteral("Connection failed");
    default:
        return QStringLiteral("Disconnected");
    }
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

qint64 readIntegerFile(const QString &path)
{
    bool ok = false;
    const qint64 value = readTextFile(path).toLongLong(&ok);
    return ok ? value : -1;
}

QString formatDuration(qint64 seconds)
{
    if (seconds <= 0)
        return {};
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    if (hours > 0)
        return QStringLiteral("%1 h %2 min").arg(hours).arg(minutes);
    return QStringLiteral("%1 min").arg(minutes);
}

bool serviceRegistered(const QString &service)
{
    QDBusConnectionInterface *interface = QDBusConnection::systemBus().interface();
    return interface != nullptr && interface->isServiceRegistered(service);
}

QVariantMap endpointMap(const MokoSystemControl::AudioEndpoint &endpoint)
{
    return {{QStringLiteral("id"), endpoint.id},
            {QStringLiteral("name"), endpoint.name},
            {QStringLiteral("active"), endpoint.defaultDevice}};
}

QString executableFromEnvironment(const char *variable, const QString &fallback)
{
    const QString override = qEnvironmentVariable(variable);
    if (!override.isEmpty())
        return override;
    return QStandardPaths::findExecutable(fallback);
}

void writeLiveEvent(const QString &event)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(event.toUtf8());
    file.write("\n");
}

} // namespace

SystemControl::SystemControl(QObject *parent, bool deferInitialRefresh)
    : QObject(parent)
    , m_targetedRefreshOnly(deferInitialRefresh)
    , m_bluetoothAgent()
    , m_bluetoothAgentPath(QStringLiteral("/org/moko/BluetoothAgent_%1").arg(getpid()))
{
    static const bool registered = [] {
        qDBusRegisterMetaType<DbusInterfaceMap>();
        qDBusRegisterMetaType<DbusManagedObjects>();
        qDBusRegisterMetaType<VariantMapList>();
        return true;
    }();
    Q_UNUSED(registered)

    connect(&m_bluetoothAgent, &BluetoothAgent::promptChanged,
            this, &SystemControl::bluetoothPromptChanged);
    connect(&m_bluetoothAgent, &BluetoothAgent::agentMessage,
            this, &SystemControl::setOperationMessage);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000);
    connect(m_refreshTimer, &QTimer::timeout, this, [this] {
        if (!m_targetedRefreshOnly) {
            refresh();
            return;
        }
        if (m_networkRefreshEnabled)
            refreshNetwork();
        if (m_bluetoothRefreshEnabled)
            refreshBluetooth();
    });
    if (!deferInitialRefresh)
        m_refreshTimer->start();
    auto *clockTimer = new QTimer(this);
    clockTimer->setInterval(1000);
    connect(clockTimer, &QTimer::timeout, this, &SystemControl::refreshTime);
    clockTimer->start();
    if (!deferInitialRefresh)
        refresh();
}

SystemControl::~SystemControl()
{
    if (m_bluetoothAgentRegistered) {
        QDBusInterface manager(bluezService,
                               QStringLiteral("/org/bluez"),
                               QStringLiteral("org.bluez.AgentManager1"),
                               QDBusConnection::systemBus());
        manager.call(QDBus::NoBlock,
                     QStringLiteral("UnregisterAgent"),
                     QVariant::fromValue(QDBusObjectPath(m_bluetoothAgentPath)));
        QDBusConnection::systemBus().unregisterObject(m_bluetoothAgentPath);
    }
}

bool SystemControl::networkManagerAvailable() const { return m_networkManagerAvailable; }
bool SystemControl::wifiAvailable() const { return m_wifiAvailable; }
bool SystemControl::wifiEnabled() const { return m_wifiEnabled; }
bool SystemControl::wifiScanning() const { return m_wifiScanning; }
bool SystemControl::networkBusy() const { return m_networkBusy; }
QString SystemControl::wifiState() const { return m_wifiState; }
QString SystemControl::activeSsid() const { return m_activeSsid; }
QVariantList SystemControl::wifiNetworks() const { return m_wifiNetworks; }
bool SystemControl::bluetoothAvailable() const { return m_bluetoothAvailable; }
bool SystemControl::bluezServiceAvailable() const { return m_bluezServiceAvailable; }
bool SystemControl::bluetoothPowered() const { return m_bluetoothPowered; }
bool SystemControl::bluetoothScanning() const { return m_bluetoothScanning; }
bool SystemControl::bluetoothBusy() const { return m_bluetoothBusy; }
QVariantList SystemControl::bluetoothDevices() const { return m_bluetoothDevices; }
bool SystemControl::bluetoothPromptVisible() const { return m_bluetoothAgent.promptVisible(); }
QString SystemControl::bluetoothPromptKind() const { return m_bluetoothAgent.promptKind(); }
QString SystemControl::bluetoothPromptTitle() const { return m_bluetoothAgent.promptTitle(); }
QString SystemControl::bluetoothPromptMessage() const { return m_bluetoothAgent.promptMessage(); }
QString SystemControl::bluetoothPromptValue() const { return m_bluetoothAgent.promptValue(); }
bool SystemControl::bluetoothPromptNeedsInput() const { return m_bluetoothAgent.promptNeedsInput(); }
bool SystemControl::audioAvailable() const { return m_audioAvailable; }
int SystemControl::outputVolume() const { return m_outputVolume; }
bool SystemControl::outputMuted() const { return m_outputMuted; }
QString SystemControl::outputDeviceName() const { return m_outputDeviceName; }
QVariantList SystemControl::outputDevices() const { return m_outputDevices; }
int SystemControl::inputVolume() const { return m_inputVolume; }
bool SystemControl::inputMuted() const { return m_inputMuted; }
QString SystemControl::inputDeviceName() const { return m_inputDeviceName; }
QVariantList SystemControl::inputDevices() const { return m_inputDevices; }
bool SystemControl::brightnessAvailable() const { return m_brightnessAvailable; }
int SystemControl::brightness() const { return m_brightness; }
bool SystemControl::batteryAvailable() const { return m_batteryAvailable; }
int SystemControl::batteryPercent() const { return m_batteryPercent; }
int SystemControl::batteryHealth() const { return m_batteryHealth; }
QString SystemControl::batteryState() const { return m_batteryState; }
QString SystemControl::batteryTime() const { return m_batteryTime; }
QString SystemControl::batteryTechnology() const { return m_batteryTechnology; }
QString SystemControl::batteryCycleCount() const { return m_batteryCycleCount; }
QString SystemControl::batteryEnergy() const { return m_batteryEnergy; }
bool SystemControl::externalPowerConnected() const { return m_externalPowerConnected; }
bool SystemControl::batteryCharging() const { return m_batteryCharging; }
bool SystemControl::powerModeAvailable() const { return m_powerModeAvailable; }
QString SystemControl::powerMode() const { return m_powerMode; }
QStringList SystemControl::powerModes() const { return m_powerModes; }
bool SystemControl::suspendAvailable() const { return m_suspendAvailable; }
bool SystemControl::suspendPending() const { return m_suspendPending; }
QString SystemControl::clockText() const { return m_clockText; }
QString SystemControl::dateText() const { return m_dateText; }
QString SystemControl::timeZoneName() const { return m_timeZoneName; }
QString SystemControl::operationMessage() const { return m_operationMessage; }

void SystemControl::refresh()
{
    refreshNetwork();
    refreshBluetooth();
    refreshAudio();
    refreshPower();
    refreshTime();
}

void SystemControl::startFullRefresh()
{
    m_targetedRefreshOnly = false;
    if (!m_refreshTimer->isActive())
        m_refreshTimer->start();
    refresh();
}

void SystemControl::preloadNetwork()
{
    m_networkRefreshEnabled = true;
    if (!m_refreshTimer->isActive())
        m_refreshTimer->start();
    if (m_networkPreloadPending)
        return;
    m_networkPreloadPending = true;
    // Defer discovery until the event loop has rendered the page. This keeps
    // Settings responsive while NetworkManager performs its own scan.
    QTimer::singleShot(0, this, [this] {
        refreshNetwork();
        m_networkPreloadPending = false;
        if (m_wifiAvailable && m_wifiEnabled && !m_wifiScanning)
            requestWifiScan();
    });
}

void SystemControl::preloadBluetooth()
{
    m_bluetoothRefreshEnabled = true;
    if (!m_refreshTimer->isActive())
        m_refreshTimer->start();
    if (m_bluetoothPreloadPending)
        return;
    m_bluetoothPreloadPending = true;
    QTimer::singleShot(0, this, [this] {
        refreshBluetooth();
        m_bluetoothPreloadPending = false;
        if (m_bluetoothAvailable && m_bluetoothPowered && !m_bluetoothScanning)
            setBluetoothScanning(true);
    });
}

void SystemControl::refreshNetwork()
{
    m_networkManagerAvailable = serviceRegistered(networkManagerService);
    m_wifiAvailable = false;
    m_wifiEnabled = false;
    m_wifiState = m_networkManagerAvailable ? QStringLiteral("Disconnected")
                                             : QStringLiteral("Unavailable");
    m_activeSsid.clear();
    m_wifiDevicePath.clear();
    m_wifiNetworks.clear();

    if (!m_networkManagerAvailable) {
        emit networkChanged();
        return;
    }

    m_wifiEnabled = dbusProperty(networkManagerService,
                                 networkManagerPath,
                                 networkManagerInterface,
                                 QStringLiteral("WirelessEnabled"))
                        .toBool();
    QDBusInterface manager(networkManagerService,
                           networkManagerPath,
                           networkManagerInterface,
                           QDBusConnection::systemBus());
    const QDBusReply<QList<QDBusObjectPath>> devices = manager.call(QStringLiteral("GetDevices"));
    if (!devices.isValid()) {
        setOperationMessage(QStringLiteral("Could not read network devices: %1")
                                .arg(devices.error().message()));
        emit networkChanged();
        return;
    }

    for (const QDBusObjectPath &device : devices.value()) {
        const uint type = dbusProperty(networkManagerService,
                                       device.path(),
                                       networkDeviceInterface,
                                       QStringLiteral("DeviceType"))
                              .toUInt();
        if (type == 2) {
            m_wifiDevicePath = device.path();
            break;
        }
    }
    if (m_wifiDevicePath.isEmpty()) {
        emit networkChanged();
        return;
    }

    m_wifiAvailable = true;
    m_wifiState = networkDeviceState(dbusProperty(networkManagerService,
                                                  m_wifiDevicePath,
                                                  networkDeviceInterface,
                                                  QStringLiteral("State"))
                                         .toUInt());
    const QDBusObjectPath activePath = dbusProperty(networkManagerService,
                                                    m_wifiDevicePath,
                                                    wirelessDeviceInterface,
                                                    QStringLiteral("ActiveAccessPoint"))
                                           .value<QDBusObjectPath>();

    QDBusInterface wireless(networkManagerService,
                            m_wifiDevicePath,
                            wirelessDeviceInterface,
                            QDBusConnection::systemBus());
    const QDBusReply<QList<QDBusObjectPath>> accessPoints =
        wireless.call(QStringLiteral("GetAccessPoints"));
    QMap<QString, QVariantMap> strongestBySsid;
    if (accessPoints.isValid()) {
        for (const QDBusObjectPath &accessPoint : accessPoints.value()) {
            const QByteArray ssidBytes = dbusProperty(networkManagerService,
                                                      accessPoint.path(),
                                                      accessPointInterface,
                                                      QStringLiteral("Ssid"))
                                             .toByteArray();
            const QString ssid = MokoSystemControl::decodeSsid(ssidBytes);
            if (ssid.isEmpty())
                continue;
            const int strength = dbusProperty(networkManagerService,
                                              accessPoint.path(),
                                              accessPointInterface,
                                              QStringLiteral("Strength"))
                                     .toInt();
            const uint flags = dbusProperty(networkManagerService,
                                            accessPoint.path(),
                                            accessPointInterface,
                                            QStringLiteral("Flags"))
                                   .toUInt();
            const uint wpaFlags = dbusProperty(networkManagerService,
                                               accessPoint.path(),
                                               accessPointInterface,
                                               QStringLiteral("WpaFlags"))
                                      .toUInt();
            const uint rsnFlags = dbusProperty(networkManagerService,
                                               accessPoint.path(),
                                               accessPointInterface,
                                               QStringLiteral("RsnFlags"))
                                      .toUInt();
            const bool connected = !activePath.path().isEmpty()
                && activePath.path() != QStringLiteral("/")
                && activePath.path() == accessPoint.path();
            QVariantMap entry{{QStringLiteral("ssid"), ssid},
                              {QStringLiteral("id"), accessPoint.path()},
                              {QStringLiteral("strength"), strength},
                              {QStringLiteral("secure"), (flags & 1U) || wpaFlags || rsnFlags},
                              {QStringLiteral("connected"), connected}};
            if (connected)
                m_activeSsid = ssid;
            if (!strongestBySsid.contains(ssid)
                || strongestBySsid.value(ssid).value(QStringLiteral("strength")).toInt() < strength
                || connected) {
                strongestBySsid.insert(ssid, entry);
            }
        }
    }
    for (const QVariantMap &network : strongestBySsid.values())
        m_wifiNetworks.append(network);
    std::sort(m_wifiNetworks.begin(), m_wifiNetworks.end(), [](const QVariant &left,
                                                                const QVariant &right) {
        const QVariantMap a = left.toMap();
        const QVariantMap b = right.toMap();
        if (a.value(QStringLiteral("connected")).toBool()
            != b.value(QStringLiteral("connected")).toBool()) {
            return a.value(QStringLiteral("connected")).toBool();
        }
        return a.value(QStringLiteral("strength")).toInt()
            > b.value(QStringLiteral("strength")).toInt();
    });
    emit networkChanged();
}

bool SystemControl::setWifiEnabled(bool enabled)
{
    if (!m_networkManagerAvailable)
        return false;
    QString error;
    if (!setDbusProperty(networkManagerService,
                         networkManagerPath,
                         networkManagerInterface,
                         QStringLiteral("WirelessEnabled"),
                         enabled,
                         &error)) {
        setOperationMessage(QStringLiteral("Could not change Wi-Fi: %1").arg(error));
        return false;
    }
    m_wifiEnabled = enabled;
    setOperationMessage(enabled ? QStringLiteral("Wi-Fi turned on")
                                : QStringLiteral("Wi-Fi turned off"));
    emit networkChanged();
    QTimer::singleShot(400, this, &SystemControl::refreshNetwork);
    return true;
}

bool SystemControl::requestWifiScan()
{
    if (!m_wifiAvailable || !m_wifiEnabled || m_networkBusy)
        return false;
    QDBusInterface wireless(networkManagerService,
                            m_wifiDevicePath,
                            wirelessDeviceInterface,
                            QDBusConnection::systemBus());
    const QVariantMap options;
    const QDBusMessage reply = wireless.call(QStringLiteral("RequestScan"), options);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        setOperationMessage(QStringLiteral("Wi-Fi scan failed: %1").arg(reply.errorMessage()));
        return false;
    }
    m_wifiScanning = true;
    setOperationMessage(QStringLiteral("Scanning for Wi-Fi networks"));
    emit networkChanged();
    QTimer::singleShot(3500, this, [this] {
        m_wifiScanning = false;
        refreshNetwork();
    });
    return true;
}

QVariantMap SystemControl::wifiNetwork(const QString &ssid) const
{
    for (const QVariant &entry : m_wifiNetworks) {
        const QVariantMap network = entry.toMap();
        if (network.value(QStringLiteral("ssid")).toString() == ssid)
            return network;
    }
    return {};
}

bool SystemControl::connectWifi(const QString &ssid, const QString &password)
{
    if (!m_wifiAvailable || !m_wifiEnabled || m_networkBusy)
        return false;
    const QString cleanSsid = ssid.trimmed();
    if (cleanSsid.isEmpty() || cleanSsid.toUtf8().size() > 32) {
        setOperationMessage(QStringLiteral("Enter a valid Wi-Fi network name."));
        return false;
    }
    const QVariantMap network = wifiNetwork(cleanSsid);
    const QString accessPointPath = network.value(
        QStringLiteral("id"), QStringLiteral("/")).toString();
    const bool secure = network.isEmpty() ? !password.isEmpty()
                                          : network.value(QStringLiteral("secure")).toBool();
    if (secure && password.isEmpty()) {
        setOperationMessage(QStringLiteral("A password is required for %1.").arg(cleanSsid));
        return false;
    }

    NetworkSettings settings;
    settings.insert(QStringLiteral("connection"),
                    {{QStringLiteral("id"), cleanSsid},
                     {QStringLiteral("type"), QStringLiteral("802-11-wireless")},
                     {QStringLiteral("autoconnect"), true}});
    settings.insert(QStringLiteral("802-11-wireless"),
                    {{QStringLiteral("ssid"), cleanSsid.toUtf8()},
                     {QStringLiteral("mode"), QStringLiteral("infrastructure")}});
    if (secure) {
        settings.insert(QStringLiteral("802-11-wireless-security"),
                        {{QStringLiteral("key-mgmt"), QStringLiteral("wpa-psk")},
                         {QStringLiteral("psk"), password}});
    }
    settings.insert(QStringLiteral("ipv4"), {{QStringLiteral("method"), QStringLiteral("auto")}});
    settings.insert(QStringLiteral("ipv6"), {{QStringLiteral("method"), QStringLiteral("auto")}});

    QDBusInterface manager(networkManagerService,
                           networkManagerPath,
                           networkManagerInterface,
                           QDBusConnection::systemBus());
    const QVariantList arguments{
        QVariant::fromValue(settings),
        QVariant::fromValue(QDBusObjectPath(m_wifiDevicePath)),
        QVariant::fromValue(QDBusObjectPath(accessPointPath)),
    };
    auto *watcher = new QDBusPendingCallWatcher(
        manager.asyncCallWithArgumentList(QStringLiteral("AddAndActivateConnection"), arguments),
        this);
    m_networkBusy = true;
    m_wifiState = QStringLiteral("Connecting");
    setOperationMessage(QStringLiteral("Connecting to %1").arg(cleanSsid));
    emit networkChanged();
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, cleanSsid](QDBusPendingCallWatcher *finished) {
                QDBusPendingReply<QDBusObjectPath, QDBusObjectPath> reply = *finished;
                m_networkBusy = false;
                if (reply.isError())
                    setOperationMessage(QStringLiteral("Could not connect to %1: %2")
                                            .arg(cleanSsid, reply.error().message()));
                else
                    setOperationMessage(QStringLiteral("Connected to %1").arg(cleanSsid));
                finished->deleteLater();
                QTimer::singleShot(300, this, &SystemControl::refreshNetwork);
            });
    return true;
}

bool SystemControl::disconnectWifi()
{
    if (!m_wifiAvailable || m_activeSsid.isEmpty() || m_networkBusy)
        return false;
    QDBusInterface device(networkManagerService,
                          m_wifiDevicePath,
                          networkDeviceInterface,
                          QDBusConnection::systemBus());
    const QDBusMessage reply = device.call(QStringLiteral("Disconnect"));
    if (reply.type() != QDBusMessage::ReplyMessage) {
        setOperationMessage(QStringLiteral("Could not disconnect Wi-Fi: %1")
                                .arg(reply.errorMessage()));
        return false;
    }
    setOperationMessage(QStringLiteral("Wi-Fi disconnected"));
    QTimer::singleShot(250, this, &SystemControl::refreshNetwork);
    return true;
}

void SystemControl::refreshBluetooth()
{
    m_bluetoothAvailable = false;
    m_bluetoothPowered = false;
    m_bluetoothScanning = false;
    m_bluetoothAdapterPath.clear();
    m_bluetoothDevices.clear();
    m_bluezServiceAvailable = serviceRegistered(bluezService);
    if (!m_bluezServiceAvailable) {
        emit bluetoothChanged();
        return;
    }

    QDBusInterface objectManager(bluezService,
                                 QStringLiteral("/"),
                                 QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                 QDBusConnection::systemBus());
    const QDBusReply<DbusManagedObjects> reply = objectManager.call(QStringLiteral("GetManagedObjects"));
    if (!reply.isValid()) {
        setOperationMessage(QStringLiteral("Could not read Bluetooth devices: %1")
                                .arg(reply.error().message()));
        emit bluetoothChanged();
        return;
    }

    for (auto iterator = reply.value().cbegin(); iterator != reply.value().cend(); ++iterator) {
        const DbusInterfaceMap interfaces = iterator.value();
        if (m_bluetoothAdapterPath.isEmpty() && interfaces.contains(bluezAdapterInterface)) {
            m_bluetoothAdapterPath = iterator.key().path();
            const QVariantMap adapter = interfaces.value(bluezAdapterInterface);
            m_bluetoothPowered = adapter.value(QStringLiteral("Powered")).toBool();
            m_bluetoothScanning = adapter.value(QStringLiteral("Discovering")).toBool();
        }
        if (!interfaces.contains(bluezDeviceInterface))
            continue;
        const QVariantMap device = interfaces.value(bluezDeviceInterface);
        const QString name = device.value(QStringLiteral("Alias")).toString().trimmed().isEmpty()
            ? device.value(QStringLiteral("Name")).toString()
            : device.value(QStringLiteral("Alias")).toString();
        m_bluetoothDevices.append(QVariantMap{
            {QStringLiteral("id"), iterator.key().path()},
            {QStringLiteral("name"), name.isEmpty() ? QStringLiteral("Bluetooth device") : name},
            {QStringLiteral("paired"), device.value(QStringLiteral("Paired")).toBool()},
            {QStringLiteral("connected"), device.value(QStringLiteral("Connected")).toBool()},
            {QStringLiteral("trusted"), device.value(QStringLiteral("Trusted")).toBool()},
            {QStringLiteral("strength"), device.value(QStringLiteral("RSSI"), -100).toInt()},
        });
    }
    m_bluetoothAvailable = !m_bluetoothAdapterPath.isEmpty();
    std::sort(m_bluetoothDevices.begin(), m_bluetoothDevices.end(), [](const QVariant &left,
                                                                       const QVariant &right) {
        const QVariantMap a = left.toMap();
        const QVariantMap b = right.toMap();
        if (a.value(QStringLiteral("connected")).toBool()
            != b.value(QStringLiteral("connected")).toBool()) {
            return a.value(QStringLiteral("connected")).toBool();
        }
        if (a.value(QStringLiteral("paired")).toBool()
            != b.value(QStringLiteral("paired")).toBool()) {
            return a.value(QStringLiteral("paired")).toBool();
        }
        return a.value(QStringLiteral("name")).toString().localeAwareCompare(
                   b.value(QStringLiteral("name")).toString()) < 0;
    });
    if (m_bluetoothAvailable)
        ensureBluetoothAgent();
    emit bluetoothChanged();
}

void SystemControl::ensureBluetoothAgent()
{
    if (m_bluetoothAgentRegistered)
        return;
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.registerObject(m_bluetoothAgentPath,
                            &m_bluetoothAgent,
                            QDBusConnection::ExportAllSlots)) {
        return;
    }
    QDBusInterface manager(bluezService,
                           QStringLiteral("/org/bluez"),
                           QStringLiteral("org.bluez.AgentManager1"),
                           bus);
    QDBusMessage reply = manager.call(QStringLiteral("RegisterAgent"),
                                      QVariant::fromValue(QDBusObjectPath(m_bluetoothAgentPath)),
                                      QStringLiteral("KeyboardDisplay"));
    if (reply.type() != QDBusMessage::ReplyMessage
        && reply.errorName() != QStringLiteral("org.bluez.Error.AlreadyExists")) {
        bus.unregisterObject(m_bluetoothAgentPath);
        return;
    }
    manager.call(QDBus::NoBlock,
                 QStringLiteral("RequestDefaultAgent"),
                 QVariant::fromValue(QDBusObjectPath(m_bluetoothAgentPath)));
    m_bluetoothAgentRegistered = true;
}

bool SystemControl::setBluetoothPowered(bool powered)
{
    if (!m_bluetoothAvailable)
        return false;
    QString error;
    if (!setDbusProperty(bluezService,
                         m_bluetoothAdapterPath,
                         bluezAdapterInterface,
                         QStringLiteral("Powered"),
                         powered,
                         &error)) {
        setOperationMessage(QStringLiteral("Could not change Bluetooth: %1").arg(error));
        return false;
    }
    m_bluetoothPowered = powered;
    setOperationMessage(powered ? QStringLiteral("Bluetooth turned on")
                                : QStringLiteral("Bluetooth turned off"));
    emit bluetoothChanged();
    QTimer::singleShot(300, this, &SystemControl::refreshBluetooth);
    return true;
}

bool SystemControl::setBluetoothScanning(bool scanning)
{
    if (!m_bluetoothAvailable || !m_bluetoothPowered || m_bluetoothBusy)
        return false;
    QDBusInterface adapter(bluezService,
                           m_bluetoothAdapterPath,
                           bluezAdapterInterface,
                           QDBusConnection::systemBus());
    const QDBusMessage reply = adapter.call(scanning ? QStringLiteral("StartDiscovery")
                                                     : QStringLiteral("StopDiscovery"));
    if (reply.type() != QDBusMessage::ReplyMessage
        && reply.errorName() != QStringLiteral("org.bluez.Error.InProgress")
        && reply.errorName() != QStringLiteral("org.bluez.Error.NotReady")) {
        setOperationMessage(QStringLiteral("Bluetooth scan failed: %1").arg(reply.errorMessage()));
        return false;
    }
    m_bluetoothScanning = scanning;
    setOperationMessage(scanning ? QStringLiteral("Scanning for Bluetooth devices")
                                 : QStringLiteral("Bluetooth scan stopped"));
    emit bluetoothChanged();
    return true;
}

QVariantMap SystemControl::bluetoothDevice(const QString &deviceId) const
{
    for (const QVariant &entry : m_bluetoothDevices) {
        const QVariantMap device = entry.toMap();
        if (device.value(QStringLiteral("id")).toString() == deviceId)
            return device;
    }
    return {};
}

bool SystemControl::bluetoothCall(const QString &deviceId,
                                  const QString &method,
                                  const QString &successMessage,
                                  bool startsPairing)
{
    if (!m_bluetoothAvailable || m_bluetoothBusy)
        return false;
    const QVariantMap device = bluetoothDevice(deviceId);
    if (device.isEmpty()) {
        setOperationMessage(QStringLiteral("That Bluetooth device is no longer available."));
        return false;
    }
    const QString name = device.value(QStringLiteral("name")).toString();
    if (startsPairing) {
        ensureBluetoothAgent();
        m_bluetoothAgent.beginPairing(deviceId, name);
    } else {
        m_bluetoothAgent.allowServiceAuthorization(deviceId, name);
    }
    QDBusInterface object(bluezService,
                          deviceId,
                          bluezDeviceInterface,
                          QDBusConnection::systemBus());
    auto *watcher = new QDBusPendingCallWatcher(object.asyncCall(method), this);
    m_bluetoothBusy = true;
    setOperationMessage(QStringLiteral("%1 %2").arg(method, name));
    emit bluetoothChanged();
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, deviceId, name, successMessage, startsPairing](QDBusPendingCallWatcher *finished) {
                QDBusPendingReply<> reply = *finished;
                m_bluetoothBusy = false;
                if (reply.isError()) {
                    setOperationMessage(QStringLiteral("Bluetooth action failed for %1: %2")
                                            .arg(name, reply.error().message()));
                } else {
                    if (startsPairing) {
                        QString ignored;
                        setDbusProperty(bluezService,
                                        deviceId,
                                        bluezDeviceInterface,
                                        QStringLiteral("Trusted"),
                                        true,
                                        &ignored);
                    }
                    setOperationMessage(successMessage.arg(name));
                }
                finished->deleteLater();
                QTimer::singleShot(250, this, &SystemControl::refreshBluetooth);
            });
    return true;
}

bool SystemControl::pairBluetoothDevice(const QString &deviceId)
{
    return bluetoothCall(deviceId,
                         QStringLiteral("Pair"),
                         QStringLiteral("%1 paired"),
                         true);
}

bool SystemControl::connectBluetoothDevice(const QString &deviceId)
{
    return bluetoothCall(deviceId,
                         QStringLiteral("Connect"),
                         QStringLiteral("%1 connected"));
}

bool SystemControl::disconnectBluetoothDevice(const QString &deviceId)
{
    return bluetoothCall(deviceId,
                         QStringLiteral("Disconnect"),
                         QStringLiteral("%1 disconnected"));
}

bool SystemControl::forgetBluetoothDevice(const QString &deviceId)
{
    if (!m_bluetoothAvailable || m_bluetoothBusy || bluetoothDevice(deviceId).isEmpty())
        return false;
    QDBusInterface adapter(bluezService,
                           m_bluetoothAdapterPath,
                           bluezAdapterInterface,
                           QDBusConnection::systemBus());
    const QDBusMessage reply = adapter.call(QStringLiteral("RemoveDevice"),
                                            QVariant::fromValue(QDBusObjectPath(deviceId)));
    if (reply.type() != QDBusMessage::ReplyMessage) {
        setOperationMessage(QStringLiteral("Could not forget Bluetooth device: %1")
                                .arg(reply.errorMessage()));
        return false;
    }
    setOperationMessage(QStringLiteral("Bluetooth device forgotten"));
    QTimer::singleShot(200, this, &SystemControl::refreshBluetooth);
    return true;
}

void SystemControl::answerBluetoothPrompt(const QString &value, bool accepted)
{
    m_bluetoothAgent.respond(value, accepted);
}

bool SystemControl::runWpctl(const QStringList &arguments, QString *output)
{
    const QString program = executableFromEnvironment("MOKO_WPCTL", QStringLiteral("wpctl"));
    if (program.isEmpty())
        return false;
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(1000) || !process.waitForFinished(2500)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    if (output != nullptr)
        *output = QString::fromUtf8(process.readAll()).trimmed();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

void SystemControl::refreshAudio()
{
    QString status;
    const bool statusOk = runWpctl({QStringLiteral("status"), QStringLiteral("--name")}, &status);
    m_outputDevices.clear();
    m_inputDevices.clear();
    m_outputDeviceName.clear();
    m_inputDeviceName.clear();
    if (statusOk) {
        const auto outputs = MokoSystemControl::parseWpctlEndpoints(status, QStringLiteral("Sinks"));
        for (const auto &endpoint : outputs) {
            m_outputDevices.append(endpointMap(endpoint));
            if (endpoint.defaultDevice)
                m_outputDeviceName = endpoint.name;
        }
        const auto inputs = MokoSystemControl::parseWpctlEndpoints(status, QStringLiteral("Sources"));
        for (const auto &endpoint : inputs) {
            m_inputDevices.append(endpointMap(endpoint));
            if (endpoint.defaultDevice)
                m_inputDeviceName = endpoint.name;
        }
    }

    QString outputLevelText;
    QString inputLevelText;
    const auto outputLevel = runWpctl({QStringLiteral("get-volume"),
                                      QStringLiteral("@DEFAULT_AUDIO_SINK@")},
                                     &outputLevelText)
        ? MokoSystemControl::parseWpctlVolume(outputLevelText)
        : MokoSystemControl::AudioLevel{};
    const auto inputLevel = runWpctl({QStringLiteral("get-volume"),
                                     QStringLiteral("@DEFAULT_AUDIO_SOURCE@")},
                                    &inputLevelText)
        ? MokoSystemControl::parseWpctlVolume(inputLevelText)
        : MokoSystemControl::AudioLevel{};
    m_audioAvailable = statusOk || outputLevel.valid || inputLevel.valid;
    if (outputLevel.valid) {
        m_outputVolume = outputLevel.percent;
        m_outputMuted = outputLevel.muted;
    }
    if (inputLevel.valid) {
        m_inputVolume = inputLevel.percent;
        m_inputMuted = inputLevel.muted;
    }
    if (m_outputDeviceName.isEmpty() && m_audioAvailable)
        m_outputDeviceName = QStringLiteral("Default output");
    if (m_inputDeviceName.isEmpty() && m_audioAvailable)
        m_inputDeviceName = QStringLiteral("Default microphone");
    emit audioChanged();
}

bool SystemControl::validAudioDevice(int id, const QVariantList &devices) const
{
    return std::any_of(devices.cbegin(), devices.cend(), [id](const QVariant &entry) {
        return entry.toMap().value(QStringLiteral("id")).toInt() == id;
    });
}

bool SystemControl::setOutputVolume(int percent)
{
    if (!m_audioAvailable)
        return false;
    percent = std::clamp(percent, 0, 150);
    const QString volume = QLocale::c().toString(percent / 100.0, 'f', 2);
    if (!runWpctl({QStringLiteral("set-volume"),
                   QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                   volume})) {
        setOperationMessage(QStringLiteral("Could not change output volume."));
        return false;
    }
    m_outputVolume = percent;
    writeLiveEvent(QStringLiteral("MOKO_CONTROL_ACTION action=output_volume value=%1 ok=1 uid=%2")
                       .arg(percent)
                       .arg(static_cast<qulonglong>(geteuid())));
    emit audioChanged();
    return true;
}

bool SystemControl::setOutputMuted(bool muted)
{
    if (!m_audioAvailable
        || !runWpctl({QStringLiteral("set-mute"),
                      QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                      muted ? QStringLiteral("1") : QStringLiteral("0")})) {
        setOperationMessage(QStringLiteral("Could not change output mute."));
        return false;
    }
    m_outputMuted = muted;
    writeLiveEvent(QStringLiteral("MOKO_CONTROL_ACTION action=output_mute value=%1 ok=1 uid=%2")
                       .arg(muted ? 1 : 0)
                       .arg(static_cast<qulonglong>(geteuid())));
    emit audioChanged();
    return true;
}

bool SystemControl::setOutputDevice(int id)
{
    if (!validAudioDevice(id, m_outputDevices)
        || !runWpctl({QStringLiteral("set-default"), QString::number(id)})) {
        setOperationMessage(QStringLiteral("Could not select that output device."));
        return false;
    }
    setOperationMessage(QStringLiteral("Audio output changed"));
    writeLiveEvent(QStringLiteral("MOKO_CONTROL_ACTION action=output_device ok=1 uid=%1")
                       .arg(static_cast<qulonglong>(geteuid())));
    QTimer::singleShot(150, this, &SystemControl::refreshAudio);
    return true;
}

bool SystemControl::setInputVolume(int percent)
{
    if (!m_audioAvailable)
        return false;
    percent = std::clamp(percent, 0, 150);
    const QString volume = QLocale::c().toString(percent / 100.0, 'f', 2);
    if (!runWpctl({QStringLiteral("set-volume"),
                   QStringLiteral("@DEFAULT_AUDIO_SOURCE@"),
                   volume})) {
        setOperationMessage(QStringLiteral("Could not change microphone level."));
        return false;
    }
    m_inputVolume = percent;
    writeLiveEvent(QStringLiteral("MOKO_CONTROL_ACTION action=input_volume value=%1 ok=1 uid=%2")
                       .arg(percent)
                       .arg(static_cast<qulonglong>(geteuid())));
    emit audioChanged();
    return true;
}

bool SystemControl::setInputMuted(bool muted)
{
    if (!m_audioAvailable
        || !runWpctl({QStringLiteral("set-mute"),
                      QStringLiteral("@DEFAULT_AUDIO_SOURCE@"),
                      muted ? QStringLiteral("1") : QStringLiteral("0")})) {
        setOperationMessage(QStringLiteral("Could not change microphone mute."));
        return false;
    }
    m_inputMuted = muted;
    writeLiveEvent(QStringLiteral("MOKO_CONTROL_ACTION action=input_mute value=%1 ok=1 uid=%2")
                       .arg(muted ? 1 : 0)
                       .arg(static_cast<qulonglong>(geteuid())));
    emit audioChanged();
    return true;
}

bool SystemControl::setInputDevice(int id)
{
    if (!validAudioDevice(id, m_inputDevices)
        || !runWpctl({QStringLiteral("set-default"), QString::number(id)})) {
        setOperationMessage(QStringLiteral("Could not select that microphone."));
        return false;
    }
    setOperationMessage(QStringLiteral("Microphone changed"));
    QTimer::singleShot(150, this, &SystemControl::refreshAudio);
    return true;
}

void SystemControl::refreshPower()
{
    const QString sysfsRoot = qEnvironmentVariable("MOKO_SYSFS_ROOT", QStringLiteral("/sys"));
    const QDir backlights(QDir(sysfsRoot).filePath(QStringLiteral("class/backlight")));
    m_backlightDevice.clear();
    m_brightnessAvailable = false;
    m_brightness = 0;
    for (const QString &entry : backlights.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = backlights.filePath(entry);
        const qint64 maximum = readIntegerFile(base + QStringLiteral("/max_brightness"));
        const qint64 current = readIntegerFile(base + QStringLiteral("/actual_brightness"));
        const qint64 fallback = readIntegerFile(base + QStringLiteral("/brightness"));
        const int value = MokoSystemControl::percentage(current >= 0 ? current : fallback, maximum);
        if (value < 0)
            continue;
        m_backlightDevice = entry;
        m_brightness = value;
        const bool writable = QFileInfo(base + QStringLiteral("/brightness")).isWritable();
        m_brightnessAvailable = writable
            || !executableFromEnvironment("MOKO_BRIGHTNESSCTL", QStringLiteral("brightnessctl")).isEmpty();
        break;
    }

    m_batteryAvailable = false;
    m_batteryPercent = -1;
    m_batteryHealth = -1;
    m_batteryState = QStringLiteral("Not detected");
    m_batteryTime.clear();
    m_batteryTechnology.clear();
    m_batteryCycleCount.clear();
    m_batteryEnergy.clear();
    m_externalPowerConnected = false;
    m_batteryCharging = false;
    const QDir supplies(QDir(sysfsRoot).filePath(QStringLiteral("class/power_supply")));
    const QStringList supplyEntries = supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : supplyEntries) {
        const QString base = supplies.filePath(entry);
        const QString type = readTextFile(base + QStringLiteral("/type"));
        if (type == QStringLiteral("Mains") || type == QStringLiteral("USB")
            || type == QStringLiteral("USB_C") || type == QStringLiteral("Wireless")) {
            m_externalPowerConnected = readIntegerFile(base + QStringLiteral("/online")) > 0;
            if (m_externalPowerConnected)
                break;
        }
    }
    for (const QString &entry : supplyEntries) {
        const QString base = supplies.filePath(entry);
        if (readTextFile(base + QStringLiteral("/type")) != QStringLiteral("Battery"))
            continue;
        m_batteryAvailable = true;
        m_batteryPercent = static_cast<int>(readIntegerFile(base + QStringLiteral("/capacity")));
        m_batteryState = readTextFile(base + QStringLiteral("/status"));
        if (m_batteryState.isEmpty())
            m_batteryState = QStringLiteral("Unknown");
        m_batteryCharging = m_batteryState.compare(QStringLiteral("Charging"),
                                                   Qt::CaseInsensitive) == 0;
        m_externalPowerConnected = m_externalPowerConnected || m_batteryCharging;
        qint64 full = readIntegerFile(base + QStringLiteral("/energy_full"));
        qint64 design = readIntegerFile(base + QStringLiteral("/energy_full_design"));
        qint64 remaining = readIntegerFile(base + QStringLiteral("/energy_now"));
        qint64 rate = readIntegerFile(base + QStringLiteral("/power_now"));
        if (full < 0 || design <= 0) {
            full = readIntegerFile(base + QStringLiteral("/charge_full"));
            design = readIntegerFile(base + QStringLiteral("/charge_full_design"));
            remaining = readIntegerFile(base + QStringLiteral("/charge_now"));
            rate = readIntegerFile(base + QStringLiteral("/current_now"));
        }
        m_batteryHealth = MokoSystemControl::percentage(full, design);
        m_batteryTechnology = readTextFile(base + QStringLiteral("/technology"));
        const qint64 cycles = readIntegerFile(base + QStringLiteral("/cycle_count"));
        if (cycles >= 0)
            m_batteryCycleCount = QString::number(cycles);
        if (remaining >= 0 && full > 0) {
            const bool energyUnits = QFileInfo::exists(base + QStringLiteral("/energy_now"));
            const double divisor = energyUnits ? 1000000.0 : 1000.0;
            const QString unit = energyUnits ? QStringLiteral("Wh") : QStringLiteral("mAh");
            m_batteryEnergy = QStringLiteral("%1 / %2 %3")
                                  .arg(remaining / divisor, 0, 'f', 1)
                                  .arg(full / divisor, 0, 'f', 1)
                                  .arg(unit);
        }
        if (remaining > 0 && rate > 0) {
            const qint64 relevant = m_batteryState.compare(QStringLiteral("Charging"),
                                                           Qt::CaseInsensitive) == 0
                ? std::max<qint64>(0, full - remaining)
                : remaining;
            m_batteryTime = formatDuration(relevant * 3600 / rate);
        }
        break;
    }

    m_powerModeAvailable = false;
    m_powerMode.clear();
    m_powerModes.clear();
    const struct {
        const char *service;
        const char *path;
        const char *interface;
    } profileServices[] = {
        {"org.freedesktop.UPower.PowerProfiles", "/org/freedesktop/UPower/PowerProfiles",
         "org.freedesktop.UPower.PowerProfiles"},
        {"net.hadess.PowerProfiles", "/net/hadess/PowerProfiles", "net.hadess.PowerProfiles"},
    };
    for (const auto &profileService : profileServices) {
        if (!serviceRegistered(QString::fromLatin1(profileService.service)))
            continue;
        const QVariant active = dbusProperty(QString::fromLatin1(profileService.service),
                                             QString::fromLatin1(profileService.path),
                                             QString::fromLatin1(profileService.interface),
                                             QStringLiteral("ActiveProfile"));
        const QVariant profilesValue = dbusProperty(QString::fromLatin1(profileService.service),
                                                    QString::fromLatin1(profileService.path),
                                                    QString::fromLatin1(profileService.interface),
                                                    QStringLiteral("Profiles"));
        const VariantMapList profiles = qdbus_cast<VariantMapList>(profilesValue);
        for (const QVariantMap &profile : profiles) {
            const QString name = profile.value(QStringLiteral("Profile")).toString();
            if (!name.isEmpty() && !m_powerModes.contains(name))
                m_powerModes.append(name);
        }
        m_powerMode = active.toString();
        m_powerModeAvailable = !m_powerMode.isEmpty() && !m_powerModes.isEmpty();
        break;
    }

    QDBusInterface loginManager(logindService,
                                logindPath,
                                logindInterface,
                                QDBusConnection::systemBus());
    const QDBusReply<QString> canSuspend = loginManager.call(QStringLiteral("CanSuspend"));
    const QString suspendPolicy = canSuspend.isValid() ? canSuspend.value() : QString();
    m_suspendAvailable = suspendPolicy == QStringLiteral("yes")
        || suspendPolicy == QStringLiteral("challenge");
    emit powerChanged();
}

void SystemControl::refreshTime()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QLocale locale = QLocale::system();
    const QSettings sharedSettings(QStringLiteral("MOKO"), QStringLiteral("MOKO OS"));
    const bool useTwentyFourHour = sharedSettings.value(
        QStringLiteral("dateTime/twentyFourHour"), false).toBool();
    const QString configuredZone = sharedSettings.value(
        QStringLiteral("dateTime/timeZone")).toString();
    const QByteArray zoneIdBytes = QTimeZone::isTimeZoneIdAvailable(configuredZone.toUtf8())
        ? configuredZone.toUtf8() : QTimeZone::systemTimeZoneId();
    const QTimeZone timeZone(zoneIdBytes);
    const QDateTime localNow = now.toUTC().toTimeZone(timeZone);
    const QString clock = useTwentyFourHour
        ? localNow.toString(QStringLiteral("HH:mm"))
        : locale.toString(localNow.time(), QLocale::ShortFormat);
    const QString zoneId = QString::fromUtf8(zoneIdBytes);
    const QString zoneName = zoneId.isEmpty() ? localNow.timeZoneAbbreviation() : zoneId;
    const QString date = QStringLiteral("%1\n%2 (%3)")
                             .arg(locale.toString(localNow.date(), QLocale::LongFormat),
                                  clock,
                                  zoneName);
    if (m_clockText == clock && m_dateText == date && m_timeZoneName == zoneName)
        return;
    m_clockText = clock;
    m_dateText = date;
    m_timeZoneName = zoneName;
    emit timeChanged();
}

bool SystemControl::setBrightness(int percent)
{
    if (!m_brightnessAvailable || m_backlightDevice.isEmpty())
        return false;
    percent = std::clamp(percent, 1, 100);
    const QString program = executableFromEnvironment("MOKO_BRIGHTNESSCTL",
                                                       QStringLiteral("brightnessctl"));
    bool changed = false;
    if (!program.isEmpty()) {
        QProcess process;
        process.start(program,
                      {QStringLiteral("-d"), m_backlightDevice, QStringLiteral("set"),
                       QStringLiteral("%1%").arg(percent)});
        changed = process.waitForStarted(1000) && process.waitForFinished(2000)
            && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }
    if (!changed) {
        const QString sysfsRoot = qEnvironmentVariable("MOKO_SYSFS_ROOT", QStringLiteral("/sys"));
        const QString base = QDir(sysfsRoot).filePath(
            QStringLiteral("class/backlight/%1").arg(m_backlightDevice));
        const qint64 maximum = readIntegerFile(base + QStringLiteral("/max_brightness"));
        QFile file(base + QStringLiteral("/brightness"));
        if (maximum > 0 && file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            const qint64 rawValue = std::max<qint64>(1, maximum * percent / 100);
            changed = file.write(QByteArray::number(rawValue)) > 0;
        }
    }
    if (!changed) {
        setOperationMessage(QStringLiteral("Brightness control is not writable for this session."));
        return false;
    }
    m_brightness = percent;
    writeLiveEvent(QStringLiteral("MOKO_CONTROL_ACTION action=brightness value=%1 ok=1 uid=%2")
                       .arg(percent)
                       .arg(static_cast<qulonglong>(geteuid())));
    emit powerChanged();
    QTimer::singleShot(150, this, &SystemControl::refreshPower);
    return true;
}

bool SystemControl::adjustBrightness(int delta)
{
    return setBrightness(m_brightness + delta);
}

bool SystemControl::setPowerMode(const QString &mode)
{
    if (!m_powerModeAvailable || !m_powerModes.contains(mode))
        return false;
    const struct {
        const char *service;
        const char *path;
        const char *interface;
    } profileServices[] = {
        {"org.freedesktop.UPower.PowerProfiles", "/org/freedesktop/UPower/PowerProfiles",
         "org.freedesktop.UPower.PowerProfiles"},
        {"net.hadess.PowerProfiles", "/net/hadess/PowerProfiles", "net.hadess.PowerProfiles"},
    };
    QString error;
    for (const auto &profileService : profileServices) {
        if (!serviceRegistered(QString::fromLatin1(profileService.service)))
            continue;
        if (!setDbusProperty(QString::fromLatin1(profileService.service),
                             QString::fromLatin1(profileService.path),
                             QString::fromLatin1(profileService.interface),
                             QStringLiteral("ActiveProfile"),
                             mode,
                             &error)) {
            setOperationMessage(QStringLiteral("Could not change power mode: %1").arg(error));
            return false;
        }
        m_powerMode = mode;
        setOperationMessage(QStringLiteral("Power mode changed to %1").arg(mode));
        writeLiveEvent(QStringLiteral("MOKO_CONTROL_ACTION action=power_mode value=%1 ok=1 uid=%2")
                           .arg(mode)
                           .arg(static_cast<qulonglong>(geteuid())));
        emit powerChanged();
        return true;
    }
    return false;
}

bool SystemControl::suspend()
{
    if (!m_suspendAvailable || m_suspendPending)
        return false;

    QDBusInterface loginManager(logindService,
                                logindPath,
                                logindInterface,
                                QDBusConnection::systemBus());
    if (!loginManager.isValid()) {
        setOperationMessage(QStringLiteral("Suspend is unavailable."));
        return false;
    }

    m_suspendPending = true;
    emit powerChanged();
    setOperationMessage(QStringLiteral("Preparing to suspend"));
    writeLiveEvent(QStringLiteral("MOKO_CONTROL_ACTION action=suspend state=requested uid=%1")
                       .arg(static_cast<qulonglong>(geteuid())));

    auto *watcher = new QDBusPendingCallWatcher(
        loginManager.asyncCall(QStringLiteral("Suspend"), false), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *finished) {
                const QDBusPendingReply<> reply = *finished;
                m_suspendPending = false;
                emit powerChanged();
                if (reply.isError()) {
                    setOperationMessage(QStringLiteral("Could not suspend: %1")
                                            .arg(reply.error().message()));
                    writeLiveEvent(QStringLiteral(
                                       "MOKO_CONTROL_ACTION action=suspend state=failed uid=%1")
                                       .arg(static_cast<qulonglong>(geteuid())));
                } else {
                    setOperationMessage(QStringLiteral("System resumed"));
                    writeLiveEvent(QStringLiteral(
                                       "MOKO_CONTROL_ACTION action=suspend state=accepted uid=%1")
                                       .arg(static_cast<qulonglong>(geteuid())));
                }
                finished->deleteLater();
            });
    return true;
}

void SystemControl::reportControlCenterOpened(int page)
{
    if (page < 0 || page > 4)
        return;
    writeLiveEvent(QStringLiteral(
                       "MOKO_CONTROL_CENTER state=open page=%1 network_manager=%2 wifi_device=%3 "
                       "bluez_service=%4 bluetooth_adapter=%5 audio=%6 brightness=%7 battery=%8 "
                       "power_mode=%9 uid=%10")
                       .arg(page)
                       .arg(m_networkManagerAvailable ? 1 : 0)
                       .arg(m_wifiAvailable ? 1 : 0)
                       .arg(m_bluezServiceAvailable ? 1 : 0)
                       .arg(m_bluetoothAvailable ? 1 : 0)
                       .arg(m_audioAvailable ? 1 : 0)
                       .arg(m_brightnessAvailable ? 1 : 0)
                       .arg(m_batteryAvailable ? 1 : 0)
                       .arg(m_powerModeAvailable ? 1 : 0)
                       .arg(static_cast<qulonglong>(geteuid())));
}

void SystemControl::setOperationMessage(const QString &message)
{
    if (m_operationMessage == message)
        return;
    m_operationMessage = message;
    emit operationMessageChanged();
}
