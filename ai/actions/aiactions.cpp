#include "aiactions.h"

#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>

#include <sys/utsname.h>
#include <unistd.h>

namespace {

QString networkStateName(uint state)
{
    switch (state) {
    case 10:
        return QStringLiteral("asleep");
    case 20:
        return QStringLiteral("disconnected");
    case 30:
        return QStringLiteral("disconnecting");
    case 40:
        return QStringLiteral("connecting");
    case 50:
        return QStringLiteral("connected-local");
    case 60:
        return QStringLiteral("connected-site");
    case 70:
        return QStringLiteral("connected");
    default:
        return QStringLiteral("unknown");
    }
}

QString sanitizedEventValue(QString value)
{
    for (QChar &character : value) {
        if (!character.isLetterOrNumber() && character != u'.' && character != u'-'
            && character != u'_' && character != u'/') {
            character = u'_';
        }
    }
    return value.left(180);
}

} // namespace

AiActions::AiActions(QString homeDirectory,
                     ApplicationLauncher applicationLauncher,
                     FileLauncher fileLauncher)
    : m_homeDirectory(homeDirectory.isEmpty() ? QDir::homePath() : std::move(homeDirectory))
    , m_allowedApplications({QStringLiteral("org.moko.Files"),
                             QStringLiteral("org.moko.Settings"),
                             QStringLiteral("org.moko.Terminal"),
                             QStringLiteral("org.moko.HardwareDiagnostics")})
    , m_applicationLauncher(std::move(applicationLauncher))
    , m_fileLauncher(std::move(fileLauncher))
{
    const QString canonicalHome = QFileInfo(m_homeDirectory).canonicalFilePath();
    if (!canonicalHome.isEmpty())
        m_homeDirectory = canonicalHome;
}

QString AiActions::readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QMap<QString, QString> AiActions::readKeyValueFile(const QString &path)
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

QString AiActions::formatBytes(quint64 bytes)
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

QVariantMap AiActions::result(bool ok, const QString &message, const QVariant &data)
{
    QVariantMap response{{QStringLiteral("ok"), ok}, {QStringLiteral("message"), message}};
    if (data.isValid())
        response.insert(QStringLiteral("data"), data);
    return response;
}

void AiActions::writeActionEvent(const QString &action,
                                 const QString &target,
                                 const QString &state)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(QStringLiteral("MOKO_AI_ACTION action=%1 target=%2 state=%3 uid=%4\n")
                   .arg(sanitizedEventValue(action), sanitizedEventValue(target), state)
                   .arg(geteuid())
                   .toUtf8());
    file.flush();
}

QString AiActions::canonicalAllowedPath(const QString &path) const
{
    if (path.trimmed().isEmpty())
        return {};
    QString expanded = path.trimmed();
    if (expanded == QStringLiteral("~"))
        expanded = m_homeDirectory;
    else if (expanded.startsWith(QStringLiteral("~/")))
        expanded = QDir(m_homeDirectory).filePath(expanded.mid(2));
    else if (QDir::isRelativePath(expanded))
        expanded = QDir(m_homeDirectory).filePath(expanded);

    const QString canonical = QFileInfo(expanded).canonicalFilePath();
    if (canonical.isEmpty())
        return {};
    const QString homePrefix = m_homeDirectory.endsWith(u'/')
        ? m_homeDirectory
        : m_homeDirectory + u'/';
    if (canonical != m_homeDirectory && !canonical.startsWith(homePrefix))
        return {};
    return canonical;
}

bool AiActions::isApplicationAllowed(const QString &appId) const
{
    return m_allowedApplications.contains(appId);
}

bool AiActions::isPathAllowed(const QString &path) const
{
    return !canonicalAllowedPath(path).isEmpty();
}

