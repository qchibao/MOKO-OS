#include "packageinstaller.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

class PackageInstallerTest final : public QObject
{
    Q_OBJECT

private slots:
    void inspectsDebianMetadata();
    void rejectsUnsupportedAndUnsafeFiles();
};

void PackageInstallerTest::inspectsDebianMetadata()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString root = temporary.filePath(QStringLiteral("sample"));
    QVERIFY(QDir().mkpath(QDir(root).filePath(QStringLiteral("DEBIAN"))));
    QVERIFY(QDir().mkpath(QDir(root).filePath(QStringLiteral("usr/share/doc/moko-sample"))));

    QFile control(QDir(root).filePath(QStringLiteral("DEBIAN/control")));
    QVERIFY(control.open(QIODevice::WriteOnly));
    control.write("Package: moko-sample\n"
                  "Version: 1.2.3\n"
                  "Architecture: all\n"
                  "Maintainer: MOKO Test <test@moko.invalid>\n"
                  "Installed-Size: 12\n"
                  "Description: Safe package inspection fixture\n");
    control.close();
    QFile readme(QDir(root).filePath(QStringLiteral("usr/share/doc/moko-sample/README")));
    QVERIFY(readme.open(QIODevice::WriteOnly));
    readme.write("fixture\n");
    readme.close();

    const QString package = temporary.filePath(QStringLiteral("moko-sample_1.2.3_all.deb"));
    QCOMPARE(QProcess::execute(QStringLiteral("dpkg-deb"),
                               {QStringLiteral("--build"), root, package}), 0);

    PackageInstaller installer;
    QVERIFY(installer.inspect(package));
    QCOMPARE(installer.state(), QStringLiteral("ready"));
    QCOMPARE(installer.packageType(), QStringLiteral("deb"));
    QCOMPARE(installer.packageName(), QStringLiteral("moko-sample"));
    QCOMPARE(installer.version(), QStringLiteral("1.2.3"));
    QCOMPARE(installer.architecture(), QStringLiteral("all"));
    QCOMPARE(installer.installedSize(), QStringLiteral("12 KB"));
    QVERIFY(installer.description().contains(QStringLiteral("Safe package")));
    QVERIFY(installer.maintainer().contains(QStringLiteral("MOKO Test")));
    QCOMPARE(installer.packageHash().size(), 64);
    QVERIFY(installer.installable());
}

void PackageInstallerTest::rejectsUnsupportedAndUnsafeFiles()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QFile rpm(temporary.filePath(QStringLiteral("sample.rpm")));
    QVERIFY(rpm.open(QIODevice::WriteOnly));
    rpm.write("not-an-rpm\n");
    rpm.close();

    PackageInstaller installer;
    QVERIFY(!installer.inspect(rpm.fileName()));
    QCOMPARE(installer.state(), QStringLiteral("unsupported"));
    QCOMPARE(installer.packageType(), QStringLiteral("rpm"));
    QVERIFY(!installer.installable());

    const QString link = temporary.filePath(QStringLiteral("linked.deb"));
    QVERIFY(QFile::link(rpm.fileName(), link));
    QVERIFY(!installer.inspect(link));
    QCOMPARE(installer.state(), QStringLiteral("failed"));

    QFile invalid(temporary.filePath(QStringLiteral("invalid.deb")));
    QVERIFY(invalid.open(QIODevice::WriteOnly));
    invalid.write("not-a-deb\n");
    invalid.close();
    QVERIFY(!installer.inspect(invalid.fileName()));
    QCOMPARE(installer.state(), QStringLiteral("failed"));
}

QTEST_MAIN(PackageInstallerTest)

#include "test_packageinstaller.moc"
