#include "screenshotcontroller.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <unistd.h>

namespace {

void writeLiveEvent(const QString &state, const QString &path = {})
{
    const QString eventPath = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (eventPath.isEmpty())
        return;
    QDir().mkpath(QFileInfo(eventPath).absolutePath());
    QFile file(eventPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(QStringLiteral("MOKO_SCREENSHOT state=%1 file=%2 uid=%3\n")
                   .arg(state,
                        path.isEmpty() ? QStringLiteral("none")
                                       : QFileInfo(path).fileName().left(160))
                   .arg(static_cast<qulonglong>(geteuid()))
                   .toUtf8());
}

}

ScreenshotController::ScreenshotController(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            fail(QStringLiteral("Screenshot service could not be started."));
    });
    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (m_state != QStringLiteral("Capturing"))
            return;
        const QFileInfo output(m_lastPath);
        if (exitStatus != QProcess::NormalExit || exitCode != 0
            || !output.exists() || output.size() == 0) {
            fail(QStringLiteral("The screenshot could not be saved."));
            return;
        }
        m_state = QStringLiteral("Saved");
        m_statusMessage = QStringLiteral("Saved to %1").arg(QDir::toNativeSeparators(m_lastPath));
        writeLiveEvent(QStringLiteral("saved"), m_lastPath);
        emit stateChanged();
        emit screenshotSaved(m_lastPath);
    });
}

bool ScreenshotController::busy() const
{
    return m_process->state() != QProcess::NotRunning;
}

QString ScreenshotController::state() const { return m_state; }
QString ScreenshotController::lastPath() const { return m_lastPath; }
QString ScreenshotController::statusMessage() const { return m_statusMessage; }

bool ScreenshotController::captureFullScreen()
{
    if (busy())
        return false;
    const QString program = qEnvironmentVariableIsSet("MOKO_GRIM")
        ? qEnvironmentVariable("MOKO_GRIM")
        : QStandardPaths::findExecutable(QStringLiteral("grim"));
    if (program.isEmpty()) {
        fail(QStringLiteral("Screenshot support is unavailable."));
        return false;
    }

    const QString directory = screenshotDirectory();
    if (directory.isEmpty() || !QDir().mkpath(directory)) {
        fail(QStringLiteral("The Screenshots folder is not writable."));
        return false;
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH-mm-ss"));
    QString path = QDir(directory).filePath(QStringLiteral("Screenshot %1.png").arg(stamp));
    int suffix = 2;
    while (QFileInfo::exists(path)) {
        path = QDir(directory).filePath(
            QStringLiteral("Screenshot %1 (%2).png").arg(stamp).arg(suffix++));
    }

    m_lastPath = path;
    m_state = QStringLiteral("Capturing");
    m_statusMessage = QStringLiteral("Capturing screenshot...");
    writeLiveEvent(QStringLiteral("capturing"), m_lastPath);
    emit stateChanged();
    m_process->start(program, {path});
    return true;
}

void ScreenshotController::fail(const QString &message)
{
    m_state = QStringLiteral("Failed");
    m_statusMessage = message;
    writeLiveEvent(QStringLiteral("failed"), m_lastPath);
    emit stateChanged();
    emit screenshotFailed(message);
}

QString ScreenshotController::screenshotDirectory() const
{
    const QString override = qEnvironmentVariable("MOKO_SCREENSHOT_DIR");
    if (!override.isEmpty())
        return QDir::cleanPath(QFileInfo(override).absoluteFilePath());
    QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (pictures.isEmpty())
        pictures = QDir(QDir::homePath()).filePath(QStringLiteral("Pictures"));
    return QDir(pictures).filePath(QStringLiteral("Screenshots"));
}