QVariantMap AiActions::getBatteryStatus() const
{
    QVariantList batteries;
    const QDir supplies(QStringLiteral("/sys/class/power_supply"));
    for (const QString &entry : supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = supplies.filePath(entry);
        if (readTextFile(base + QStringLiteral("/type")) != QStringLiteral("Battery"))
            continue;
        batteries.append(QVariantMap{
            {QStringLiteral("name"), entry},
            {QStringLiteral("capacity"), readTextFile(base + QStringLiteral("/capacity"))},
            {QStringLiteral("status"), readTextFile(base + QStringLiteral("/status"))},
        });
    }
    return {{QStringLiteral("present"), !batteries.isEmpty()},
            {QStringLiteral("batteries"), batteries},
            {QStringLiteral("source"), QStringLiteral("/sys/class/power_supply")}};
}

QVariantMap AiActions::getNetworkStatus() const
{
    QDBusInterface manager(QStringLiteral("org.freedesktop.NetworkManager"),
                           QStringLiteral("/org/freedesktop/NetworkManager"),
                           QStringLiteral("org.freedesktop.NetworkManager"),
                           QDBusConnection::systemBus());
    if (!manager.isValid()) {
        return {{QStringLiteral("available"), false},
                {QStringLiteral("state"), QStringLiteral("unavailable")},
                {QStringLiteral("error"), manager.lastError().message()}};
    }

    const uint state = manager.property("State").toUInt();
    const uint connectivity = manager.property("Connectivity").toUInt();
    const QDBusReply<QList<QDBusObjectPath>> devices = manager.call(QStringLiteral("GetDevices"));
    return {{QStringLiteral("available"), true},
            {QStringLiteral("state"), networkStateName(state)},
            {QStringLiteral("stateCode"), state},
            {QStringLiteral("connectivityCode"), connectivity},
            {QStringLiteral("wirelessEnabled"), manager.property("WirelessEnabled").toBool()},
            {QStringLiteral("deviceCount"), devices.isValid() ? devices.value().size() : 0},
            {QStringLiteral("source"), QStringLiteral("NetworkManager D-Bus")}};
}

QVariantMap AiActions::getStorageStatus() const
{
    QVariantList volumes;
    QSet<QString> seenRoots;
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady() || seenRoots.contains(storage.rootPath()))
            continue;
        seenRoots.insert(storage.rootPath());
        volumes.append(QVariantMap{
            {QStringLiteral("mountPoint"), storage.rootPath()},
            {QStringLiteral("fileSystem"), QString::fromUtf8(storage.fileSystemType())},
            {QStringLiteral("totalBytes"), QVariant::fromValue<qulonglong>(storage.bytesTotal())},
            {QStringLiteral("availableBytes"), QVariant::fromValue<qulonglong>(storage.bytesAvailable())},
            {QStringLiteral("total"), formatBytes(storage.bytesTotal())},
            {QStringLiteral("available"), formatBytes(storage.bytesAvailable())},
            {QStringLiteral("readOnly"), storage.isReadOnly()},
        });
    }
    return {{QStringLiteral("available"), !volumes.isEmpty()},
            {QStringLiteral("volumes"), volumes},
            {QStringLiteral("source"), QStringLiteral("mounted filesystem table")}};
}

QVariantMap AiActions::getSystemSummary() const
{
    struct utsname kernel {};
    const bool haveKernel = uname(&kernel) == 0;
    const auto osRelease = readKeyValueFile(QStringLiteral("/etc/os-release"));
    const QString memInfo = readTextFile(QStringLiteral("/proc/meminfo"));
    const QRegularExpressionMatch totalMatch =
        QRegularExpression(QStringLiteral("^MemTotal:\\s+(\\d+) kB"),
                           QRegularExpression::MultilineOption)
            .match(memInfo);
    const QRegularExpressionMatch availableMatch =
        QRegularExpression(QStringLiteral("^MemAvailable:\\s+(\\d+) kB"),
                           QRegularExpression::MultilineOption)
            .match(memInfo);
    const quint64 totalMemory = totalMatch.hasMatch()
        ? totalMatch.captured(1).toULongLong() * 1024ULL
        : 0;
    const quint64 availableMemory = availableMatch.hasMatch()
        ? availableMatch.captured(1).toULongLong() * 1024ULL
        : 0;

    return {{QStringLiteral("product"), QStringLiteral("MOKO OS v0.1 Developer Preview")},
            {QStringLiteral("base"), osRelease.value(QStringLiteral("PRETTY_NAME"),
                                                      QStringLiteral("Unknown Linux"))},
            {QStringLiteral("hostName"), QSysInfo::machineHostName()},
            {QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture()},
            {QStringLiteral("kernel"), haveKernel ? QString::fromLocal8Bit(kernel.release)
                                                    : QStringLiteral("Unknown")},
            {QStringLiteral("memoryTotalBytes"), QVariant::fromValue<qulonglong>(totalMemory)},
            {QStringLiteral("memoryAvailableBytes"), QVariant::fromValue<qulonglong>(availableMemory)},
            {QStringLiteral("memoryTotal"), totalMemory ? formatBytes(totalMemory)
                                                         : QStringLiteral("Unknown")},
            {QStringLiteral("memoryAvailable"), availableMemory ? formatBytes(availableMemory)
                                                                  : QStringLiteral("Unknown")},
            {QStringLiteral("network"), getNetworkStatus()},
            {QStringLiteral("battery"), getBatteryStatus()},
            {QStringLiteral("storage"), getStorageStatus()}};
}

