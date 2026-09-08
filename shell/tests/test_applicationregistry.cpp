#include "applicationregistry.h"
#include "applicationservice.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

void writeDesktopFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qPrintable(file.errorString()));
    QCOMPARE(file.write(contents), contents.size());
}

} // namespace

class ApplicationRegistryTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsAndFiltersDesktopEntries();
    void launchesWithoutShellInterpretation();
    void reportsLaunchFailure();
    void reloadsThroughApplicationService();
};

void ApplicationRegistryTest::readsAndFiltersDesktopEntries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    writeDesktopFile(directory.filePath(QStringLiteral("org.moko.Files.desktop")), R"DESKTOP(
[Desktop Entry]
Type=Application
Name=MOKO Files
Comment=Browse local files
Exec=/bin/echo %c %F
Icon=system-file-manager
Categories=System;FileManager;
Keywords=folders;documents;storage;
X-MOKO-Pinned=true
X-MOKO-PinOrder=10
X-MOKO-Glyph=files
)DESKTOP");
    writeDesktopFile(directory.filePath(QStringLiteral("hidden.desktop")), R"DESKTOP(
[Desktop Entry]
Type=Application
Name=Hidden Application
Exec=/bin/true
NoDisplay=true
)DESKTOP");
    writeDesktopFile(directory.filePath(QStringLiteral("invalid.desktop")), R"DESKTOP(
[Desktop Entry]
Type=Application
Name=Invalid Application
Exec=/bin/true %Z
)DESKTOP");

    ApplicationRegistry registry(nullptr, {directory.path()});
    QCOMPARE(registry.rowCount(), 1);
    const QVariantMap app = registry.application(QStringLiteral("org.moko.Files"));
    QCOMPARE(app.value(QStringLiteral("displayName")).toString(), QStringLiteral("MOKO Files"));
    QCOMPARE(app.value(QStringLiteral("category")).toString(), QStringLiteral("System"));
    QVERIFY(app.value(QStringLiteral("pinned")).toBool());

    ApplicationFilterModel search;
    search.setSourceModel(&registry);
    QCOMPARE(search.rowCount(), 1);
    search.setSearchText(QStringLiteral("documents storage"));
    QCOMPARE(search.rowCount(), 1);
    search.setSearchText(QStringLiteral("browser"));
    QCOMPARE(search.rowCount(), 0);
}

void ApplicationRegistryTest::launchesWithoutShellInterpretation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString marker = directory.filePath(QStringLiteral("created;still-one-argument"));
    const QByteArray desktop = QStringLiteral(R"DESKTOP(
[Desktop Entry]
Type=Application
Name=Safe Launch
Exec=/usr/bin/touch "%1"
)DESKTOP")
                                   .arg(marker)
                                   .toUtf8();
    writeDesktopFile(directory.filePath(QStringLiteral("safe.desktop")), desktop);

    ApplicationRegistry registry(nullptr, {directory.path()});
    QSignalSpy runningSpy(&registry, &ApplicationRegistry::applicationRunning);
    QSignalSpy stoppedSpy(&registry, &ApplicationRegistry::applicationStopped);
    QVERIFY(registry.launch(QStringLiteral("safe")));
    QTRY_VERIFY_WITH_TIMEOUT(runningSpy.count() > 0, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(marker), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(stoppedSpy.count(), 1, 3000);
    QCOMPARE(stoppedSpy.constFirst().at(2).toInt(), 0);
    QVERIFY(!QFile::exists(directory.filePath(QStringLiteral("still-one-argument"))));
}

void ApplicationRegistryTest::reportsLaunchFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    writeDesktopFile(directory.filePath(QStringLiteral("missing.desktop")), R"DESKTOP(
[Desktop Entry]
Type=Application
Name=Missing Application
Exec=/definitely/not/a/moko-program
)DESKTOP");

    ApplicationRegistry registry(nullptr, {directory.path()});
    QSignalSpy failureSpy(&registry, &ApplicationRegistry::applicationFailed);
    QVERIFY(registry.launch(QStringLiteral("missing")));
    QTRY_COMPARE_WITH_TIMEOUT(failureSpy.count(), 1, 3000);
    QCOMPARE(registry.application(QStringLiteral("missing"))
                 .value(QStringLiteral("launchState"))
                 .toString(),
             QStringLiteral("failed"));
}

void ApplicationRegistryTest::reloadsThroughApplicationService()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationRegistry registry(nullptr, {directory.path()});
    ApplicationService service(&registry);
    QCOMPARE(registry.rowCount(), 0);

    writeDesktopFile(directory.filePath(QStringLiteral("installed.desktop")), R"DESKTOP(
[Desktop Entry]
Type=Application
Name=Installed Application
Exec=/bin/true
)DESKTOP");
    const QVariantMap result = service.reloadApplications();
    QVERIFY(result.value(QStringLiteral("ok")).toBool());
    QCOMPARE(result.value(QStringLiteral("count")).toInt(), 1);
    QCOMPARE(registry.rowCount(), 1);
}

QTEST_GUILESS_MAIN(ApplicationRegistryTest)

#include "test_applicationregistry.moc"
