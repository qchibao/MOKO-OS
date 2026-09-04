#include "ptysession.h"

#include <QDir>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <csignal>
#include <cstring>

#include <fcntl.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

PtySession::PtySession(QObject *parent)
    : QObject(parent)
    , m_childTimer(new QTimer(this))
{
    m_childTimer->setInterval(200);
    connect(m_childTimer, &QTimer::timeout, this, &PtySession::checkChild);
}

PtySession::~PtySession()
{
    stop();
}

bool PtySession::running() const
{
    return m_childPid > 0;
}

bool PtySession::start(const QString &program, const QStringList &arguments)
{
    if (running()) {
        emit errorOccurred(QStringLiteral("A shell is already running."));
        return false;
    }
    const QFileInfo executable(program);
    if (!executable.isExecutable()) {
        emit errorOccurred(QStringLiteral("Shell is not executable: %1").arg(program));
        return false;
    }

    struct winsize windowSize {};
    windowSize.ws_col = static_cast<unsigned short>(m_columns);
    windowSize.ws_row = static_cast<unsigned short>(m_rows);
    const QByteArray programBytes = QFile::encodeName(program);
    QList<QByteArray> argumentBytes;
    argumentBytes.reserve(arguments.size() + 1);
    argumentBytes.append(programBytes);
    for (const QString &argument : arguments)
        argumentBytes.append(argument.toLocal8Bit());
    QVector<char *> argv;
    argv.reserve(argumentBytes.size() + 1);
    for (QByteArray &argument : argumentBytes)
        argv.append(argument.data());
    argv.append(nullptr);
    const QByteArray home = qgetenv("HOME");

    int masterFd = -1;
    const pid_t child = forkpty(&masterFd, nullptr, nullptr, &windowSize);
    if (child < 0) {
        emit errorOccurred(QStringLiteral("Could not create PTY: %1")
                               .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }
    if (child == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        if (!home.isEmpty())
            chdir(home.constData());
        execv(programBytes.constData(), argv.data());
        _exit(127);
    }

    const int flags = fcntl(masterFd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(masterFd, F_SETFL, flags | O_NONBLOCK);
    m_masterFd = masterFd;
    m_childPid = child;
    m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &PtySession::readAvailable);
    m_childTimer->start();
    emit runningChanged();
    emit started(child);
    return true;
}

bool PtySession::writeBytes(const QByteArray &bytes)
{
    if (!running() || m_masterFd < 0 || bytes.isEmpty())
        return false;

    qsizetype offset = 0;
    while (offset < bytes.size()) {
        const ssize_t written = ::write(m_masterFd,
                                        bytes.constData() + offset,
                                        static_cast<size_t>(bytes.size() - offset));
        if (written > 0) {
            offset += written;
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return false;
        emit errorOccurred(QStringLiteral("PTY write failed: %1")
                               .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }
    return true;
}

void PtySession::resize(int columns, int rows)
{
    m_columns = qMax(2, columns);
    m_rows = qMax(2, rows);
    if (m_masterFd < 0)
        return;
    struct winsize windowSize {};
    windowSize.ws_col = static_cast<unsigned short>(m_columns);
    windowSize.ws_row = static_cast<unsigned short>(m_rows);
    if (ioctl(m_masterFd, TIOCSWINSZ, &windowSize) == 0 && m_childPid > 0)
        kill(static_cast<pid_t>(m_childPid), SIGWINCH);
}

void PtySession::stop()
{
    if (m_childPid > 0) {
        const pid_t child = static_cast<pid_t>(m_childPid);
        kill(child, SIGHUP);
        int status = 0;
        bool reaped = false;
        for (int attempt = 0; attempt < 20; ++attempt) {
            if (waitpid(child, &status, WNOHANG) != 0) {
                reaped = true;
                break;
            }
            usleep(10000);
        }
        if (!reaped) {
            kill(child, SIGTERM);
            for (int attempt = 0; attempt < 20; ++attempt) {
                if (waitpid(child, &status, WNOHANG) != 0) {
                    reaped = true;
                    break;
                }
                usleep(10000);
            }
        }
        if (!reaped) {
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
        }
    }
    const bool wasRunning = running();
    m_childPid = -1;
    m_childTimer->stop();
    closeMaster();
    if (wasRunning)
        emit runningChanged();
}

void PtySession::readAvailable()
{
    if (m_masterFd < 0)
        return;
    QByteArray available;
    char buffer[8192];
    for (;;) {
        const ssize_t count = ::read(m_masterFd, buffer, sizeof(buffer));
        if (count > 0) {
            available.append(buffer, count);
            continue;
        }
        if (count < 0 && errno == EINTR)
            continue;
        break;
    }
    if (!available.isEmpty())
        emit bytesReceived(available);
}

void PtySession::checkChild()
{
    if (m_childPid <= 0)
        return;
    int status = 0;
    const pid_t result = waitpid(static_cast<pid_t>(m_childPid), &status, WNOHANG);
    if (result == 0)
        return;
    if (result < 0 && errno == EINTR)
        return;

    readAvailable();
    const int exitCode = result > 0 && WIFEXITED(status) ? WEXITSTATUS(status) : 128;
    m_childPid = -1;
    m_childTimer->stop();
    closeMaster();
    emit runningChanged();
    emit exited(exitCode);
}

void PtySession::closeMaster()
{
    if (m_readNotifier) {
        m_readNotifier->setEnabled(false);
        m_readNotifier->deleteLater();
        m_readNotifier = nullptr;
    }
    if (m_masterFd >= 0) {
        close(m_masterFd);
        m_masterFd = -1;
    }
}
