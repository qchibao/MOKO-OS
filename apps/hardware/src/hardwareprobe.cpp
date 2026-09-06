#include "hardwareprobe.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>
#include <QThread>
#include <QSet>

#include <functional>
#include <sys/utsname.h>
#include <unistd.h>

namespace {

constexpr auto supported = "SUPPORTED";
constexpr auto partial = "PARTIAL";
constexpr auto unknown = "UNKNOWN";
constexpr auto unsupported = "UNSUPPORTED";

QString firstNonEmpty(const QStringList &values, const QString &fallback = QStringLiteral("Unknown"))
{
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty())
            return value.trimmed();
    }
    return fallback;
}

quint64 memoryValue(const QString &memInfo, const QString &key)
{
    const QRegularExpression expression(
        QStringLiteral("^%1:\\s+(\\d+) kB").arg(QRegularExpression::escape(key)),
        QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = expression.match(memInfo);
    return match.hasMatch() ? match.captured(1).toULongLong() * 1024ULL : 0;
}

QString normalizedHex(QString value)
{
    value = value.trimmed().toLower();
    if (value.startsWith(QStringLiteral("0x")))
        value.remove(0, 2);
    return value;
}

QString linkBaseName(const QString &path)
{
    const QString target = QFileInfo(path).symLinkTarget();
    return target.isEmpty() ? QString{} : QFileInfo(target).fileName();
}

QString privacySafeMountPoint(const QString &path)
{
    const QString home = QDir::cleanPath(QDir::homePath());
    if (path == home || path.startsWith(home + u'/'))
        return QStringLiteral("$HOME") + path.mid(home.size());
    if (path.startsWith(QStringLiteral("/home/")))
        return QStringLiteral("/home/<redacted>");
    if (path.startsWith(QStringLiteral("/run/media/")))
        return QStringLiteral("/run/media/<redacted>");
    if (path.startsWith(QStringLiteral("/media/")))
        return QStringLiteral("/media/<redacted>");
    return path;
}

QVariantList jsonRows(const QJsonArray &devices)
{
    QVariantList result;
    for (const QJsonValue &value : devices)
        result.append(value.toObject().toVariantMap());
    return result;
}

} // namespace

HardwareProbe::HardwareProbe(QObject *parent)
    : QObject(parent)
    , m_sectionOrder({QStringLiteral("system"), QStringLiteral("cpu"),
                      QStringLiteral("memory"), QStringLiteral("graphics"),
                      QStringLiteral("storage"), QStringLiteral("network"),
                      QStringLiteral("bluetooth"), QStringLiteral("audio"),
                      QStringLiteral("input"), QStringLiteral("power"),
                      QStringLiteral("mac")})
    , m_exportDirectory(QDir::homePath())
{
}

QString HardwareProbe::overallStatus() const { return m_overallStatus; }
QString HardwareProbe::overallDisplayStatus() const { return displayStatus(m_overallStatus); }
QString HardwareProbe::refreshedAt() const { return m_refreshedAt; }
QString HardwareProbe::statusMessage() const { return m_statusMessage; }
QString HardwareProbe::exportDirectory() const { return m_exportDirectory; }
QString HardwareProbe::manufacturer() const { return m_manufacturer; }
QString HardwareProbe::model() const { return m_model; }
QString HardwareProbe::activeRenderer() const { return m_activeRenderer; }
bool HardwareProbe::writableDiskDetected() const { return m_writableDiskDetected; }
QJsonObject HardwareProbe::report() const { return m_report; }

void HardwareProbe::setExportDirectory(const QString &directory)
{
    const QString clean = QDir::cleanPath(directory.trimmed());
    if (clean.isEmpty() || clean == m_exportDirectory)
        return;
    m_exportDirectory = clean;
    emit exportDirectoryChanged();
}

QStringList HardwareProbe::sectionIds() const { return m_sectionOrder; }

QString HardwareProbe::sectionTitle(const QString &sectionId) const
{
    return m_sections.value(sectionId).title;
}

QString HardwareProbe::sectionStatus(const QString &sectionId) const
{
    return m_sections.value(sectionId).status;
}

QString HardwareProbe::sectionDisplayStatus(const QString &sectionId) const
{
    return displayStatus(sectionStatus(sectionId));
}

QString HardwareProbe::sectionSummary(const QString &sectionId) const
{
    return m_sections.value(sectionId).summary;
}

QVariantList HardwareProbe::rows(const QString &sectionId) const
{
    return m_sections.value(sectionId).rows;
}

QVariantMap HardwareProbe::row(const QString &label,
                               const QString &value,
                               const QString &evidence,
                               bool available,
                               bool technical)
{
    return {{QStringLiteral("label"), label},
            {QStringLiteral("value"), value},
            {QStringLiteral("evidence"), evidence},
            {QStringLiteral("available"), available},
            {QStringLiteral("technical"), technical}};
}

QString HardwareProbe::displayStatus(const QString &status)
{
    if (status == QString::fromLatin1(supported))
        return QStringLiteral("Working");
    if (status == QString::fromLatin1(partial))
        return QStringLiteral("Limited");
    if (status == QString::fromLatin1(unsupported))
        return QStringLiteral("Unsupported");
    return QStringLiteral("Not detected");
}

QString HardwareProbe::readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}

QMap<QString, QString> HardwareProbe::readKeyValueFile(const QString &path)
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

QString HardwareProbe::processOutput(const QString &program,
                                     const QStringList &arguments,
                                     int timeoutMs)
{
    const QString executable = QStandardPaths::findExecutable(program);
    if (executable.isEmpty())
        return {};
    QProcess process;
    process.setProgram(executable);
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

QString HardwareProbe::formatBytes(quint64 bytes)
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

QString HardwareProbe::vendorName(const QString &vendorId)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("8086"), QStringLiteral("Intel")},
        {QStringLiteral("1002"), QStringLiteral("AMD")},
        {QStringLiteral("1022"), QStringLiteral("AMD")},
        {QStringLiteral("10de"), QStringLiteral("NVIDIA")},
        {QStringLiteral("106b"), QStringLiteral("Apple")},
        {QStringLiteral("14e4"), QStringLiteral("Broadcom")},
        {QStringLiteral("1af4"), QStringLiteral("Red Hat / Virtio")},
        {QStringLiteral("1234"), QStringLiteral("QEMU")},
    };
    const QString normalized = normalizedHex(vendorId);
    return names.value(normalized, normalized.isEmpty()
                                      ? QStringLiteral("Unknown")
                                      : QStringLiteral("PCI vendor 0x%1").arg(normalized));
}

