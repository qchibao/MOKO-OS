#include "systemsettings.h"

#include <QDateTime>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QStorageInfo>
#include <QSysInfo>

#include <sys/utsname.h>
#include <unistd.h>

namespace {

QString networkStateName(uint state)
{
    switch (state) {
    case 10:
        return QStringLiteral("Asleep");
    case 20:
        return QStringLiteral("Disconnected");
    case 30:
        return QStringLiteral("Disconnecting");
    case 40:
        return QStringLiteral("Connecting");
    case 50:
        return QStringLiteral("Connected locally");
    case 60:
        return QStringLiteral("Connected to site");
    case 70:
        return QStringLiteral("Connected");
    default:
        return QStringLiteral("Unknown (%1)").arg(state);
    }
}

QString firstNonEmpty(const QStringList &values, const QString &fallback = QStringLiteral("Unknown"))
{
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty())
            return value.trimmed();
    }
    return fallback;
}

} // namespace

SystemSettings::SystemSettings(QObject *parent)
    : QObject(parent)
{
    refresh();
}

QString SystemSettings::refreshedAt() const
{
    return m_refreshedAt;
}

QString SystemSettings::statusMessage() const
{
    return m_statusMessage;
}

QStringList SystemSettings::sectionIds() const
{
    return {QStringLiteral("about"),      QStringLiteral("display"),
            QStringLiteral("appearance"), QStringLiteral("sound"),
            QStringLiteral("network"),    QStringLiteral("bluetooth"),
            QStringLiteral("power"),      QStringLiteral("storage"),
            QStringLiteral("system")};
}

QString SystemSettings::sectionTitle(const QString &sectionId) const
{
    static const QHash<QString, QString> titles = {
        {QStringLiteral("about"), QStringLiteral("About")},
        {QStringLiteral("display"), QStringLiteral("Display")},
        {QStringLiteral("appearance"), QStringLiteral("Appearance")},
        {QStringLiteral("sound"), QStringLiteral("Sound")},
        {QStringLiteral("network"), QStringLiteral("Network")},
        {QStringLiteral("bluetooth"), QStringLiteral("Bluetooth")},
        {QStringLiteral("power"), QStringLiteral("Power")},
        {QStringLiteral("storage"), QStringLiteral("Storage")},
        {QStringLiteral("system"), QStringLiteral("System Information")},
    };
    return titles.value(sectionId, sectionId);
}

QString SystemSettings::sectionDescription(const QString &sectionId) const
{
    static const QHash<QString, QString> descriptions = {
        {QStringLiteral("about"), QStringLiteral("MOKO OS and device identity")},
        {QStringLiteral("display"), QStringLiteral("Active Wayland display information")},
        {QStringLiteral("appearance"), QStringLiteral("Current MOKO visual configuration")},
        {QStringLiteral("sound"), QStringLiteral("PipeWire and audio service state")},
        {QStringLiteral("network"), QStringLiteral("NetworkManager connectivity")},
        {QStringLiteral("bluetooth"), QStringLiteral("BlueZ controller service")},
        {QStringLiteral("power"), QStringLiteral("Battery and suspend capability")},
        {QStringLiteral("storage"), QStringLiteral("Mounted filesystems and free space")},
        {QStringLiteral("system"), QStringLiteral("Kernel, architecture and runtime details")},
    };
    return descriptions.value(sectionId);
}

QVariantList SystemSettings::rows(const QString &sectionId) const
{
    return m_rows.value(sectionId);
}

void SystemSettings::refresh()
{
    m_rows.clear();
    collectAbout();
    collectDisplay();
    collectAppearance();
    collectSound();
    collectNetwork();
    collectBluetooth();
    collectPower();
    collectStorage();
    collectSystem();
    m_refreshedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_statusMessage = QStringLiteral("Live system data refreshed");
    emit dataChanged();
}

