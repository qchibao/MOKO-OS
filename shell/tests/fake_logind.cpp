#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusError>
#include <QDBusUnixFileDescriptor>
#include <QThread>

#include <fcntl.h>

class FakeLoginManager final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.login1.Manager")

public slots:
    QString CanSuspend() const
    {
        if (m_canSuspendDelayMs > 0)
            QThread::msleep(static_cast<unsigned long>(m_canSuspendDelayMs));
        return QStringLiteral("yes");
    }
    QString CanReboot() const { return QStringLiteral("yes"); }
    QString CanPowerOff() const { return QStringLiteral("yes"); }

    QDBusUnixFileDescriptor Inhibit(const QString &what,
                                    const QString &who,
                                    const QString &why,
                                    const QString &mode)
    {
        m_lastInhibitWhat = what;
        m_lastInhibitWho = who;
        m_lastInhibitWhy = why;
        m_lastInhibitMode = mode;
        if (m_failInhibit) {
            sendErrorReply(QDBusError::Failed, QStringLiteral("Test inhibitor failure"));
            return {};
        }
        QDBusUnixFileDescriptor descriptor;
        descriptor.giveFileDescriptor(open("/dev/null", O_RDONLY | O_CLOEXEC));
        return descriptor;
    }

    void Suspend(bool interactive)
    {
        Q_UNUSED(interactive)
        m_lastAction = QStringLiteral("suspend");
    }

    void Reboot(bool interactive)
    {
        Q_UNUSED(interactive)
        if (m_failPowerAction) {
            sendErrorReply(QDBusError::Failed, QStringLiteral("Test power action failure"));
            return;
        }
        m_lastAction = QStringLiteral("reboot");
    }

    void PowerOff(bool interactive)
    {
        Q_UNUSED(interactive)
        if (m_failPowerAction) {
            sendErrorReply(QDBusError::Failed, QStringLiteral("Test power action failure"));
            return;
        }
        m_lastAction = QStringLiteral("poweroff");
    }

    void SetInhibitFailure(bool fail) { m_failInhibit = fail; }
    void SetPowerActionFailure(bool fail) { m_failPowerAction = fail; }
    void SetCanSuspendDelay(int milliseconds) { m_canSuspendDelayMs = qMax(0, milliseconds); }

    QString LastAction() const { return m_lastAction; }
    QString LastInhibitWhat() const { return m_lastInhibitWhat; }
    QString LastInhibitWho() const { return m_lastInhibitWho; }
    QString LastInhibitWhy() const { return m_lastInhibitWhy; }
    QString LastInhibitMode() const { return m_lastInhibitMode; }

private:
    QString m_lastAction;
    QString m_lastInhibitWhat;
    QString m_lastInhibitWho;
    QString m_lastInhibitWhy;
    QString m_lastInhibitMode;
    bool m_failInhibit = false;
    bool m_failPowerAction = false;
    int m_canSuspendDelayMs = 0;
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QDBusConnection bus = QDBusConnection::systemBus();
    FakeLoginManager manager;
    if (!bus.registerService(QStringLiteral("org.freedesktop.login1"))
        || !bus.registerObject(QStringLiteral("/org/freedesktop/login1"),
                               &manager,
                               QDBusConnection::ExportAllSlots)) {
        return 1;
    }
    return app.exec();
}

#include "fake_logind.moc"