QString HardwareProbe::driverForDevice(const QString &devicePath)
{
    return firstNonEmpty({linkBaseName(QDir(devicePath).filePath(QStringLiteral("driver"))),
                          linkBaseName(QDir(devicePath).filePath(QStringLiteral("device/driver")))},
                         QStringLiteral("Not bound"));
}

QString HardwareProbe::detectOpenGlRenderer()
{
    QOpenGLContext context;
    if (!context.create())
        return {};
    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();
    if (!surface.isValid() || !context.makeCurrent(&surface))
        return {};
    QOpenGLFunctions *functions = context.functions();
    const auto *renderer = functions ? functions->glGetString(GL_RENDERER) : nullptr;
    const QString result = renderer
        ? QString::fromLatin1(reinterpret_cast<const char *>(renderer)).trimmed()
        : QString{};
    context.doneCurrent();
    return result;
}

QString HardwareProbe::safeEventValue(QString value)
{
    for (QChar &character : value) {
        if (!character.isLetterOrNumber() && character != u'.' && character != u'-'
            && character != u'_') {
            character = u'_';
        }
    }
    return value.left(100);
}

void HardwareProbe::setSection(const QString &id,
                               const QString &title,
                               const QString &status,
                               const QString &summary,
                               const QVariantList &rows)
{
    m_sections.insert(id, {title, status, summary, rows});
}

void HardwareProbe::refresh()
{
    m_sections.clear();
    collectEvidence();
    collectSystem();
    collectCpu();
    collectMemory();
    collectGraphics();
    collectStorage();
    collectNetwork();
    collectBluetooth();
    collectAudio();
    collectInput();
    collectPower();
    collectMacSpecific();
    updateOverallStatus();
    m_refreshedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    rebuildReport();
    setStatusMessage(QStringLiteral("Read-only hardware scan complete"));
    emit dataChanged();
}

void HardwareProbe::collectSystem()
{
    m_manufacturer = firstNonEmpty({readTextFile(QStringLiteral("/sys/devices/virtual/dmi/id/sys_vendor")),
                                    readTextFile(QStringLiteral("/sys/firmware/devicetree/base/manufacturer"))});
    m_model = firstNonEmpty({readTextFile(QStringLiteral("/sys/devices/virtual/dmi/id/product_name")),
                             readTextFile(QStringLiteral("/sys/firmware/devicetree/base/model"))});
    const QString firmware = QDir(QStringLiteral("/sys/firmware/efi")).exists()
        ? QStringLiteral("UEFI") : QStringLiteral("Legacy BIOS or UEFI CSM");
    const QString biosVendor = readTextFile(QStringLiteral("/sys/devices/virtual/dmi/id/bios_vendor"));
    const QString biosVersion = readTextFile(QStringLiteral("/sys/devices/virtual/dmi/id/bios_version"));
    const QString biosDate = readTextFile(QStringLiteral("/sys/devices/virtual/dmi/id/bios_date"));
    const QString architecture = QSysInfo::currentCpuArchitecture();
    const bool targetArchitecture = architecture == QStringLiteral("x86_64")
        || architecture == QStringLiteral("amd64");
    setSection(QStringLiteral("system"), QStringLiteral("System"),
               targetArchitecture ? QString::fromLatin1(supported) : QString::fromLatin1(unsupported),
               targetArchitecture ? QStringLiteral("Compatible 64-bit system architecture")
                                  : QStringLiteral("This preview supports x86_64 systems only"),
               {row(QStringLiteral("Manufacturer"), m_manufacturer, QStringLiteral("DMI/sysfs")),
                row(QStringLiteral("Model"), m_model, QStringLiteral("DMI/sysfs")),
                row(QStringLiteral("Firmware"), firmware, QStringLiteral("/sys/firmware/efi")),
                row(QStringLiteral("Firmware vendor"), firstNonEmpty({biosVendor}),
                    QStringLiteral("DMI bios_vendor"), !biosVendor.isEmpty(), true),
                row(QStringLiteral("Firmware version"), firstNonEmpty({biosVersion}),
                    biosDate.isEmpty() ? QStringLiteral("DMI bios_version")
                                       : QStringLiteral("DMI date %1").arg(biosDate),
                    !biosVersion.isEmpty(), true),
                row(QStringLiteral("Architecture"), architecture, QStringLiteral("Qt system information"))});
}

