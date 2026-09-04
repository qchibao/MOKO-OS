#include "aidaemon.h"
#include "aiactions.h"
#include "localstubprovider.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

class AiTests final : public QObject
{
    Q_OBJECT

private slots:
    void providerProducesOnlyKnownActions();
    void actionLayerEnforcesApplicationAllowlist();
    void fileSearchAndOpenStayInsideHome();
    void daemonDispatchesSafeRequests();
};

void AiTests::providerProducesOnlyKnownActions()
{
    LocalStubProvider provider;
    QCOMPARE(provider.interpret(QStringLiteral("open moko files")).action,
             QStringLiteral("open_application"));
    QCOMPARE(provider.interpret(QStringLiteral("system overview")).action,
             QStringLiteral("system_summary"));
    QCOMPARE(provider.interpret(QStringLiteral("find report")).action,
             QStringLiteral("search_files"));
    QCOMPARE(provider.interpret(QStringLiteral("execute shell rm -rf / ")).action,
             QStringLiteral("refuse"));
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
        return QVariantMap{{QStringLiteral("ok"), appId == QStringLiteral("org.moko.Files")},
                           {QStringLiteral("message"), QStringLiteral("accepted")}};
    });
    AiDaemon daemon(&actions, std::make_unique<LocalStubProvider>());
    QCOMPARE(daemon.ping(), QStringLiteral("pong"));
    QVERIFY(daemon.request(QStringLiteral("open files")).value(QStringLiteral("ok")).toBool());
    QVERIFY(!daemon.request(QStringLiteral("run shell command"))
                 .value(QStringLiteral("ok")).toBool());
    QVERIFY(!daemon.getSystemSummary().value(QStringLiteral("kernel")).toString().isEmpty());
}

QTEST_GUILESS_MAIN(AiTests)
#include "test_ai.moc"
