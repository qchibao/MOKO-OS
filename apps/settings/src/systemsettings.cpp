#include "systemsettings.h"
#include "livemarker.h"

#include <QDateTime>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLocale>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
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
        return QStringLiteral("Not detected");
    }
}

QString suspendStateName(const QString &state)
{
    if (state == QStringLiteral("yes"))
        return QStringLiteral("Available");
    if (state == QStringLiteral("challenge"))
        return QStringLiteral("Authorization required");
    if (state == QStringLiteral("no") || state == QStringLiteral("na"))
        return QStringLiteral("Unavailable");
    return QStringLiteral("Not detected");
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
    QSettings settings;
    m_developerMode = settings.value(QStringLiteral("developerMode"), false).toBool();
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

bool SystemSettings::developerMode() const
{
    return m_developerMode;
}

void SystemSettings::setDeveloperMode(bool enabled)
{
    if (m_developerMode == enabled)
        return;
    m_developerMode = enabled;
    QSettings().setValue(QStringLiteral("developerMode"), enabled);
    emit developerModeChanged();
    emit dataChanged();
}

QStringList SystemSettings::sectionIds() const
{
    return {QStringLiteral("about"),      QStringLiteral("display"),
            QStringLiteral("appearance"), QStringLiteral("sound"),
            QStringLiteral("network"),    QStringLiteral("bluetooth"),
            QStringLiteral("power"),      QStringLiteral("storage"),
            QStringLiteral("hardware"),   QStringLiteral("system")};
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
        {QStringLiteral("hardware"), QStringLiteral("Hardware Diagnostics")},
        {QStringLiteral("system"), QStringLiteral("Advanced Technical Information")},
    };
    return titles.value(sectionId, sectionId);
}

QString SystemSettings::sectionDescription(const QString &sectionId) const
{
    static const QHash<QString, QString> descriptions = {
        {QStringLiteral("about"), QStringLiteral("MOKO OS and device identity")},
        {QStringLiteral("display"), QStringLiteral("Connected displays and interface scale")},
        {QStringLiteral("appearance"), QStringLiteral("MOKO color and interface style")},
        {QStringLiteral("sound"), QStringLiteral("Audio availability")},
        {QStringLiteral("network"), QStringLiteral("Connection and Wi-Fi status")},
        {QStringLiteral("bluetooth"), QStringLiteral("Bluetooth availability")},
        {QStringLiteral("power"), QStringLiteral("Battery and suspend capability")},
        {QStringLiteral("storage"), QStringLiteral("Mounted filesystems and free space")},
        {QStringLiteral("hardware"), QStringLiteral("Read-only compatibility report and export")},
        {QStringLiteral("system"), QStringLiteral("Kernel and runtime details for troubleshooting")},
    };
    return descriptions.value(sectionId);
}

QVariantList SystemSettings::rows(const QString &sectionId) const
{
    if (m_developerMode)
        return m_rows.value(sectionId);

    Rows visibleRows;
    for (const QVariant &rowValue : m_rows.value(sectionId)) {
        if (!rowValue.toMap().value(QStringLiteral("technical")).toBool())
            visibleRows.append(rowValue);
    }
    return visibleRows;
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
    collectHardwareDiagnostics();
    collectSystem();
    const QDateTime now = QDateTime::currentDateTime();
    m_refreshedAt = QStringLiteral("%1 %2")
                        .arg(QLocale::system().toString(now, QLocale::ShortFormat),
                             now.timeZoneAbbreviation());
    m_statusMessage = QStringLiteral("Live system data refreshed");
    emit dataChanged();
}