void HardwareProbe::collectCpu()
{
    const QString cpuInfo = readTextFile(QStringLiteral("/proc/cpuinfo"));
    const QStringList lines = cpuInfo.split(u'\n');
    QString vendor;
    QString model;
    QString flags;
    int threads = 0;
    QSet<QString> physicalCores;
    QString physicalId;
    QString coreId;
    int reportedCores = 0;
    for (const QString &line : lines) {
        const qsizetype separator = line.indexOf(u':');
        if (separator < 0)
            continue;
        const QString key = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();
        if (key == QStringLiteral("processor")) {
            if (!physicalId.isEmpty() || !coreId.isEmpty())
                physicalCores.insert(physicalId + u':' + coreId);
            physicalId.clear();
            coreId.clear();
            ++threads;
        } else if (key == QStringLiteral("vendor_id") && vendor.isEmpty()) {
            vendor = value;
        } else if (key == QStringLiteral("model name") && model.isEmpty()) {
            model = value;
        } else if (key == QStringLiteral("flags") && flags.isEmpty()) {
            flags = value;
        } else if (key == QStringLiteral("physical id")) {
            physicalId = value;
        } else if (key == QStringLiteral("core id")) {
            coreId = value;
        } else if (key == QStringLiteral("cpu cores") && reportedCores == 0) {
            reportedCores = value.toInt();
        }
    }
    if (!physicalId.isEmpty() || !coreId.isEmpty())
        physicalCores.insert(physicalId + u':' + coreId);
    const int cores = !physicalCores.isEmpty() ? physicalCores.size()
                                               : qMax(reportedCores, qMax(1, threads));
    const bool virtualization = flags.split(u' ', Qt::SkipEmptyParts).contains(QStringLiteral("vmx"))
        || flags.split(u' ', Qt::SkipEmptyParts).contains(QStringLiteral("svm"));
    const bool architectureOk = QSysInfo::currentCpuArchitecture() == QStringLiteral("x86_64")
        || QSysInfo::currentCpuArchitecture() == QStringLiteral("amd64");
    const QString status = !architectureOk ? QString::fromLatin1(unsupported)
        : threads >= 2 ? QString::fromLatin1(supported) : QString::fromLatin1(partial);
    setSection(QStringLiteral("cpu"), QStringLiteral("CPU"), status,
               threads >= 2 ? QStringLiteral("Processor meets the preview requirements")
                            : QStringLiteral("Processor support is limited"),
               {row(QStringLiteral("Vendor"), firstNonEmpty({vendor}), QStringLiteral("/proc/cpuinfo")),
                row(QStringLiteral("Model"), firstNonEmpty({model}), QStringLiteral("/proc/cpuinfo")),
                row(QStringLiteral("Physical cores"), QString::number(cores), QStringLiteral("/proc/cpuinfo")),
                row(QStringLiteral("Hardware threads"), QString::number(threads), QStringLiteral("/proc/cpuinfo")),
                row(QStringLiteral("Virtualization capability"),
                    virtualization ? QStringLiteral("Available") : QStringLiteral("Not advertised"),
                    virtualization ? QStringLiteral("vmx/svm CPU flag") : QStringLiteral("No vmx/svm CPU flag"),
                    virtualization)});
}

void HardwareProbe::collectMemory()
{
    const QString memInfo = readTextFile(QStringLiteral("/proc/meminfo"));
    const quint64 total = memoryValue(memInfo, QStringLiteral("MemTotal"));
    const quint64 availableBytes = memoryValue(memInfo, QStringLiteral("MemAvailable"));
    const quint64 gib = 1024ULL * 1024ULL * 1024ULL;
    const QString status = total >= 4 * gib ? QString::fromLatin1(supported)
        : total >= 2 * gib ? QString::fromLatin1(partial)
        : total > 0 ? QString::fromLatin1(unsupported) : QString::fromLatin1(unknown);
    const QString summary = total >= 4 * gib
        ? QStringLiteral("Memory meets the preview requirements")
        : total > 0 ? QStringLiteral("Available memory is below the recommended amount")
                    : QStringLiteral("Memory capacity could not be read");
    setSection(QStringLiteral("memory"), QStringLiteral("Memory"), status, summary,
               {row(QStringLiteral("Total RAM"), total ? formatBytes(total) : QStringLiteral("Unknown"),
                    QStringLiteral("/proc/meminfo"), total > 0),
                row(QStringLiteral("Available RAM"), availableBytes ? formatBytes(availableBytes)
                                                                    : QStringLiteral("Unknown"),
                    QStringLiteral("/proc/meminfo"), availableBytes > 0)});
}

void HardwareProbe::collectGraphics()
{
    QString gpuVendor;
    QString gpuModel;
    QString driver;
    const QString lspci = processOutput(QStringLiteral("lspci"),
                                        {QStringLiteral("-Dnnk")}, 2000);
    const QStringList blocks = lspci.split(QRegularExpression(QStringLiteral("\\n(?=\\S)")));
    for (const QString &block : blocks) {
        if (!block.contains(QRegularExpression(QStringLiteral("VGA compatible controller|3D controller|Display controller"),
                                               QRegularExpression::CaseInsensitiveOption)))
            continue;
        const QString firstLine = block.section(u'\n', 0, 0);
        gpuModel = firstLine.section(QStringLiteral(": "), 1).trimmed();
        const QRegularExpressionMatch vendorMatch = QRegularExpression(QStringLiteral("\\[([0-9a-fA-F]{4}):[0-9a-fA-F]{4}\\]"))
                                                       .match(firstLine);
        if (vendorMatch.hasMatch())
            gpuVendor = vendorName(vendorMatch.captured(1));
        const QRegularExpressionMatch driverMatch = QRegularExpression(QStringLiteral("Kernel driver in use:\\s*(.+)"))
                                                       .match(block);
        if (driverMatch.hasMatch())
            driver = driverMatch.captured(1).trimmed();
        break;
    }

    if (gpuModel.isEmpty()) {
        const QDir devices(QStringLiteral("/sys/bus/pci/devices"));
        for (const QString &entry : devices.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString base = devices.filePath(entry);
            if (!readTextFile(base + QStringLiteral("/class")).startsWith(QStringLiteral("0x03")))
                continue;
            const QString vendorId = readTextFile(base + QStringLiteral("/vendor"));
            const QString deviceId = normalizedHex(readTextFile(base + QStringLiteral("/device")));
            gpuVendor = vendorName(vendorId);
            gpuModel = QStringLiteral("PCI device %1:%2")
                           .arg(normalizedHex(vendorId), deviceId);
            driver = driverForDevice(base);
            break;
        }
    }

    m_activeRenderer = detectOpenGlRenderer();
    const QString waylandRenderer = QStringLiteral("%1 / %2")
                                        .arg(QGuiApplication::platformName(),
                                             firstNonEmpty({m_activeRenderer}, QStringLiteral("renderer unavailable")));
    const bool software = m_activeRenderer.contains(QStringLiteral("llvmpipe"), Qt::CaseInsensitive)
        || m_activeRenderer.contains(QStringLiteral("softpipe"), Qt::CaseInsensitive);
    const bool driverBound = !driver.isEmpty() && driver != QStringLiteral("Not bound");
    const QString status = driverBound && !m_activeRenderer.isEmpty() && !software
        ? QString::fromLatin1(supported)
        : (driverBound || !m_activeRenderer.isEmpty()) ? QString::fromLatin1(partial)
                                                       : QString::fromLatin1(unknown);
    setSection(QStringLiteral("graphics"), QStringLiteral("Graphics"), status,
               software ? QStringLiteral("Graphics is available with limited acceleration")
                        : driverBound ? QStringLiteral("Graphics acceleration is working")
                                      : QStringLiteral("Graphics support needs hardware validation"),
               {row(QStringLiteral("GPU vendor"), firstNonEmpty({gpuVendor}), QStringLiteral("PCI inventory")),
                row(QStringLiteral("GPU model"), firstNonEmpty({gpuModel}), QStringLiteral("lspci/sysfs")),
                row(QStringLiteral("Kernel driver"), firstNonEmpty({driver}),
                    QStringLiteral("PCI driver binding"), driverBound, true),
                row(QStringLiteral("Active renderer"), firstNonEmpty({m_activeRenderer}),
                    QStringLiteral("OpenGL context"), !m_activeRenderer.isEmpty(), true),
                row(QStringLiteral("Wayland renderer"), waylandRenderer,
                    QStringLiteral("Session and Qt Quick graphics API"), true, true)});
}

