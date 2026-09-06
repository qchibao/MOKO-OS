#include "notificationmodel.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QProcess>
#include <QTest>
#include <QTimer>

namespace {

constexpr auto serviceName = "org.freedesktop.Notifications";
constexpr auto objectPath = "/org/freedesktop/Notifications";
constexpr auto interfaceName = "org.freedesktop.Notifications";

int runService(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    NotificationModel model;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(QString::fromLatin1(serviceName))
        || !bus.registerObject(QString::fromLatin1(objectPath), &model,
                               QDBusConnection::ExportAllSlots
                                   | QDBusConnection::ExportAllSignals)) {
        return 2;
    }
    QTimer::singleShot(10000, &app, &QCoreApplication::quit);
    return app.exec();
}

}

class NotificationDbusTest final : public QObject
{
    Q_OBJECT

private slots:
    void exportsStandardNotificationContract();
};

void NotificationDbusTest::exportsStandardNotificationContract()
{
    QProcess service;
    service.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--service")});
    QVERIFY(service.waitForStarted());

    QDBusConnection bus = QDBusConnection::sessionBus();
    QVERIFY(bus.isConnected());
    QTRY_VERIFY_WITH_TIMEOUT(bus.interface()->isServiceRegistered(
                                 QString::fromLatin1(serviceName)), 3000);

    QDBusInterface notifications(QString::fromLatin1(serviceName),
                                 QString::fromLatin1(objectPath),
                                 QString::fromLatin1(interfaceName), bus);
    QVERIFY(notifications.isValid());
    const QDBusReply<QStringList> capabilities = notifications.call(
        QStringLiteral("GetCapabilities"));
    QVERIFY(capabilities.isValid());
    QVERIFY(capabilities.value().contains(QStringLiteral("actions")));

    const QDBusReply<uint> notified = notifications.call(
        QStringLiteral("Notify"),
        QStringLiteral("MOKO D-Bus Test"),
        0U,
        QString(),
        QStringLiteral("Notification contract"),
        QStringLiteral("Delivered through the session bus"),
        QStringList({QStringLiteral("open"), QStringLiteral("Open")}),
        QVariantMap(),
        0);
    QVERIFY2(notified.isValid(), qPrintable(notified.error().message()));
    QVERIFY(notified.value() > 0);

    QDBusInterface introspection(QString::fromLatin1(serviceName),
                                QString::fromLatin1(objectPath),
                                QStringLiteral("org.freedesktop.DBus.Introspectable"), bus);
    const QDBusReply<QString> xml = introspection.call(QStringLiteral("Introspect"));
    QVERIFY(xml.isValid());
    QVERIFY(xml.value().contains(QStringLiteral("method name=\"Notify\"")));
    QVERIFY(xml.value().contains(QStringLiteral("signal name=\"NotificationClosed\"")));
    QVERIFY(xml.value().contains(QStringLiteral("signal name=\"ActionInvoked\"")));

    const QDBusMessage information = notifications.call(QStringLiteral("GetServerInformation"));
    QCOMPARE(information.type(), QDBusMessage::ReplyMessage);
    QCOMPARE(information.arguments().size(), 4);
    QCOMPARE(information.arguments().at(0).toString(),
             QStringLiteral("MOKO Notification Center"));

    const QDBusMessage closeReply = notifications.call(
        QStringLiteral("CloseNotification"), notified.value());
    QCOMPARE(closeReply.type(), QDBusMessage::ReplyMessage);

    service.terminate();
    QVERIFY(service.waitForFinished(3000));
}

int main(int argc, char **argv)
{
    if (argc > 1 && QByteArray(argv[1]) == QByteArray("--service"))
        return runService(argc, argv);
    QCoreApplication app(argc, argv);
    NotificationDbusTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_notificationdbus.moc"