bool SystemSettings::openHardwareDiagnostics()
{
    QDBusInterface applications(QStringLiteral("org.moko.Applications1"),
                                QStringLiteral("/org/moko/Applications1"),
                                QStringLiteral("org.moko.Applications1"),
                                QDBusConnection::sessionBus());
    const QDBusReply<QVariantMap> reply = applications.call(
        QStringLiteral("openApplication"), QStringLiteral("org.moko.HardwareDiagnostics"));
    const bool launched = reply.isValid() && reply.value().value(QStringLiteral("ok")).toBool();
    m_statusMessage = launched ? QStringLiteral("Opening MOKO Hardware Diagnostics")
        : reply.isValid() ? reply.value().value(QStringLiteral("message")).toString()
                          : QStringLiteral("MOKO application service is unavailable");
    if (launched) {
        writeMokoLiveEvent(QStringLiteral("MOKO_SETTINGS_ACTION action=open_hardware_diagnostics state=accepted uid=%1")
                               .arg(geteuid()));
    }
    emit dataChanged();
    return launched;
}

QVariantMap SystemSettings::row(const QString &label,
                                const QString &value,
                                const QString &detail,
                                bool available,
                                bool writable,
                                bool technical)
{
    return {{QStringLiteral("label"), label},
            {QStringLiteral("value"), value},
            {QStringLiteral("detail"), detail},
            {QStringLiteral("available"), available},
            {QStringLiteral("writable"), writable},
            {QStringLiteral("technical"), technical}};
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
                  {row(QStringLiteral("Product"), QStringLiteral("MOKO OS v0.1.1 Hardware & Usability Preview")),
                   row(QStringLiteral("Linux base"), os.value(QStringLiteral("PRETTY_NAME"), QStringLiteral("Unknown")),
                       {}, true, false, true),
                   row(QStringLiteral("Device"), model),
                   row(QStringLiteral("Host name"), QSysInfo::machineHostName(), {}, true, false, true),
                   row(QStringLiteral("Architecture"), QSysInfo::currentCpuArchitecture(), {}, true, false, true)});
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
    result.append(row(QStringLiteral("Display backend"), QGuiApplication::platformName(),
                      QStringLiteral("Qt platform plugin"), true, false, true));
    m_rows.insert(QStringLiteral("display"), result);
}