void HardwareProbe::collectStorage()
{
    const QString output = processOutput(QStringLiteral("lsblk"),
                                         {QStringLiteral("--json"), QStringLiteral("--bytes"),
                                          QStringLiteral("--output"),
                                          QStringLiteral("NAME,TYPE,TRAN,SIZE,FSTYPE,FSAVAIL,RO,RM,MODEL")},
                                         2400);
    const QJsonDocument document = QJsonDocument::fromJson(output.toUtf8());
    const QJsonArray devices = document.object().value(QStringLiteral("blockdevices")).toArray();
    QVariantList rows;
    bool writableDisk = false;
    int diskCount = 0;
    std::function<void(const QJsonObject &, const QString &)> appendDevice;
    appendDevice = [&](const QJsonObject &device, const QString &parentTransport) {
        const QString type = device.value(QStringLiteral("type")).toString();
        const QString transport = firstNonEmpty({device.value(QStringLiteral("tran")).toString(), parentTransport},
                                                QStringLiteral("Unknown transport"));
        if (type == QStringLiteral("disk")) {
            ++diskCount;
            const bool readOnly = device.value(QStringLiteral("ro")).toBool();
            const bool removable = device.value(QStringLiteral("rm")).toBool();
            writableDisk = writableDisk || !readOnly;
            rows.append(row(QStringLiteral("Disk %1").arg(device.value(QStringLiteral("name")).toString()),
                            formatBytes(device.value(QStringLiteral("size")).toVariant().toULongLong()),
                            QStringLiteral("%1; %2; %3")
                                .arg(transport.toUpper(),
                                     firstNonEmpty({device.value(QStringLiteral("model")).toString()},
                                                   QStringLiteral("model not reported")),
                                     readOnly ? QStringLiteral("read-only")
                                              : removable ? QStringLiteral("removable")
                                                          : QStringLiteral("writable"))));
        } else if (type == QStringLiteral("part") || type == QStringLiteral("crypt")
                   || type == QStringLiteral("lvm")) {
            const QString filesystem = firstNonEmpty({device.value(QStringLiteral("fstype")).toString()},
                                                     QStringLiteral("No filesystem reported"));
            const quint64 free = device.value(QStringLiteral("fsavail")).toVariant().toULongLong();
            rows.append(row(QStringLiteral("Volume %1").arg(device.value(QStringLiteral("name")).toString()),
                            free ? QStringLiteral("%1 free").arg(formatBytes(free)) : filesystem,
                            QStringLiteral("%1; parent transport %2").arg(filesystem, transport.toUpper())));
        }
        for (const QJsonValue &child : device.value(QStringLiteral("children")).toArray())
            appendDevice(child.toObject(), transport);
    };
    for (const QJsonValue &device : devices)
        appendDevice(device.toObject(), {});
    QSet<QString> mountedRoots;
    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        const QString rootPath = volume.rootPath();
        if (!volume.isValid() || !volume.isReady() || !QFileInfo(rootPath).isDir()
            || mountedRoots.contains(rootPath)) {
            continue;
        }
        mountedRoots.insert(rootPath);
        rows.append(row(QStringLiteral("Filesystem %1").arg(privacySafeMountPoint(rootPath)),
                        QStringLiteral("%1 free of %2")
                            .arg(formatBytes(volume.bytesAvailable()), formatBytes(volume.bytesTotal())),
                        QStringLiteral("%1; %2")
                            .arg(QString::fromUtf8(volume.fileSystemType()),
                                 volume.isReadOnly() ? QStringLiteral("read-only")
                                                     : QStringLiteral("writable"))));
    }
    if (rows.isEmpty())
        rows.append(row(QStringLiteral("Target disks"), QStringLiteral("None detected"),
                        QStringLiteral("The validation VM attaches no writable disk"), false));
    m_writableDiskDetected = writableDisk;
    const QString status = writableDisk ? QString::fromLatin1(supported)
        : diskCount > 0 ? QString::fromLatin1(partial) : QString::fromLatin1(unknown);
    setSection(QStringLiteral("storage"), QStringLiteral("Storage"), status,
               writableDisk ? QStringLiteral("At least one writable disk was detected")
                            : QStringLiteral("No writable target disk is present"), rows);
}

