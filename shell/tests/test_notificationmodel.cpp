#include "notificationmodel.h"

#include <QSignalSpy>
#include <QTest>

class NotificationModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void tracksUnreadDismissAndCloseReasons();
    void replacesNotificationsWithoutUsingStaleExpiry();
    void invokesOnlyDeclaredActions();
};

void NotificationModelTest::tracksUnreadDismissAndCloseReasons()
{
    NotificationModel model;
    QSignalSpy closed(&model, &NotificationModel::NotificationClosed);

    const uint first = model.Notify(QStringLiteral("Files"), 0, {},
                                    QStringLiteral("Copy complete"), {}, {}, {}, 0);
    const uint second = model.Notify(QStringLiteral("Browser"), 0, {},
                                     QStringLiteral("Download complete"), {}, {}, {}, 0);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.unreadCount(), 2);

    model.markAllRead();
    QCOMPARE(model.unreadCount(), 0);
    QVERIFY(model.dismiss(0));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(closed.size(), 1);
    QCOMPARE(closed.at(0).at(0).toUInt(), second);
    QCOMPARE(closed.at(0).at(1).toUInt(), 2U);

    model.CloseNotification(first);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(closed.size(), 2);
    QCOMPARE(closed.at(1).at(0).toUInt(), first);
    QCOMPARE(closed.at(1).at(1).toUInt(), 3U);
}

void NotificationModelTest::replacesNotificationsWithoutUsingStaleExpiry()
{
    NotificationModel model;
    QSignalSpy closed(&model, &NotificationModel::NotificationClosed);
    const uint id = model.Notify(QStringLiteral("Browser"), 0, {},
                                 QStringLiteral("Starting"), {}, {}, {}, 80);
    QTest::qWait(20);
    QCOMPARE(model.Notify(QStringLiteral("Browser"), id, {},
                          QStringLiteral("Finished"), {}, {}, {}, 260), id);

    QTest::qWait(100);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), NotificationModel::SummaryRole).toString(),
             QStringLiteral("Finished"));
    QVERIFY(model.data(model.index(0), NotificationModel::ActiveRole).toBool());
    QCOMPARE(closed.size(), 0);

    QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 400);
    QCOMPARE(closed.at(0).at(0).toUInt(), id);
    QCOMPARE(closed.at(0).at(1).toUInt(), 1U);
    QVERIFY(!model.data(model.index(0), NotificationModel::ActiveRole).toBool());
}

void NotificationModelTest::invokesOnlyDeclaredActions()
{
    NotificationModel model;
    QSignalSpy actionInvoked(&model, &NotificationModel::ActionInvoked);
    const uint id = model.Notify(QStringLiteral("MOKO"), 0, {},
                                 QStringLiteral("Ready"), {},
                                 {QStringLiteral("open"), QStringLiteral("Open Files")}, {}, 0);
    QVERIFY(!model.invokeAction(0, QStringLiteral("delete")));
    QVERIFY(model.invokeAction(0, QStringLiteral("open")));
    QCOMPARE(actionInvoked.size(), 1);
    QCOMPARE(actionInvoked.at(0).at(0).toUInt(), id);
    QCOMPARE(actionInvoked.at(0).at(1).toString(), QStringLiteral("open"));

    QCOMPARE(model.GetCapabilities(),
             QStringList({QStringLiteral("actions"), QStringLiteral("body")}));
    QString name;
    QString vendor;
    QString version;
    QString specVersion;
    model.GetServerInformation(name, vendor, version, specVersion);
    QCOMPARE(name, QStringLiteral("MOKO Notification Center"));
    QCOMPARE(vendor, QStringLiteral("MOKO"));
    QCOMPARE(specVersion, QStringLiteral("1.2"));
}

QTEST_GUILESS_MAIN(NotificationModelTest)

#include "test_notificationmodel.moc"
