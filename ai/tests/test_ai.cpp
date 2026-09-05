#include "aidaemon.h"
#include "aiactions.h"
#include "localstubprovider.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

class UnavailableProvider final : public AiProvider
{
public:
    QString name() const override { return QStringLiteral("unavailable-test"); }
    bool isAvailable() const override { return false; }
    QString unavailableMessage() const override
    {
        return QStringLiteral("Test provider is not configured.");
    }
    AiIntent interpret(const QString &) const override { return {}; }
};

} // namespace

class AiTests final : public QObject
{
    Q_OBJECT

private slots:
    void providerProducesOnlyKnownActions();
    void providerRecognizesRequiredIntents_data();
    void providerRecognizesRequiredIntents();
    void actionLayerEnforcesApplicationAllowlist();
    void fileSearchAndOpenStayInsideHome();
    void daemonDispatchesSafeRequests();
    void daemonReportsUnavailableProvider();
};

void AiTests::providerProducesOnlyKnownActions()
{
    LocalStubProvider provider;
    QCOMPARE(provider.interpret(QStringLiteral("open moko files")).action,
             QStringLiteral("open_application"));
    QCOMPARE(provider.interpret(QStringLiteral("open hardware diagnostics"))
                 .parameters.value(QStringLiteral("appId")).toString(),
             QStringLiteral("org.moko.HardwareDiagnostics"));
    QCOMPARE(provider.interpret(QStringLiteral("system overview")).action,
             QStringLiteral("system_summary"));
    QCOMPARE(provider.interpret(QStringLiteral("find report")).action,
             QStringLiteral("search_files"));
    QCOMPARE(provider.interpret(QStringLiteral("execute shell rm -rf / ")).action,
             QStringLiteral("refuse"));
}

void AiTests::providerRecognizesRequiredIntents_data()
{
    QTest::addColumn<QString>("prompt");
    QTest::addColumn<QString>("action");
    QTest::addColumn<QString>("appId");

    QTest::newRow("files") << QStringLiteral("please open MOKO Files")
                            << QStringLiteral("open_application")
                            << QStringLiteral("org.moko.Files");
    QTest::newRow("settings") << QStringLiteral("launch settings")
                               << QStringLiteral("open_application")
                               << QStringLiteral("org.moko.Settings");
    QTest::newRow("terminal") << QStringLiteral("start the terminal")
                               << QStringLiteral("open_application")
                               << QStringLiteral("org.moko.Terminal");
    QTest::newRow("battery") << QStringLiteral("what is my battery status")
                              << QStringLiteral("battery_status") << QString();
    QTest::newRow("network") << QStringLiteral("show network status")
                              << QStringLiteral("network_status") << QString();
    QTest::newRow("storage") << QStringLiteral("how much free space is available")
                              << QStringLiteral("storage_status") << QString();
    QTest::newRow("system") << QStringLiteral("show computer info")
                             << QStringLiteral("system_summary") << QString();
}

void AiTests::providerRecognizesRequiredIntents()
{
    QFETCH(QString, prompt);
    QFETCH(QString, action);
    QFETCH(QString, appId);

    LocalStubProvider provider;
    const AiIntent intent = provider.interpret(prompt);
    QVERIFY(intent.valid);
    QCOMPARE(intent.action, action);
    if (!appId.isEmpty()) {
        QCOMPARE(intent.parameters.value(QStringLiteral("appId")).toString(), appId);
    }
}

void AiTests::actionLayerEnforcesApplicationAllowlist()
{
    QString launched;
    AiActions actions({}, [&launched](const QString &appId) {
        launched = appId;
        return QVariantMap{{QStringLiteral("ok"), true},
                           {QStringLiteral("message"), QStringLiteral("accepted")}};
    });
    QVERIFY(actions.openApplication(QStringLiteral("org.moko.Files"))
                .value(QStringLiteral("ok")).toBool());
    QCOMPARE(launched, QStringLiteral("org.moko.Files"));
    QVERIFY(!actions.openApplication(QStringLiteral("org.example.Untrusted"))
                 .value(QStringLiteral("ok")).toBool());
}

void AiTests::fileSearchAndOpenStayInsideHome()
{
    QTemporaryDir home;
    QTemporaryDir outside;
    QVERIFY(home.isValid());
    QVERIFY(outside.isValid());
    QFile report(home.filePath(QStringLiteral("hardware-report.txt")));
    QVERIFY(report.open(QIODevice::WriteOnly));
    report.write("MOKO");
    report.close();
    QFile secret(outside.filePath(QStringLiteral("secret.txt")));
    QVERIFY(secret.open(QIODevice::WriteOnly));
    secret.write("private");
    secret.close();

    QString opened;
    AiActions actions(home.path(), {}, [&opened](const QString &path) {
        opened = path;
        return true;
    });
    QCOMPARE(actions.searchFiles(QStringLiteral("hardware")).size(), 1);
    QVERIFY(actions.openFile(report.fileName()).value(QStringLiteral("ok")).toBool());
    QCOMPARE(opened, QFileInfo(report).canonicalFilePath());
    QVERIFY(!actions.isPathAllowed(secret.fileName()));
    QVERIFY(!actions.openFile(secret.fileName()).value(QStringLiteral("ok")).toBool());
}

void AiTests::daemonDispatchesSafeRequests()
{
    AiActions actions({}, [](const QString &appId) {
        return QVariantMap{{QStringLiteral("ok"), appId.startsWith(QStringLiteral("org.moko."))},
                           {QStringLiteral("message"), QStringLiteral("accepted")}};
    });
    AiDaemon daemon(&actions, std::make_unique<LocalStubProvider>());
    QCOMPARE(daemon.ping(), QStringLiteral("pong"));
    QVERIFY(daemon.providerStatus().value(QStringLiteral("available")).toBool());
    QVERIFY(daemon.request(QStringLiteral("open files")).value(QStringLiteral("ok")).toBool());
    QVERIFY(daemon.request(QStringLiteral("launch settings")).value(QStringLiteral("ok")).toBool());
    QVERIFY(daemon.request(QStringLiteral("start terminal")).value(QStringLiteral("ok")).toBool());
    QVERIFY(daemon.request(QStringLiteral("battery status")).value(QStringLiteral("ok")).toBool());
    QVERIFY(daemon.request(QStringLiteral("network status")).value(QStringLiteral("ok")).toBool());
    QVERIFY(daemon.request(QStringLiteral("storage status")).value(QStringLiteral("ok")).toBool());
    QVERIFY(daemon.request(QStringLiteral("system information")).value(QStringLiteral("ok")).toBool());
    QVERIFY(!daemon.request(QStringLiteral("run shell command"))
                 .value(QStringLiteral("ok")).toBool());
    QVERIFY(!daemon.getSystemSummary().value(QStringLiteral("kernel")).toString().isEmpty());
}

void AiTests::daemonReportsUnavailableProvider()
{
    AiActions actions;
    AiDaemon daemon(&actions, std::make_unique<UnavailableProvider>());
    const QVariantMap status = daemon.providerStatus();
    QCOMPARE(status.value(QStringLiteral("name")).toString(),
             QStringLiteral("unavailable-test"));
    QVERIFY(!status.value(QStringLiteral("available")).toBool());

    const QVariantMap response = daemon.request(QStringLiteral("system information"));
    QVERIFY(!response.value(QStringLiteral("ok")).toBool());
    QCOMPARE(response.value(QStringLiteral("action")).toString(),
             QStringLiteral("provider_unavailable"));
}

QTEST_GUILESS_MAIN(AiTests)
#include "test_ai.moc"