void HardwareProbe::collectNetwork()
{
    QVariantList rows;
    const QDir interfaces(QStringLiteral("/sys/class/net"));
    bool deviceFound = false;
    bool driverFound = false;
    for (const QString &name : interfaces.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (name == QStringLiteral("lo"))
            continue;
        const QString base = interfaces.filePath(name);
        const bool wireless = QDir(base + QStringLiteral("/wireless")).exists();
        const QString driver = driverForDevice(base);
        const QString state = firstNonEmpty({readTextFile(base + QStringLiteral("/operstate"))});
        const QString ethtool = processOutput(QStringLiteral("ethtool"),
                                              {QStringLiteral("--driver"), name}, 1200);
        const QRegularExpressionMatch firmwareMatch =
            QRegularExpression(QStringLiteral("^firmware-version:\\s*(.+)$"),
                               QRegularExpression::MultilineOption).match(ethtool);
        const QString firmware = firmwareMatch.hasMatch() && !firmwareMatch.captured(1).trimmed().isEmpty()
            ? firmwareMatch.captured(1).trimmed() : QStringLiteral("Not reported by the driver");
        deviceFound = true;
        driverFound = driverFound || driver != QStringLiteral("Not bound");
        rows.append(row(wireless ? QStringLiteral("Wi-Fi") : QStringLiteral("Ethernet"),
                        driver != QStringLiteral("Not bound") ? QStringLiteral("Working")
                                                               : QStringLiteral("Limited"),
                        QStringLiteral("interface %1; driver %2; state %3; firmware %4")
                            .arg(name, driver, state, firmware),
                        driver != QStringLiteral("Not bound")));
    }
    if (!deviceFound)
        rows.append(row(QStringLiteral("Network devices"), QStringLiteral("None detected"),
                        QStringLiteral("/sys/class/net"), false));
    const QString status = driverFound ? QString::fromLatin1(supported)
        : deviceFound ? QString::fromLatin1(partial) : QString::fromLatin1(unknown);
    setSection(QStringLiteral("network"), QStringLiteral("Network"), status,
               driverFound ? QStringLiteral("Network hardware is working")
                           : QStringLiteral("Network support needs further validation"), rows);
}

void HardwareProbe::collectBluetooth()
{
    QDBusConnectionInterface *interface = QDBusConnection::systemBus().interface();
    const bool bluez = interface && interface->isServiceRegistered(QStringLiteral("org.bluez"));
    QVariantList rows{row(QStringLiteral("Bluetooth service"),
                              bluez ? QStringLiteral("Working") : QStringLiteral("Not detected")),
                      row(QStringLiteral("BlueZ service"), bluez ? QStringLiteral("Running")
                                                                  : QStringLiteral("Unavailable"),
                          QStringLiteral("org.bluez on system D-Bus"), bluez, true)};
    const QDir controllers(QStringLiteral("/sys/class/bluetooth"));
    const QStringList entries = controllers.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    bool driverFound = false;
    for (const QString &controller : entries) {
        const QString driver = driverForDevice(controllers.filePath(controller));
        driverFound = driverFound || driver != QStringLiteral("Not bound");
        rows.append(row(QStringLiteral("Controller"), QStringLiteral("Detected"),
                        QStringLiteral("%1; driver %2").arg(controller, driver),
                        driver != QStringLiteral("Not bound")));
    }
    if (entries.isEmpty())
        rows.append(row(QStringLiteral("Controller"), QStringLiteral("None detected"),
                        QStringLiteral("/sys/class/bluetooth"), false));
    const QString status = bluez && driverFound ? QString::fromLatin1(supported)
        : bluez || !entries.isEmpty() ? QString::fromLatin1(partial) : QString::fromLatin1(unknown);
    setSection(QStringLiteral("bluetooth"), QStringLiteral("Bluetooth"), status,
               entries.isEmpty() ? QStringLiteral("Bluetooth hardware was not detected")
                                 : QStringLiteral("Bluetooth hardware is available"), rows);
}

void HardwareProbe::collectAudio()
{
    const QString runtimeDirectory = qEnvironmentVariable(
        "XDG_RUNTIME_DIR", QStringLiteral("/run/user/%1").arg(geteuid()));
    const bool pipewire = QFile::exists(QDir(runtimeDirectory).filePath(QStringLiteral("pipewire-0")));
    QVariantList rows{row(QStringLiteral("Audio service"),
                              pipewire ? QStringLiteral("Working") : QStringLiteral("Not detected")),
                      row(QStringLiteral("PipeWire"), pipewire ? QStringLiteral("Running")
                                                               : QStringLiteral("Unavailable"),
                          QDir(runtimeDirectory).filePath(QStringLiteral("pipewire-0")), pipewire, true)};
    const QDir sound(QStringLiteral("/sys/class/sound"));
    const QStringList cards = sound.entryList({QStringLiteral("card[0-9]*")},
                                              QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &card : cards) {
        const QString base = sound.filePath(card);
        rows.append(row(QStringLiteral("Audio device %1").arg(card),
                        firstNonEmpty({readTextFile(base + QStringLiteral("/id")),
                                       readTextFile(base + QStringLiteral("/device/product"))}),
                        QStringLiteral("%1; driver %2").arg(base, driverForDevice(base))));
    }
    if (cards.isEmpty())
        rows.append(row(QStringLiteral("Audio devices"), QStringLiteral("None detected"),
                        QStringLiteral("/sys/class/sound"), false));
    const QString wpctl = processOutput(QStringLiteral("wpctl"),
                                        {QStringLiteral("status"), QStringLiteral("--name")}, 1800);
    rows.append(row(QStringLiteral("WirePlumber graph"), wpctl.isEmpty()
                        ? QStringLiteral("No device graph reported") : QStringLiteral("Available"),
                    wpctl.isEmpty() ? QStringLiteral("wpctl returned no data")
                                    : QStringLiteral("wpctl returned a live graph; device names are omitted from exports"),
                    !wpctl.isEmpty(), true));
    const QString status = pipewire && !cards.isEmpty() ? QString::fromLatin1(supported)
        : pipewire ? QString::fromLatin1(partial) : QString::fromLatin1(unknown);
    setSection(QStringLiteral("audio"), QStringLiteral("Audio"), status,
               pipewire ? (cards.isEmpty() ? QStringLiteral("Audio is available, but no output device was detected")
                                           : QStringLiteral("Audio is working"))
                        : QStringLiteral("Audio was not detected"), rows);
}

