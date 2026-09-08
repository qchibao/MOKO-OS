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
#include <QTimeZone>

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

QSettings sharedSettings()
{
    return QSettings(QStringLiteral("MOKO"), QStringLiteral("MOKO OS"));
}

} // namespace

SystemSettings::SystemSettings(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_developerMode = settings.value(QStringLiteral("developerMode"), false).toBool();
    m_twentyFourHour = sharedSettings().value(
        QStringLiteral("dateTime/twentyFourHour"), false).toBool();
    QDBusConnection::sessionBus().connect(QStringLiteral("org.moko.Desktop1"),
                                          QStringLiteral("/org/moko/Desktop1"),
                                          QStringLiteral("org.moko.Desktop1"),
                                          QStringLiteral("inputChanged"),
                                          this,
                                          SLOT(refresh()));
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

QString SystemSettings::timeZoneId() const { return m_timeZoneId; }
QString SystemSettings::localDateTime() const { return m_localDateTime; }
bool SystemSettings::automaticTime() const { return m_automaticTime; }
bool SystemSettings::automaticTimeAvailable() const { return m_automaticTimeAvailable; }
bool SystemSettings::twentyFourHour() const { return m_twentyFourHour; }
bool SystemSettings::trackpadAvailable() const { return m_trackpadAvailable; }
int SystemSettings::trackpadCount() const { return m_trackpadCount; }
bool SystemSettings::naturalScrollAvailable() const { return m_naturalScrollAvailable; }
bool SystemSettings::naturalScrollEnabled() const { return m_naturalScrollEnabled; }
bool SystemSettings::threeFingerDragAvailable() const { return m_threeFingerDragAvailable; }
bool SystemSettings::threeFingerDragEnabled() const { return m_threeFingerDragEnabled; }
bool SystemSettings::browserHistorySwipeAvailable() const
{
    return m_browserHistorySwipeAvailable;
}
bool SystemSettings::browserHistorySwipeEnabled() const
{
    return m_browserHistorySwipeEnabled;
}
QString SystemSettings::appearanceMode() const { return m_appearanceMode; }
QStringList SystemSettings::appearanceModes() const
{
    return {QStringLiteral("light"), QStringLiteral("dark"), QStringLiteral("glass")};
}
bool SystemSettings::safeGraphics() const { return m_safeGraphics; }
bool SystemSettings::glassEffectsEnabled() const { return m_glassEffectsEnabled; }

QStringList SystemSettings::timeZoneChoices() const
{
    QStringList choices;
    for (const QByteArray &id : QTimeZone::availableTimeZoneIds())
        choices.append(QString::fromUtf8(id));
    choices.sort(Qt::CaseInsensitive);
    return choices;
}

void SystemSettings::setTwentyFourHour(bool enabled)
{
    if (m_twentyFourHour == enabled)
        return;
    m_twentyFourHour = enabled;
    sharedSettings().setValue(QStringLiteral("dateTime/twentyFourHour"), enabled);
    collectDateTime();
    emit dateTimeChanged();
    emit dataChanged();
}

QStringList SystemSettings::sectionIds() const
{
    return {QStringLiteral("about"),      QStringLiteral("display"),
            QStringLiteral("appearance"), QStringLiteral("trackpad"),
            QStringLiteral("date-time"),
            QStringLiteral("sound"),
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
        {QStringLiteral("trackpad"), QStringLiteral("Trackpad")},
        {QStringLiteral("date-time"), QStringLiteral("Date & Time")},
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
        {QStringLiteral("trackpad"), QStringLiteral("Scrolling, pointer and window gestures")},
        {QStringLiteral("date-time"), QStringLiteral("Clock, network time and time zone")},
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
    collectTrackpad();
    collectDateTime();
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

void SystemSettings::collectTrackpad()
{
    QDBusInterface desktop(QStringLiteral("org.moko.Desktop1"),
                           QStringLiteral("/org/moko/Desktop1"),
                           QStringLiteral("org.moko.Desktop1"),
                           QDBusConnection::sessionBus());
    const QDBusReply<QVariantMap> reply = desktop.call(QStringLiteral("inputState"));
    const QVariantMap state = reply.isValid() ? reply.value() : QVariantMap{};
    m_trackpadAvailable = state.value(QStringLiteral("touchpadAvailable")).toBool();
    m_trackpadCount = state.value(QStringLiteral("touchpadCount")).toInt();
    m_naturalScrollAvailable = state.value(QStringLiteral("naturalScrollAvailable")).toBool();
    m_naturalScrollEnabled = state.value(QStringLiteral("naturalScrollEnabled")).toBool();
    m_threeFingerDragAvailable = state.value(
        QStringLiteral("threeFingerDragAvailable")).toBool();
    m_threeFingerDragEnabled = state.value(
        QStringLiteral("threeFingerDragEnabled")).toBool();
    m_browserHistorySwipeAvailable = state.value(
        QStringLiteral("browserHistorySwipeAvailable")).toBool();
    m_browserHistorySwipeEnabled = state.value(
        QStringLiteral("browserHistorySwipeEnabled")).toBool();
    m_rows.insert(QStringLiteral("trackpad"),
                  {row(QStringLiteral("Trackpad"),
                       m_trackpadAvailable
                           ? QStringLiteral("%1 detected").arg(m_trackpadCount)
                           : QStringLiteral("Not detected"),
                       reply.isValid() ? QStringLiteral("MOKO compositor input state")
                                       : QStringLiteral("MOKO desktop service unavailable"),
                       m_trackpadAvailable),
                   row(QStringLiteral("Tap to click"),
                       state.value(QStringLiteral("tapToClickEnabled")).toBool()
                           ? QStringLiteral("On") : QStringLiteral("Unavailable")),
                   row(QStringLiteral("Two-finger scrolling"),
                       state.value(QStringLiteral("twoFingerScrollEnabled")).toBool()
                           ? QStringLiteral("On") : QStringLiteral("Unavailable")),
                   row(QStringLiteral("Secondary click"),
                       state.value(QStringLiteral("secondaryClickEnabled")).toBool()
                           ? QStringLiteral("Two fingers") : QStringLiteral("Unavailable")),
                   row(QStringLiteral("Palm rejection"),
                       state.value(QStringLiteral("palmRejectionManaged")).toBool()
                           ? QStringLiteral("Automatic") : QStringLiteral("Unavailable"))});
    emit trackpadChanged();
}

bool SystemSettings::setNaturalScrollEnabled(bool enabled)
{
    QDBusInterface desktop(QStringLiteral("org.moko.Desktop1"),
                           QStringLiteral("/org/moko/Desktop1"),
                           QStringLiteral("org.moko.Desktop1"),
                           QDBusConnection::sessionBus());
    const QDBusReply<bool> reply = desktop.call(
        QStringLiteral("setNaturalScrollEnabled"), enabled);
    m_statusMessage = reply.isValid() && reply.value()
        ? QStringLiteral("Natural scrolling updated")
        : QStringLiteral("Natural scrolling could not be changed");
    collectTrackpad();
    emit dataChanged();
    return reply.isValid() && reply.value();
}

bool SystemSettings::setThreeFingerDragEnabled(bool enabled)
{
    QDBusInterface desktop(QStringLiteral("org.moko.Desktop1"),
                           QStringLiteral("/org/moko/Desktop1"),
                           QStringLiteral("org.moko.Desktop1"),
                           QDBusConnection::sessionBus());
    const QDBusReply<bool> reply = desktop.call(
        QStringLiteral("setThreeFingerDragEnabled"), enabled);
    m_statusMessage = reply.isValid() && reply.value()
        ? QStringLiteral("Three-finger window drag updated")
        : QStringLiteral("Three-finger window drag could not be changed");
    collectTrackpad();
    emit dataChanged();
    return reply.isValid() && reply.value();
}

bool SystemSettings::setBrowserHistorySwipeEnabled(bool enabled)
{
    QDBusInterface desktop(QStringLiteral("org.moko.Desktop1"),
                           QStringLiteral("/org/moko/Desktop1"),
                           QStringLiteral("org.moko.Desktop1"),
                           QDBusConnection::sessionBus());
    const QDBusReply<bool> reply = desktop.call(
        QStringLiteral("setBrowserHistorySwipeEnabled"), enabled);
    m_statusMessage = reply.isValid() && reply.value()
        ? QStringLiteral("Browser history swipe updated")
        : QStringLiteral("Browser history swipe could not be changed");
    collectTrackpad();
    emit dataChanged();
    return reply.isValid() && reply.value();
}

bool SystemSettings::setAppearanceMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (!appearanceModes().contains(normalized)) {
        m_statusMessage = QStringLiteral("That appearance mode is not available");
        emit dataChanged();
        return false;
    }

    QDBusInterface desktop(QStringLiteral("org.moko.Desktop1"),
                           QStringLiteral("/org/moko/Desktop1"),
                           QStringLiteral("org.moko.Desktop1"),
                           QDBusConnection::sessionBus());
    bool applied = false;
    if (desktop.isValid()) {
        const QDBusReply<bool> reply = desktop.call(
            QStringLiteral("setAppearanceMode"), normalized);
        applied = reply.isValid() && reply.value();
    } else {
        sharedSettings().setValue(QStringLiteral("appearance/mode"), normalized);
        applied = true;
    }
    if (!applied) {
        m_statusMessage = QStringLiteral("Appearance could not be changed");
        emit dataChanged();
        return false;
    }

    m_appearanceMode = normalized;
    m_safeGraphics = qEnvironmentVariableIntValue("MOKO_SAFE_GRAPHICS") == 1;
    m_glassEffectsEnabled = normalized == QStringLiteral("glass") && !m_safeGraphics;
    m_statusMessage = m_safeGraphics && normalized == QStringLiteral("glass")
        ? QStringLiteral("Glass selected with reduced effects in Safe Graphics")
        : QStringLiteral("Appearance changed to MOKO %1")
              .arg(normalized.left(1).toUpper() + normalized.mid(1));
    collectAppearance();
    emit appearanceChanged();
    emit dataChanged();
    return true;
}

bool SystemSettings::setTimeZone(const QString &zoneId)
{
    const QByteArray encoded = zoneId.trimmed().toUtf8();
    if (!QTimeZone::isTimeZoneIdAvailable(encoded)) {
        m_statusMessage = QStringLiteral("That time zone is not available");
        emit dataChanged();
        return false;
    }

    QDBusInterface timedate(QStringLiteral("org.freedesktop.timedate1"),
                            QStringLiteral("/org/freedesktop/timedate1"),
                            QStringLiteral("org.freedesktop.timedate1"),
                            QDBusConnection::systemBus());
    bool systemChanged = false;
    if (timedate.isValid()) {
        timedate.setTimeout(1500);
        const QDBusMessage reply = timedate.call(QStringLiteral("SetTimezone"), zoneId, true);
        systemChanged = reply.type() == QDBusMessage::ReplyMessage;
    }

    sharedSettings().setValue(QStringLiteral("dateTime/timeZone"), zoneId);
    m_statusMessage = systemChanged
        ? QStringLiteral("Time zone changed to %1").arg(zoneId)
        : QStringLiteral("Using %1 for this Live session").arg(zoneId);
    collectDateTime();
    emit dateTimeChanged();
    emit dataChanged();
    return true;
}

bool SystemSettings::setAutomaticTime(bool enabled)
{
    QDBusInterface timedate(QStringLiteral("org.freedesktop.timedate1"),
                            QStringLiteral("/org/freedesktop/timedate1"),
                            QStringLiteral("org.freedesktop.timedate1"),
                            QDBusConnection::systemBus());
    if (!timedate.isValid() || !timedate.property("CanNTP").toBool()) {
        m_statusMessage = QStringLiteral("Automatic network time is unavailable");
        emit dataChanged();
        return false;
    }
    timedate.setTimeout(1500);
    const QDBusMessage reply = timedate.call(QStringLiteral("SetNTP"), enabled, true);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        m_statusMessage = QStringLiteral("Could not change automatic network time");
        emit dataChanged();
        return false;
    }
    m_statusMessage = enabled ? QStringLiteral("Automatic network time enabled")
                              : QStringLiteral("Automatic network time disabled");
    collectDateTime();
    emit dateTimeChanged();
    emit dataChanged();
    return true;
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
    const QString sysfsRoot = qEnvironmentVariable("MOKO_SYSFS_ROOT", QStringLiteral("/sys"));
    const QDir drm(QDir(sysfsRoot).filePath(QStringLiteral("class/drm")));
    for (const QString &connector : drm.entryList(QStringList() << QStringLiteral("card*-*"),
                                                   QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString status = readTextFile(drm.filePath(connector + QStringLiteral("/status")));
        if (status.compare(QStringLiteral("connected"), Qt::CaseInsensitive) != 0)
            continue;
        QFile modesFile(drm.filePath(connector + QStringLiteral("/modes")));
        if (!modesFile.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        QStringList modes;
        while (!modesFile.atEnd()) {
            const QString mode = QString::fromUtf8(modesFile.readLine()).trimmed();
            if (!mode.isEmpty() && !modes.contains(mode))
                modes.append(mode);
        }
        if (!modes.isEmpty())
            result.append(row(QStringLiteral("Available modes (%1)").arg(connector),
                              modes.join(QStringLiteral(", ")),
                              QStringLiteral("DRM modes exposed by the connected display"),
                              true, false));
    }
    result.append(row(QStringLiteral("Display backend"), QGuiApplication::platformName(),
                      QStringLiteral("Qt platform plugin"), true, false, true));
    m_rows.insert(QStringLiteral("display"), result);
}

void SystemSettings::collectAppearance()
{
    QDBusInterface desktop(QStringLiteral("org.moko.Desktop1"),
                           QStringLiteral("/org/moko/Desktop1"),
                           QStringLiteral("org.moko.Desktop1"),
                           QDBusConnection::sessionBus());
    const QDBusReply<QVariantMap> reply = desktop.call(QStringLiteral("appearanceState"));
    const QVariantMap state = reply.isValid() ? reply.value() : QVariantMap{};
    const QString stored = sharedSettings().value(
        QStringLiteral("appearance/mode"), QStringLiteral("light")).toString().toLower();
    m_appearanceMode = appearanceModes().contains(state.value(QStringLiteral("mode")).toString())
        ? state.value(QStringLiteral("mode")).toString()
        : appearanceModes().contains(stored) ? stored : QStringLiteral("light");
    m_safeGraphics = state.contains(QStringLiteral("safeGraphics"))
        ? state.value(QStringLiteral("safeGraphics")).toBool()
        : qEnvironmentVariableIntValue("MOKO_SAFE_GRAPHICS") == 1;
    m_glassEffectsEnabled = state.contains(QStringLiteral("glassEffectsEnabled"))
        ? state.value(QStringLiteral("glassEffectsEnabled")).toBool()
        : m_appearanceMode == QStringLiteral("glass") && !m_safeGraphics;
    const QString displayMode = m_appearanceMode.left(1).toUpper() + m_appearanceMode.mid(1);
    m_rows.insert(QStringLiteral("appearance"),
                  {row(QStringLiteral("Theme"), QStringLiteral("MOKO %1").arg(displayMode),
                       QStringLiteral("Applied to the current MOKO session"), true, true),
                   row(QStringLiteral("Accent"), QStringLiteral("Blue"),
                       QStringLiteral("MOKO design token #3F7CFF")),
                   row(QStringLiteral("Glass effects"),
                       m_glassEffectsEnabled ? QStringLiteral("On")
                           : m_safeGraphics && m_appearanceMode == QStringLiteral("glass")
                               ? QStringLiteral("Reduced") : QStringLiteral("Off"),
                       m_safeGraphics ? QStringLiteral("Safe Graphics uses opaque surfaces")
                                      : QStringLiteral("Hardware rendering available"))});
    emit appearanceChanged();
}

void SystemSettings::collectDateTime()
{
    const QString configured = sharedSettings().value(
        QStringLiteral("dateTime/timeZone")).toString();
    const QByteArray systemId = QTimeZone::systemTimeZoneId();
    const QByteArray selectedId = QTimeZone::isTimeZoneIdAvailable(configured.toUtf8())
        ? configured.toUtf8() : systemId;
    const QTimeZone zone(selectedId);
    const QDateTime now = QDateTime::currentDateTimeUtc().toTimeZone(zone);
    const QLocale locale = QLocale::system();
    const QString time = m_twentyFourHour
        ? now.toString(QStringLiteral("HH:mm:ss"))
        : locale.toString(now.time(), QLocale::LongFormat);
    m_timeZoneId = selectedId.isEmpty() ? QStringLiteral("UTC")
                                        : QString::fromUtf8(selectedId);
    m_localDateTime = QStringLiteral("%1, %2")
                          .arg(locale.toString(now.date(), QLocale::LongFormat), time);

    QDBusInterface timedate(QStringLiteral("org.freedesktop.timedate1"),
                            QStringLiteral("/org/freedesktop/timedate1"),
                            QStringLiteral("org.freedesktop.timedate1"),
                            QDBusConnection::systemBus());
    m_automaticTimeAvailable = timedate.isValid() && timedate.property("CanNTP").toBool();
    m_automaticTime = timedate.isValid() && timedate.property("NTP").toBool();
    m_rows.insert(QStringLiteral("date-time"),
                  {row(QStringLiteral("Local date and time"), m_localDateTime),
                   row(QStringLiteral("Time zone"), m_timeZoneId,
                       configured.isEmpty() ? QStringLiteral("System time zone")
                                            : QStringLiteral("Selected for this Live session"),
                       true, true),
                   row(QStringLiteral("Automatic network time"),
                       m_automaticTime ? QStringLiteral("On")
                                       : m_automaticTimeAvailable ? QStringLiteral("Off")
                                                                  : QStringLiteral("Unavailable"),
                       QStringLiteral("systemd-timedated over the system D-Bus"),
                       m_automaticTimeAvailable, m_automaticTimeAvailable),
                   row(QStringLiteral("Clock format"),
                       m_twentyFourHour ? QStringLiteral("24-hour")
                                           : QStringLiteral("12-hour"),
                       {}, true, true)});
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