QStringList AiActions::searchFiles(const QString &query, int maximumResults) const
{
    const QString needle = query.simplified();
    if (needle.isEmpty() || needle.size() > 128 || maximumResults <= 0)
        return {};
    maximumResults = qMin(maximumResults, 100);

    QStringList matches;
    int visited = 0;
    QDirIterator iterator(m_homeDirectory,
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Readable,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext() && matches.size() < maximumResults && visited < 20000) {
        const QString path = iterator.next();
        ++visited;
        const QFileInfo info = iterator.fileInfo();
        if (info.fileName().contains(needle, Qt::CaseInsensitive))
            matches.append(path);
    }
    return matches;
}

QVariantMap AiActions::openApplication(const QString &appId) const
{
    if (!isApplicationAllowed(appId)) {
        writeActionEvent(QStringLiteral("open_application"), appId, QStringLiteral("denied"));
        return result(false, QStringLiteral("That application is not on the MOKO AI allowlist."));
    }

    QVariantMap response;
    if (m_applicationLauncher) {
        response = m_applicationLauncher(appId);
    } else {
        QDBusInterface applications(QStringLiteral("org.moko.Applications1"),
                                    QStringLiteral("/org/moko/Applications1"),
                                    QStringLiteral("org.moko.Applications1"),
                                    QDBusConnection::sessionBus());
        if (!applications.isValid()) {
            writeActionEvent(QStringLiteral("open_application"), appId, QStringLiteral("failed"));
            return result(false, QStringLiteral("MOKO application service is unavailable."));
        }
        const QDBusReply<QVariantMap> reply = applications.call(QStringLiteral("openApplication"), appId);
        if (!reply.isValid()) {
            writeActionEvent(QStringLiteral("open_application"), appId, QStringLiteral("failed"));
            return result(false, reply.error().message());
        }
        response = reply.value();
    }

    const bool ok = response.value(QStringLiteral("ok")).toBool();
    writeActionEvent(QStringLiteral("open_application"), appId,
                     ok ? QStringLiteral("accepted") : QStringLiteral("failed"));
    return response;
}

QVariantMap AiActions::openFile(const QString &path) const
{
    const QString canonical = canonicalAllowedPath(path);
    const QFileInfo info(canonical);
    if (canonical.isEmpty() || !info.isFile()) {
        writeActionEvent(QStringLiteral("open_file"), path, QStringLiteral("denied"));
        return result(false, QStringLiteral("MOKO AI can only open an existing file inside your home directory."));
    }

    bool launched = false;
    if (m_fileLauncher) {
        launched = m_fileLauncher(canonical);
    } else {
        const QString opener = QStandardPaths::findExecutable(QStringLiteral("xdg-open"));
        launched = !opener.isEmpty() && QProcess::startDetached(opener, {canonical});
    }
    writeActionEvent(QStringLiteral("open_file"), canonical,
                     launched ? QStringLiteral("accepted") : QStringLiteral("failed"));
    return result(launched,
                  launched ? QStringLiteral("Opening the file with its registered MIME handler.")
                           : QStringLiteral("No system MIME handler could open the file."),
                  canonical);
}