void HardwareProbe::collectInput()
{
    QVariantList rows;
    const QDir input(QStringLiteral("/sys/class/input"));
    QSet<QString> names;
    bool keyboard = false;
    bool pointer = false;
    bool deviceFound = false;
    for (const QString &event : input.entryList({QStringLiteral("event*")},
                                                QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = input.filePath(event);
        const QString name = readTextFile(base + QStringLiteral("/device/name"));
        if (name.isEmpty() || names.contains(name))
            continue;
        names.insert(name);
        deviceFound = true;
        const QString lower = name.toLower();
        QString type = QStringLiteral("Input device");
        if (lower.contains(QStringLiteral("keyboard")) || lower.contains(QStringLiteral("kbd"))) {
            type = QStringLiteral("Keyboard");
            keyboard = true;
        } else if (lower.contains(QStringLiteral("touchpad")) || lower.contains(QStringLiteral("trackpad"))) {
            type = QStringLiteral("Touchpad");
            pointer = true;
        } else if (lower.contains(QStringLiteral("mouse")) || lower.contains(QStringLiteral("tablet"))
                   || lower.contains(QStringLiteral("pointer"))) {
            type = QStringLiteral("Mouse / pointer");
            pointer = true;
        }
        rows.append(row(type, name, QStringLiteral("%1; driver %2").arg(event, driverForDevice(base))));
    }
    if (rows.isEmpty())
        rows.append(row(QStringLiteral("Input devices"), QStringLiteral("None detected"),
                        QStringLiteral("/sys/class/input"), false));
    const QString status = keyboard && pointer ? QString::fromLatin1(supported)
        : deviceFound ? QString::fromLatin1(partial) : QString::fromLatin1(unknown);
    setSection(QStringLiteral("input"), QStringLiteral("Input"), status,
               keyboard && pointer ? QStringLiteral("Keyboard and pointing input are visible")
                                   : QStringLiteral("Input coverage is incomplete"), rows);
}

void HardwareProbe::collectPower()
{
    QVariantList rows;
    const QDir supplies(QStringLiteral("/sys/class/power_supply"));
    bool battery = false;
    for (const QString &entry : supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = supplies.filePath(entry);
        const QString type = readTextFile(base + QStringLiteral("/type"));
        if (type == QStringLiteral("Battery")) {
            battery = true;
            const QString capacity = readTextFile(base + QStringLiteral("/capacity"));
            const QString chargingState = readTextFile(base + QStringLiteral("/status"));
            rows.append(row(QStringLiteral("Battery %1").arg(entry),
                            capacity.isEmpty() ? firstNonEmpty({chargingState})
                                               : capacity + QStringLiteral("%"),
                            QStringLiteral("charging state %1").arg(
                                firstNonEmpty({chargingState}))));
        }
    }
    if (!battery)
        rows.append(row(QStringLiteral("Battery"), QStringLiteral("Not detected"),
                        QStringLiteral("No battery in /sys/class/power_supply"), false));
    QDBusInterface login(QStringLiteral("org.freedesktop.login1"),
                         QStringLiteral("/org/freedesktop/login1"),
                         QStringLiteral("org.freedesktop.login1.Manager"),
                         QDBusConnection::systemBus());
    QString canSuspend;
    if (login.isValid()) {
        const QDBusMessage reply = login.call(QStringLiteral("CanSuspend"));
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
            canSuspend = reply.arguments().constFirst().toString();
    }
    rows.append(row(QStringLiteral("Sleep"), canSuspend == QStringLiteral("yes")
                                                   ? QStringLiteral("Available")
                                                   : canSuspend == QStringLiteral("challenge")
                                                     ? QStringLiteral("Authorization required")
                                                     : canSuspend.isEmpty() ? QStringLiteral("Not detected")
                                                                            : QStringLiteral("Unavailable"),
                    QStringLiteral("systemd-logind CanSuspend returned '%1'").arg(canSuspend),
                    !canSuspend.isEmpty()));
    const QString status = canSuspend == QStringLiteral("yes") ? QString::fromLatin1(supported)
        : !canSuspend.isEmpty() ? QString::fromLatin1(partial) : QString::fromLatin1(unknown);
    setSection(QStringLiteral("power"), QStringLiteral("Power"), status,
               battery ? QStringLiteral("Battery and sleep information is available")
                       : QStringLiteral("No battery was detected on this device"), rows);
}

void HardwareProbe::collectMacSpecific()
{
    const bool appleSystem = m_manufacturer.contains(QStringLiteral("Apple"), Qt::CaseInsensitive)
        || m_model.startsWith(QStringLiteral("Mac"), Qt::CaseInsensitive);
    const QJsonArray pci = m_evidence.value(QStringLiteral("pciDevices")).toArray();
    const QJsonArray usb = m_evidence.value(QStringLiteral("usbDevices")).toArray();
    bool t2 = false;
    bool broadcom = false;
    bool appleTrackpad = false;
    bool appleKeyboard = false;
    for (const QJsonValue &value : pci) {
        const QJsonObject device = value.toObject();
        const QString vendor = device.value(QStringLiteral("vendorId")).toString();
        const QString label = device.value(QStringLiteral("label")).toString();
        const QString deviceId = device.value(QStringLiteral("deviceId")).toString();
        t2 = t2 || (vendor == QStringLiteral("106b")
                    && (label.contains(QStringLiteral("T2"), Qt::CaseInsensitive)
                        || QStringList{QStringLiteral("1801"), QStringLiteral("1802"),
                                       QStringLiteral("1803"), QStringLiteral("1804")}
                               .contains(deviceId)));
        broadcom = broadcom || vendor == QStringLiteral("14e4");
    }
    for (const QJsonValue &value : usb) {
        const QString product = value.toObject().value(QStringLiteral("product")).toString();
        appleTrackpad = appleTrackpad || product.contains(QStringLiteral("Trackpad"), Qt::CaseInsensitive);
        appleKeyboard = appleKeyboard || product.contains(QStringLiteral("Keyboard"), Qt::CaseInsensitive)
            && value.toObject().value(QStringLiteral("vendorId")).toString() == QStringLiteral("05ac");
    }
    for (const QVariant &value : m_sections.value(QStringLiteral("input")).rows) {
        const QString inputName = value.toMap().value(QStringLiteral("value")).toString();
        appleTrackpad = appleTrackpad
            || inputName.contains(QStringLiteral("Apple"), Qt::CaseInsensitive)
                && (inputName.contains(QStringLiteral("Trackpad"), Qt::CaseInsensitive)
                    || inputName.contains(QStringLiteral("Touchpad"), Qt::CaseInsensitive));
        appleKeyboard = appleKeyboard
            || inputName.contains(QStringLiteral("Apple"), Qt::CaseInsensitive)
                && inputName.contains(QStringLiteral("Keyboard"), Qt::CaseInsensitive);
    }
    setSection(QStringLiteral("mac"), QStringLiteral("Mac-specific"),
               appleSystem ? QString::fromLatin1(partial) : QString::fromLatin1(unknown),
               appleSystem ? QStringLiteral("Intel Mac evidence found; model-specific live testing is required")
                           : QStringLiteral("No Apple system identity detected"),
               {row(QStringLiteral("Intel Mac model"), appleSystem ? m_model : QStringLiteral("Not detected"),
                    QStringLiteral("DMI manufacturer/model"), appleSystem),
                row(QStringLiteral("Apple T2"), t2 ? QStringLiteral("Detected") : QStringLiteral("Not detected"),
                    QStringLiteral("Apple PCI bridge/T2 evidence"), t2),
                row(QStringLiteral("Broadcom hardware"), broadcom ? QStringLiteral("Detected") : QStringLiteral("Not detected"),
                    QStringLiteral("PCI vendor 14e4"), broadcom),
                row(QStringLiteral("Apple trackpad"), appleTrackpad ? QStringLiteral("Detected") : QStringLiteral("Not detected"),
                    QStringLiteral("USB product inventory"), appleTrackpad),
                row(QStringLiteral("Apple keyboard"), appleKeyboard ? QStringLiteral("Detected") : QStringLiteral("Not detected"),
                    QStringLiteral("USB product inventory"), appleKeyboard)});
}

void HardwareProbe::collectEvidence()
{
    struct utsname kernelInfo {};
    const QString kernel = uname(&kernelInfo) == 0
        ? QString::fromLocal8Bit(kernelInfo.release) : QStringLiteral("Unknown");
    QJsonArray modules;
    const QString moduleText = readTextFile(QStringLiteral("/proc/modules"));
    for (const QString &line : moduleText.split(u'\n', Qt::SkipEmptyParts))
        modules.append(line.section(u' ', 0, 0));

    QJsonArray pciDevices;
    const QDir pci(QStringLiteral("/sys/bus/pci/devices"));
    for (const QString &entry : pci.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = pci.filePath(entry);
        const QString vendor = normalizedHex(readTextFile(base + QStringLiteral("/vendor")));
        const QString device = normalizedHex(readTextFile(base + QStringLiteral("/device")));
        const QString classId = normalizedHex(readTextFile(base + QStringLiteral("/class")));
        pciDevices.append(QJsonObject{{QStringLiteral("address"), entry},
                                      {QStringLiteral("vendorId"), vendor},
                                      {QStringLiteral("deviceId"), device},
                                      {QStringLiteral("classId"), classId},
                                      {QStringLiteral("driver"), driverForDevice(base)},
                                      {QStringLiteral("label"), QStringLiteral("%1 %2:%3")
                                           .arg(vendorName(vendor), vendor, device)}});
    }

    QJsonArray usbDevices;
    const QDir usb(QStringLiteral("/sys/bus/usb/devices"));
    for (const QString &entry : usb.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = usb.filePath(entry);
        const QString vendor = normalizedHex(readTextFile(base + QStringLiteral("/idVendor")));
        const QString productId = normalizedHex(readTextFile(base + QStringLiteral("/idProduct")));
        if (vendor.isEmpty() || productId.isEmpty())
            continue;
        usbDevices.append(QJsonObject{{QStringLiteral("vendorId"), vendor},
                                      {QStringLiteral("productId"), productId},
                                      {QStringLiteral("manufacturer"), readTextFile(base + QStringLiteral("/manufacturer"))},
                                      {QStringLiteral("product"), readTextFile(base + QStringLiteral("/product"))},
                                      {QStringLiteral("driver"), driverForDevice(base)}});
    }
    m_evidence = {{QStringLiteral("kernelVersion"), kernel},
                  {QStringLiteral("loadedModules"), modules},
                  {QStringLiteral("pciDevices"), pciDevices},
                  {QStringLiteral("usbDevices"), usbDevices}};
}

void HardwareProbe::updateOverallStatus()
{
    bool anyKnown = false;
    bool degraded = false;
    for (const QString &id : m_sectionOrder) {
        const QString status = m_sections.value(id).status;
        if (status == QString::fromLatin1(unsupported)) {
            m_overallStatus = QString::fromLatin1(unsupported);
            return;
        }
        anyKnown = anyKnown || status == QString::fromLatin1(supported)
            || status == QString::fromLatin1(partial);
        degraded = degraded || status == QString::fromLatin1(partial)
            || status == QString::fromLatin1(unknown);
    }
    m_overallStatus = !anyKnown ? QString::fromLatin1(unknown)
        : degraded ? QString::fromLatin1(partial) : QString::fromLatin1(supported);
}

void HardwareProbe::rebuildReport()
{
    QJsonObject categories;
    for (const QString &id : m_sectionOrder) {
        const Section section = m_sections.value(id);
        categories.insert(id, QJsonObject{{QStringLiteral("title"), section.title},
                                          {QStringLiteral("status"), section.status},
                                          {QStringLiteral("summary"), section.summary},
                                          {QStringLiteral("items"), QJsonArray::fromVariantList(section.rows)}});
    }
    const auto os = readKeyValueFile(QStringLiteral("/etc/os-release"));
    m_report = {{QStringLiteral("schema"), QStringLiteral("org.moko.hardware-report.v1")},
                {QStringLiteral("generatedAt"), m_refreshedAt},
                {QStringLiteral("product"), QStringLiteral("MOKO OS v0.1.1 Hardware & Usability Preview")},
                {QStringLiteral("linuxBase"), os.value(QStringLiteral("PRETTY_NAME"), QStringLiteral("Unknown Linux"))},
                {QStringLiteral("overallStatus"), m_overallStatus},
                {QStringLiteral("privacy"), QStringLiteral("Serial numbers, MAC addresses, host names and personal files are excluded")},
                {QStringLiteral("categories"), categories},
                {QStringLiteral("evidence"), m_evidence}};
}

void HardwareProbe::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

bool HardwareProbe::exportJson() { return exportJsonToDirectory(m_exportDirectory); }
bool HardwareProbe::exportText() { return exportTextToDirectory(m_exportDirectory); }

bool HardwareProbe::exportAll()
{
    const bool json = exportJson();
    const bool text = exportText();
    return json && text;
}

bool HardwareProbe::exportJsonToDirectory(const QString &directory)
{
    return writeReport(QStringLiteral("json"), directory);
}

bool HardwareProbe::exportTextToDirectory(const QString &directory)
{
    return writeReport(QStringLiteral("txt"), directory);
}

bool HardwareProbe::exportJsonToPath(const QString &path)
{
    return writeReportPath(QStringLiteral("json"), path);
}

bool HardwareProbe::exportTextToPath(const QString &path)
{
    return writeReportPath(QStringLiteral("txt"), path);
}

bool HardwareProbe::writeReport(const QString &format, const QString &directory)
{
    QDir target(directory);
    if (!target.exists() && !QDir().mkpath(target.absolutePath())) {
        setStatusMessage(QStringLiteral("Could not create the report directory"));
        return false;
    }
    const QString fileName = format == QStringLiteral("json")
        ? QStringLiteral("moko-hardware-report.json")
        : QStringLiteral("moko-hardware-report.txt");
    return writeReportPath(format, target.filePath(fileName));
}

bool HardwareProbe::writeReportPath(const QString &format, const QString &path)
{
    const QFileInfo targetInfo(path);
    if (format != QStringLiteral("json") && format != QStringLiteral("txt")) {
        setStatusMessage(QStringLiteral("Unsupported report format"));
        return false;
    }
    if (targetInfo.fileName().isEmpty() || targetInfo.isDir()) {
        setStatusMessage(QStringLiteral("Choose a report file"));
        return false;
    }
    QDir directory(targetInfo.absolutePath());
    if (!directory.exists() || !QFileInfo(directory.absolutePath()).isWritable()) {
        setStatusMessage(QStringLiteral("The report folder is not writable"));
        return false;
    }
    const QString cleanPath = targetInfo.absoluteFilePath();
    const QByteArray contents = format == QStringLiteral("json")
        ? QJsonDocument(m_report).toJson(QJsonDocument::Indented) : textReport();
    QSaveFile file(cleanPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()
        || !file.commit()) {
        setStatusMessage(QStringLiteral("Could not write %1").arg(targetInfo.fileName()));
        return false;
    }
    setExportDirectory(directory.absolutePath());
    setStatusMessage(QStringLiteral("Exported %1").arg(targetInfo.fileName()));
    emit exportWritten(format, cleanPath, contents.size());
    return true;
}

QByteArray HardwareProbe::textReport() const
{
    QString text;
    text += QStringLiteral("MOKO Hardware Report\n");
    text += QStringLiteral("Generated: %1\n").arg(m_refreshedAt);
    text += QStringLiteral("Overall compatibility: %1\n").arg(m_overallStatus);
    text += QStringLiteral("Privacy: serial numbers, MAC addresses, host names and personal files are excluded.\n\n");
    for (const QString &id : m_sectionOrder) {
        const Section section = m_sections.value(id);
        text += QStringLiteral("[%1] %2\n%3\n")
                    .arg(section.status, section.title, section.summary);
        for (const QVariant &value : section.rows) {
            const QVariantMap item = value.toMap();
            text += QStringLiteral("- %1: %2")
                        .arg(item.value(QStringLiteral("label")).toString(),
                             item.value(QStringLiteral("value")).toString());
            const QString evidence = item.value(QStringLiteral("evidence")).toString();
            if (!evidence.isEmpty())
                text += QStringLiteral(" (%1)").arg(evidence.simplified());
            text += u'\n';
        }
        text += u'\n';
    }
    text += QStringLiteral("Kernel: %1\n")
                .arg(m_evidence.value(QStringLiteral("kernelVersion")).toString());
    QStringList modules;
    for (const QJsonValue &module : m_evidence.value(QStringLiteral("loadedModules")).toArray())
        modules.append(module.toString());
    text += QStringLiteral("Loaded modules: %1\n").arg(modules.join(QStringLiteral(", ")));
    text += QStringLiteral("PCI devices:\n");
    for (const QVariant &device : jsonRows(m_evidence.value(QStringLiteral("pciDevices")).toArray())) {
        const QVariantMap item = device.toMap();
        text += QStringLiteral("- %1 %2:%3 class %4 driver %5\n")
                    .arg(item.value(QStringLiteral("address")).toString(),
                         item.value(QStringLiteral("vendorId")).toString(),
                         item.value(QStringLiteral("deviceId")).toString(),
                         item.value(QStringLiteral("classId")).toString(),
                         item.value(QStringLiteral("driver")).toString());
    }
    text += QStringLiteral("USB devices:\n");
    for (const QVariant &device : jsonRows(m_evidence.value(QStringLiteral("usbDevices")).toArray())) {
        const QVariantMap item = device.toMap();
        text += QStringLiteral("- %1:%2 %3 %4 driver %5\n")
                    .arg(item.value(QStringLiteral("vendorId")).toString(),
                         item.value(QStringLiteral("productId")).toString(),
                         item.value(QStringLiteral("manufacturer")).toString(),
                         item.value(QStringLiteral("product")).toString(),
                         item.value(QStringLiteral("driver")).toString());
    }
    return text.toUtf8();
}