QVariantMap SystemSettings::row(const QString &label,
                                const QString &value,
                                const QString &detail,
                                bool available,
                                bool writable)
{
    return {{QStringLiteral("label"), label},
            {QStringLiteral("value"), value},
            {QStringLiteral("detail"), detail},
            {QStringLiteral("available"), available},
            {QStringLiteral("writable"), writable}};
}

QString SystemSettings::readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QMap<QString, QString> SystemSettings::readKeyValueFile(const QString &path)
{
    QMap<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return values;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        const qsizetype separator = line.indexOf(u'=');
        if (separator <= 0)
            continue;
        QString value = line.mid(separator + 1).trimmed();
        if (value.size() >= 2 && value.startsWith(u'"') && value.endsWith(u'"'))
            value = value.mid(1, value.size() - 2);
        values.insert(line.left(separator), value);
    }
    return values;
}

QString SystemSettings::formatBytes(quint64 bytes)
{
    static const QStringList units = {QStringLiteral("B"), QStringLiteral("KiB"),
                                      QStringLiteral("MiB"), QStringLiteral("GiB"),
                                      QStringLiteral("TiB")};
    double value = bytes;
    int unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        ++unit;
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', unit > 1 ? 1 : 0).arg(units.at(unit));
}

QString SystemSettings::processOutput(const QString &program,
                                      const QStringList &arguments,
                                      int timeoutMs)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(timeoutMs) || !process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished();
        return {};
    }
    return QString::fromUtf8(process.readAll()).trimmed();
}

void SystemSettings::collectAbout()
{
    const auto os = readKeyValueFile(QStringLiteral("/etc/os-release"));
    const QString model = firstNonEmpty({readTextFile(QStringLiteral("/sys/devices/virtual/dmi/id/product_name")),
                                         readTextFile(QStringLiteral("/sys/firmware/devicetree/base/model"))});
    m_rows.insert(QStringLiteral("about"),
                  {row(QStringLiteral("Product"), QStringLiteral("MOKO OS v0.1 Developer Preview")),
                   row(QStringLiteral("Linux base"), os.value(QStringLiteral("PRETTY_NAME"), QStringLiteral("Unknown"))),
                   row(QStringLiteral("Device"), model),
                   row(QStringLiteral("Host name"), QSysInfo::machineHostName()),
                   row(QStringLiteral("Architecture"), QSysInfo::currentCpuArchitecture())});
}

void SystemSettings::collectDisplay()
{
    Rows result;
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        result.append(row(QStringLiteral("Displays"), QStringLiteral("Unavailable"),
                          QStringLiteral("No active Qt screen"), false));
    }
    for (int index = 0; index < screens.size(); ++index) {
        const QScreen *screen = screens.at(index);
        const QSize size = screen->size();
        result.append(row(QStringLiteral("Display %1").arg(index + 1),
                          QStringLiteral("%1 x %2 @ %3 Hz")
                              .arg(size.width())
                              .arg(size.height())
                              .arg(screen->refreshRate(), 0, 'f', 1),
                          firstNonEmpty({screen->manufacturer(), screen->model(), screen->name()})));
        result.append(row(QStringLiteral("Scale %1").arg(index + 1),
                          QString::number(screen->devicePixelRatio(), 'f', 2),
                          QStringLiteral("Read-only in v0.1")));
    }
    result.append(row(QStringLiteral("Platform"), QGuiApplication::platformName(),
                      QStringLiteral("Qt display backend")));
    m_rows.insert(QStringLiteral("display"), result);
}

void SystemSettings::collectAppearance()
{
    m_rows.insert(QStringLiteral("appearance"),
                  {row(QStringLiteral("Theme"), QStringLiteral("MOKO Light"),
                       QStringLiteral("Read-only in v0.1")),
                   row(QStringLiteral("Accent"), QStringLiteral("#3F7CFF"),
                       QStringLiteral("MOKO design token; read-only")),
                   row(QStringLiteral("Interface style"), QStringLiteral("Bright glass / ice"),
                       QStringLiteral("Current shell design language"))});
}