void SystemSettings::collectAppearance()
{
    m_rows.insert(QStringLiteral("appearance"),
                  {row(QStringLiteral("Theme"), QStringLiteral("MOKO Light"),
                       QStringLiteral("Read-only in v0.1.1")),
                   row(QStringLiteral("Accent"), QStringLiteral("Blue"),
                       QStringLiteral("MOKO design token #3F7CFF; read-only")),
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
    const bool sessionReady = socketPresent && !status.isEmpty();
    m_rows.insert(QStringLiteral("sound"),
                  {row(QStringLiteral("Audio"), sessionReady ? QStringLiteral("Working")
                                                              : socketPresent ? QStringLiteral("Limited")
                                                                              : QStringLiteral("Not detected"),
                       QStringLiteral("PipeWire socket: %1")
                           .arg(QDir(runtimeDirectory).filePath(QStringLiteral("pipewire-0"))),
                       socketPresent),
                   row(QStringLiteral("Audio service details"),
                       status.isEmpty() ? QStringLiteral("Not detected") : QStringLiteral("Working"),
                       status.isEmpty() ? QStringLiteral("wpctl returned no data") : status,
                       !status.isEmpty(), false, true)});
}

void SystemSettings::collectNetwork()
{
    QDBusInterface manager(QStringLiteral("org.freedesktop.NetworkManager"),
                           QStringLiteral("/org/freedesktop/NetworkManager"),
                           QStringLiteral("org.freedesktop.NetworkManager"),
                           QDBusConnection::systemBus());
    if (!manager.isValid()) {
        m_rows.insert(QStringLiteral("network"),
                      {row(QStringLiteral("Connection"), QStringLiteral("Not detected"),
                           QStringLiteral("Network controls are unavailable"), false),
                       row(QStringLiteral("Connection service"), QStringLiteral("Not detected"),
                           manager.lastError().message(), false, false, true)});
        return;
    }

    const uint state = manager.property("State").toUInt();
    const bool wirelessEnabled = manager.property("WirelessEnabled").toBool();
    QDBusMessage devicesReply = manager.call(QStringLiteral("GetDevices"));
    int deviceCount = 0;
    if (devicesReply.type() == QDBusMessage::ReplyMessage && !devicesReply.arguments().isEmpty())
        deviceCount = qdbus_cast<QList<QDBusObjectPath>>(devicesReply.arguments().constFirst()).size();
    m_rows.insert(QStringLiteral("network"),
                  {row(QStringLiteral("Connection"), networkStateName(state),
                       QStringLiteral("Reported by NetworkManager over the system D-Bus")),
                   row(QStringLiteral("Wi-Fi"), wirelessEnabled ? QStringLiteral("On")
                                                                  : QStringLiteral("Off"),
                       QStringLiteral("Reported by NetworkManager; read-only")),
                   row(QStringLiteral("Network devices"), QString::number(deviceCount),
                       QStringLiteral("NetworkManager GetDevices"), true, false, true),
                   row(QStringLiteral("Connection service"), QStringLiteral("Working"),
                       QStringLiteral("org.freedesktop.NetworkManager on the system D-Bus"),
                       true, false, true)});
}

void SystemSettings::collectBluetooth()
{
    QDBusConnectionInterface *interface = QDBusConnection::systemBus().interface();
    const bool available = interface
        && interface->isServiceRegistered(QStringLiteral("org.bluez"));
    const QStringList controllers = QDir(QStringLiteral("/sys/class/bluetooth"))
                                        .entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    m_rows.insert(QStringLiteral("bluetooth"),
                  {row(QStringLiteral("Bluetooth"), available ? QStringLiteral("Working")
                                                               : QStringLiteral("Not detected"),
                       QStringLiteral("BlueZ service on the system D-Bus"), available),
                   row(QStringLiteral("Controllers"), controllers.isEmpty()
                                                           ? QStringLiteral("None detected")
                                                           : controllers.join(QStringLiteral(", ")),
                       QStringLiteral("Kernel Bluetooth class"), !controllers.isEmpty()),
                   row(QStringLiteral("Bluetooth service"),
                       available ? QStringLiteral("Connected") : QStringLiteral("Disconnected"),
                       QStringLiteral("org.bluez on the system D-Bus"), available, false, true)});
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
    result.append(row(QStringLiteral("Sleep"), suspendStateName(canSuspend),
                      QStringLiteral("systemd-logind CanSuspend returned '%1'").arg(canSuspend),
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

void SystemSettings::collectHardwareDiagnostics()
{
    const QString program = QStandardPaths::findExecutable(QStringLiteral("moko-hardware-diagnostics"));
    const bool available = !program.isEmpty();
    m_rows.insert(QStringLiteral("hardware"),
                  {row(QStringLiteral("Compatibility scanner"),
                       available ? QStringLiteral("Ready") : QStringLiteral("Not detected"),
                       available ? program : QStringLiteral("moko-hardware-diagnostics was not found"),
                       available),
                   row(QStringLiteral("Operations"), QStringLiteral("Read-only"),
                       QStringLiteral("No mounting, partitioning, formatting or firmware changes")),
                   row(QStringLiteral("Exports"),
                       QStringLiteral("moko-hardware-report.json / .txt"),
                       QStringLiteral("Serial numbers, MAC addresses, host names and personal files are excluded"))});
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
                                                          : QStringLiteral("Unknown"),
                       {}, unameOk, false, true),
                   row(QStringLiteral("Kernel name"), unameOk ? QString::fromLocal8Bit(kernelInfo.sysname)
                                                               : QStringLiteral("Unknown"),
                       {}, unameOk, false, true),
                   row(QStringLiteral("Machine"), unameOk ? QString::fromLocal8Bit(kernelInfo.machine)
                                                           : QSysInfo::currentCpuArchitecture(),
                       {}, true, false, true),
                   row(QStringLiteral("Total memory"), memoryBytes ? formatBytes(memoryBytes)
                                                                   : QStringLiteral("Unknown"),
                       QStringLiteral("/proc/meminfo"), memoryBytes > 0),
                   row(QStringLiteral("Qt version"), QString::fromLatin1(qVersion()),
                       {}, true, false, true),
                   row(QStringLiteral("Session"), qEnvironmentVariable("XDG_SESSION_TYPE", QStringLiteral("Unknown")),
                       {}, true, false, true) });
}
