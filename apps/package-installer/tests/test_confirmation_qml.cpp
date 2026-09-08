#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTest>

class MockPackageInstaller final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString packageType MEMBER packageType CONSTANT)
    Q_PROPERTY(QString packageName MEMBER packageName CONSTANT)
    Q_PROPERTY(QString version MEMBER version CONSTANT)
    Q_PROPERTY(QString architecture MEMBER architecture CONSTANT)
    Q_PROPERTY(QString installedSize MEMBER installedSize CONSTANT)
    Q_PROPERTY(QString fileSize MEMBER fileSize CONSTANT)
    Q_PROPERTY(QString description MEMBER description CONSTANT)
    Q_PROPERTY(QString maintainer MEMBER maintainer CONSTANT)
    Q_PROPERTY(QString trustMessage MEMBER trustMessage CONSTANT)
    Q_PROPERTY(QString state MEMBER state NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage MEMBER statusMessage NOTIFY stateChanged)
    Q_PROPERTY(QString developerDetails MEMBER developerDetails NOTIFY stateChanged)
    Q_PROPERTY(bool installable MEMBER installable NOTIFY stateChanged)
    Q_PROPERTY(bool busy MEMBER busy NOTIFY stateChanged)

public:
    QString packageType = QStringLiteral("deb");
    QString packageName = QStringLiteral("moko-test");
    QString version = QStringLiteral("1.0");
    QString architecture = QStringLiteral("amd64");
    QString installedSize = QStringLiteral("1 KB");
    QString fileSize = QStringLiteral("1 KB");
    QString description = QStringLiteral("Keyboard confirmation test");
    QString maintainer = QStringLiteral("MOKO Test");
    QString trustMessage = QStringLiteral("Test package");
    QString state = QStringLiteral("ready");
    QString statusMessage = QStringLiteral("Ready for confirmation");
    QString developerDetails;
    bool installable = true;
    bool busy = false;
    int installCalls = 0;

    Q_INVOKABLE bool install()
    {
        ++installCalls;
        return true;
    }

    Q_INVOKABLE void cancel() {}

signals:
    void stateChanged();
    void installFinished(bool success, const QString &message);
};

class PackageInstallerConfirmationTest final : public QObject
{
    Q_OBJECT

private slots:
    void enterRequiresTwoDistinctSteps();
};

void PackageInstallerConfirmationTest::enterRequiresTwoDistinctSteps()
{
    MockPackageInstaller installer;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("mokoPackageInstaller"), &installer);
    engine.load(QUrl::fromLocalFile(QStringLiteral(MOKO_PACKAGE_INSTALLER_QML)));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window);
    QTRY_VERIFY(window->isVisible());
    QObject *dialog = window->findChild<QObject *>(QStringLiteral("confirmDialog"));
    QVERIFY(dialog);
    QVERIFY(!dialog->property("visible").toBool());

    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(dialog->property("visible").toBool());
    QCOMPARE(installer.installCalls, 0);

    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(!dialog->property("visible").toBool());
    QCOMPARE(installer.installCalls, 1);
}

QTEST_MAIN(PackageInstallerConfirmationTest)

#include "test_confirmation_qml.moc"