void SystemSettings::collectSound()
{
    const QString runtimeDirectory = qEnvironmentVariable(
        "XDG_RUNTIME_DIR", QStringLiteral("/run/user/%1").arg(geteuid()));
    const bool socketPresent = QFile::exists(QDir(runtimeDirectory).filePath(QStringLiteral("pipewire-0")));
    QString status = processOutput(QStringLiteral("wpctl"), {QStringLiteral("status"), QStringLiteral("--name")});
    if (status.size() > 1200)
        status = status.left(1200) + QStringLiteral("...");
    m_rows.insert(QStringLiteral("sound"),
                  {row(QStringLiteral("PipeWire socket"), socketPresent ? QStringLiteral("Available")
                                                                       : QStringLiteral("Unavailable"),
                       QDir(runtimeDirectory).filePath(QStringLiteral("pipewire-0")), socketPresent),
                   row(QStringLiteral("WirePlumber graph"),
                       status.isEmpty() ? QStringLiteral("Unavailable") : QStringLiteral("Detected"),
                       status.isEmpty() ? QStringLiteral("wpctl returned no data") : status,
                       !status.isEmpty())});
}

void SystemSettings::collectNetwork()
{
    QDBusInterface manager(QStringLiteral("org.freedesktop.NetworkManager"),
                           QStringLiteral("/org/freedesktop/NetworkManager"),
                           QStringLiteral("org.freedesktop.NetworkManager"),
                           QDBusConnection::systemBus());
    if (!manager.isValid()) {
        m_rows.insert(QStringLiteral("network"),
                      {row(QStringLiteral("NetworkManager"), QStringLiteral("Unavailable"),
                           manager.lastError().message(), false)});
        return;
    }

    const uint state = manager.property("State").toUInt();
    const bool wirelessEnabled = manager.property("WirelessEnabled").toBool();
    QDBusMessage devicesReply = manager.call(QStringLiteral("GetDevices"));
    int deviceCount = 0;
    if (devicesReply.type() == QDBusMessage::ReplyMessage && !devicesReply.arguments().isEmpty())
        deviceCount = qdbus_cast<QList<QDBusObjectPath>>(devicesReply.arguments().constFirst()).size();
    m_rows.insert(QStringLiteral("network"),
                  {row(QStringLiteral("NetworkManager"), QStringLiteral("Connected to D-Bus")),
                   row(QStringLiteral("Connectivity"), networkStateName(state)),
                   row(QStringLiteral("Network devices"), QString::number(deviceCount)),
                   row(QStringLiteral("Wi-Fi radio"), wirelessEnabled ? QStringLiteral("Enabled")
                                                                         : QStringLiteral("Disabled"),
                       QStringLiteral("Reported by NetworkManager; read-only"))});
}

void SystemSettings::collectBluetooth()
{
    QDBusConnectionInterface *interface = QDBusConnection::systemBus().interface();
    const bool available = interface
        && interface->isServiceRegistered(QStringLiteral("org.bluez"));
    const QStringList controllers = QDir(QStringLiteral("/sys/class/bluetooth"))
                                        .entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    m_rows.insert(QStringLiteral("bluetooth"),
                  {row(QStringLiteral("BlueZ service"), available ? QStringLiteral("Running")
                                                                   : QStringLiteral("Unavailable"),
                       QStringLiteral("System D-Bus service org.bluez"), available),
                   row(QStringLiteral("Controllers"), controllers.isEmpty()
                                                           ? QStringLiteral("None detected")
                                                           : controllers.join(QStringLiteral(", ")),
                       QStringLiteral("Kernel bluetooth class"), !controllers.isEmpty())});
}

