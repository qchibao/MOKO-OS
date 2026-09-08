#include "packageinstaller.h"

#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSysInfo>
#include <QStandardPaths>

PackageInstaller::PackageInstaller(QObject *parent)
    : QObject(parent)
{
}

QString PackageInstaller::packagePath() const { return m_packagePath; }
QString PackageInstaller::packageType() const { return m_packageType; }
QString PackageInstaller::packageName() const { return m_packageName; }
QString PackageInstaller::version() const { return m_version; }
QString PackageInstaller::architecture() const { return m_architecture; }
QString PackageInstaller::installedSize() const { return m_installedSize; }
QString PackageInstaller::fileSize() const { return m_fileSize; }
QString PackageInstaller::description() const { return m_description; }
QString PackageInstaller::maintainer() const { return m_maintainer; }
QString PackageInstaller::trustMessage() const { return m_trustMessage; }
QString PackageInstaller::packageHash() const { return m_packageHash; }
QString PackageInstaller::state() const { return m_state; }
QString PackageInstaller::statusMessage() const { return m_statusMessage; }
QString PackageInstaller::developerDetails() const { return m_developerDetails; }
bool PackageInstaller::installable() const
{
    return m_packageType == QStringLiteral("deb") && !busy()
        && (m_state == QStringLiteral("ready") || m_state == QStringLiteral("failed")
            || m_state == QStringLiteral("cancelled"));
}
bool PackageInstaller::busy() const { return m_process != nullptr; }

void PackageInstaller::clearPackage()
{
    m_packagePath.clear();
    m_packageType.clear();
    m_packageName.clear();
    m_version.clear();
    m_architecture.clear();
    m_installedSize.clear();
    m_fileSize.clear();
    m_description.clear();
    m_maintainer.clear();
    m_trustMessage.clear();
    m_packageHash.clear();
    m_developerDetails.clear();
}

void PackageInstaller::setState(const QString &state, const QString &message)
{
    if (m_state == state && m_statusMessage == message)
        return;
    m_state = state;
    m_statusMessage = message;
    emit stateChanged();
}

bool PackageInstaller::isSafeLocalPath(const QString &path, QString *canonicalPath) const
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || !info.isReadable() || info.isSymLink())
        return false;
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty() || canonicalPath == nullptr)
        return false;
    *canonicalPath = canonical;
    return true;
}

QString PackageInstaller::formatBytes(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    if (bytes < 1024 * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
    return QStringLiteral("%1 GB").arg(QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1));
}

QString PackageInstaller::fieldValue(const QHash<QString, QString> &fields, const QString &key)
{
    return fields.value(key).trimmed();
}

bool PackageInstaller::inspect(const QString &path)
{
    cancel();
    clearPackage();
    emit packageChanged();

    QString canonical;
    if (!isSafeLocalPath(path, &canonical)) {
        setState(QStringLiteral("failed"), QStringLiteral("The selected file is not a readable local file."));
        return false;
    }
    m_packagePath = canonical;
    m_fileSize = formatBytes(QFileInfo(canonical).size());
    QFile file(canonical);
    if (file.open(QIODevice::ReadOnly)) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!file.atEnd())
            hash.addData(file.read(1024 * 1024));
        m_packageHash = QString::fromLatin1(hash.result().toHex());
    }

    if (canonical.endsWith(QStringLiteral(".rpm"), Qt::CaseInsensitive)) {
        m_packageType = QStringLiteral("rpm");
        m_trustMessage = QStringLiteral("RPM packages are not supported on Debian.");
        emit packageChanged();
        setState(QStringLiteral("unsupported"), QStringLiteral("This package format is not supported on MOKO OS."));
        return false;
    }
    if (!canonical.endsWith(QStringLiteral(".deb"), Qt::CaseInsensitive)) {
        m_packageType = QStringLiteral("unknown");
        m_trustMessage = QStringLiteral("Only Debian .deb packages can be installed.");
        emit packageChanged();
        setState(QStringLiteral("unsupported"), QStringLiteral("This file is not a supported package."));
        return false;
    }
    return inspectDebianPackage(canonical);
}