void SystemSettings::collectPower()
{
    Rows result;
    const QDir supplies(QStringLiteral("/sys/class/power_supply"));
    const QStringList entries = supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    bool batteryFound = false;
    for (const QString &entryName : entries) {
        const QString base = supplies.filePath(entryName);
        if (readTextFile(base + QStringLiteral("/type")) != QStringLiteral("Battery"))
            continue;
        batteryFound = true;
        const QString capacity = readTextFile(base + QStringLiteral("/capacity"));
        const QString status = readTextFile(base + QStringLiteral("/status"));
        result.append(row(QStringLiteral("Battery %1").arg(entryName),
                          capacity.isEmpty() ? status : capacity + QStringLiteral("% - ") + status));
    }
    if (!batteryFound)
        result.append(row(QStringLiteral("Battery"), QStringLiteral("Not detected"),
                          QStringLiteral("No battery in /sys/class/power_supply"), false));

    QDBusInterface login(QStringLiteral("org.freedesktop.login1"),
                         QStringLiteral("/org/freedesktop/login1"),
                         QStringLiteral("org.freedesktop.login1.Manager"),
                         QDBusConnection::systemBus());
    QString canSuspend = QStringLiteral("Unavailable");
    if (login.isValid()) {
        const QDBusMessage reply = login.call(QStringLiteral("CanSuspend"));
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
            canSuspend = reply.arguments().constFirst().toString();
    }
    result.append(row(QStringLiteral("Suspend capability"), canSuspend,
                      QStringLiteral("systemd-logind; no action exposed here"),
                      canSuspend != QStringLiteral("Unavailable")));
    m_rows.insert(QStringLiteral("power"), result);
}

void SystemSettings::collectStorage()
{
    Rows result;
    QSet<QByteArray> seenDevices;
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady() || storage.rootPath().startsWith(QStringLiteral("/snap/")))
            continue;
        const QByteArray key = storage.device() + '\0' + storage.rootPath().toUtf8();
        if (seenDevices.contains(key))
            continue;
        seenDevices.insert(key);
        result.append(row(storage.displayName().isEmpty() ? storage.rootPath() : storage.displayName(),
                          QStringLiteral("%1 free of %2")
                              .arg(formatBytes(storage.bytesAvailable()), formatBytes(storage.bytesTotal())),
                          QStringLiteral("%1 - %2")
                              .arg(QString::fromUtf8(storage.fileSystemType()), storage.rootPath()),
                          true));
    }
    if (result.isEmpty())
        result.append(row(QStringLiteral("Mounted storage"), QStringLiteral("Unavailable"), {}, false));
    m_rows.insert(QStringLiteral("storage"), result);
}

void SystemSettings::collectSystem()
{
    struct utsname kernelInfo {};
    const bool unameOk = uname(&kernelInfo) == 0;
    const QString memInfo = readTextFile(QStringLiteral("/proc/meminfo"));
    const QRegularExpressionMatch memoryMatch =
        QRegularExpression(QStringLiteral("^MemTotal:\\s+(\\d+) kB"),
                           QRegularExpression::MultilineOption)
            .match(memInfo);
    const quint64 memoryBytes = memoryMatch.hasMatch()
        ? memoryMatch.captured(1).toULongLong() * 1024ULL
        : 0;
    m_rows.insert(QStringLiteral("system"),
                  {row(QStringLiteral("Kernel"), unameOk ? QString::fromLocal8Bit(kernelInfo.release)
                                                          : QStringLiteral("Unknown")),
                   row(QStringLiteral("Kernel name"), unameOk ? QString::fromLocal8Bit(kernelInfo.sysname)
                                                               : QStringLiteral("Unknown")),
                   row(QStringLiteral("Machine"), unameOk ? QString::fromLocal8Bit(kernelInfo.machine)
                                                           : QSysInfo::currentCpuArchitecture()),
                   row(QStringLiteral("Total memory"), memoryBytes ? formatBytes(memoryBytes)
                                                                   : QStringLiteral("Unknown"),
                       QStringLiteral("/proc/meminfo"), memoryBytes > 0),
                   row(QStringLiteral("Qt version"), QString::fromLatin1(qVersion())),
                   row(QStringLiteral("Session"), qEnvironmentVariable("XDG_SESSION_TYPE", QStringLiteral("Unknown"))) });
}