bool PackageInstaller::inspectDebianPackage(const QString &path)
{
    QProcess process;
    process.setProgram(QStandardPaths::findExecutable(QStringLiteral("dpkg-deb")));
    if (process.program().isEmpty()) {
        setState(QStringLiteral("failed"), QStringLiteral("Debian package tools are unavailable."));
        return false;
    }
    process.setArguments({QStringLiteral("--field"), path});
    process.start();
    if (!process.waitForFinished(5000) || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        setState(QStringLiteral("failed"), QStringLiteral("The selected file is not a valid Debian package."));
        return false;
    }

    QHash<QString, QString> fields;
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    for (const QString &line : output.split(u'\n')) {
        const int separator = line.indexOf(u':');
        if (separator <= 0)
            continue;
        fields.insert(line.left(separator).trimmed(), line.mid(separator + 1).trimmed());
    }
    m_packageType = QStringLiteral("deb");
    m_packageName = fieldValue(fields, QStringLiteral("Package"));
    m_version = fieldValue(fields, QStringLiteral("Version"));
    m_architecture = fieldValue(fields, QStringLiteral("Architecture"));
    const QString hostArchitecture = QSysInfo::currentCpuArchitecture() == QStringLiteral("x86_64")
        ? QStringLiteral("amd64") : QSysInfo::currentCpuArchitecture();
    if (m_packageName.isEmpty() || m_version.isEmpty()
        || (m_architecture != QStringLiteral("all") && m_architecture != hostArchitecture)) {
        m_trustMessage = QStringLiteral("This package is not compatible with this system architecture.");
        emit packageChanged();
        setState(QStringLiteral("unsupported"), QStringLiteral("This Debian package cannot run on this system."));
        return false;
    }
    const QString installedKiB = fieldValue(fields, QStringLiteral("Installed-Size"));
    m_installedSize = installedKiB.isEmpty() ? QStringLiteral("Unknown")
                                             : QStringLiteral("%1 KB").arg(installedKiB);
    m_description = fieldValue(fields, QStringLiteral("Description"));
    if (m_description.isEmpty())
        m_description = QStringLiteral("No description provided.");
    m_maintainer = fieldValue(fields, QStringLiteral("Maintainer"));
    if (m_maintainer.isEmpty())
        m_maintainer = QStringLiteral("Unknown publisher");
    m_trustMessage = QStringLiteral("Package signature and publisher could not be independently verified. Review before installing.");
    emit packageChanged();
    setState(QStringLiteral("ready"), QStringLiteral("Package details inspected. Installation requires your confirmation."));
    return true;
}

bool PackageInstaller::install()
{
    if (!installable()) {
        if (m_packageType == QStringLiteral("rpm"))
            setState(QStringLiteral("unsupported"), QStringLiteral("RPM packages cannot be installed on Debian."));
        else
            setState(QStringLiteral("failed"), QStringLiteral("Inspect a supported Debian package first."));
        return false;
    }
    if (m_packageHash.isEmpty() || m_packagePath.isEmpty()) {
        setState(QStringLiteral("failed"), QStringLiteral("The package could not be verified."));
        return false;
    }

    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    if (pkexec.isEmpty()) {
        setState(QStringLiteral("failed"), QStringLiteral("Authorization service is unavailable in this session."));
        emit installFinished(false, m_statusMessage);
        return false;
    }
    const QString helper = QStringLiteral("/usr/local/libexec/moko-package-install-helper");
    m_process = new QProcess(this);
    m_process->setProgram(pkexec);
    m_process->setArguments({helper, m_packagePath, m_packageHash});
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::finished, this, &PackageInstaller::processFinished);
    connect(m_process, &QProcess::errorOccurred, this, &PackageInstaller::processError);
    m_process->start();
    setState(QStringLiteral("installing"), QStringLiteral("Waiting for authorization..."));
    return true;
}

void PackageInstaller::cancel()
{
    if (!m_process)
        return;
    QProcess *process = m_process;
    m_process = nullptr;
    disconnect(process, nullptr, this, nullptr);
    process->kill();
    process->deleteLater();
    setState(QStringLiteral("cancelled"), QStringLiteral("Installation cancelled. No system changes were made."));
}

void PackageInstaller::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_process)
        return;
    QProcess *process = m_process;
    const QString output = QString::fromLocal8Bit(process->readAll());
    const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
    m_developerDetails = output.trimmed().left(4000);
    const QString message = success
        ? QStringLiteral("Package installed. Newly provided applications are now available.")
        : (exitCode == 126 || exitCode == 127
               ? QStringLiteral("Installation was not authorized or was cancelled. No package was installed.")
               : QStringLiteral("The package could not be installed. Open Developer Details for technical information."));
    m_process = nullptr;
    disconnect(process, nullptr, this, nullptr);
    process->deleteLater();
    if (success) {
        QDBusInterface applications(QStringLiteral("org.moko.Applications1"),
                                    QStringLiteral("/org/moko/Applications1"),
                                    QStringLiteral("org.moko.Applications1"),
                                    QDBusConnection::sessionBus());
        if (applications.isValid())
            applications.call(QStringLiteral("reloadApplications"));
    }
    setState(success ? QStringLiteral("installed") : QStringLiteral("failed"), message);
    emit installFinished(success, message);
}

void PackageInstaller::processError(QProcess::ProcessError error)
{
    if (!m_process)
        return;
    QProcess *process = m_process;
    m_process = nullptr;
    disconnect(process, nullptr, this, nullptr);
    process->deleteLater();
    m_developerDetails = QStringLiteral("The authorization helper could not be started (process error %1).")
                             .arg(static_cast<int>(error));
    setState(QStringLiteral("failed"), QStringLiteral("Authorization or package installation could not be started."));
    emit installFinished(false, m_statusMessage);
}
