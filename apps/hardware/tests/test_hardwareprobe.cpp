#include "hardwareprobe.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

class HardwareProbeTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesRequiredCategoriesAndEvidence();
    void exportsPrivacySafeJsonAndText();
    void exposesConsumerStatusLabelsAndTechnicalRows();
};

void HardwareProbeTest::exposesRequiredCategoriesAndEvidence()
{
    HardwareProbe probe;
    probe.refresh();
    const QStringList expected = {QStringLiteral("system"), QStringLiteral("cpu"),
                                  QStringLiteral("memory"), QStringLiteral("graphics"),
                                  QStringLiteral("storage"), QStringLiteral("network"),
                                  QStringLiteral("bluetooth"), QStringLiteral("audio"),
                                  QStringLiteral("input"), QStringLiteral("power"),
                                  QStringLiteral("mac")};
    QCOMPARE(probe.sectionIds(), expected);
    const QStringList statuses = {QStringLiteral("SUPPORTED"), QStringLiteral("PARTIAL"),
                                  QStringLiteral("UNKNOWN"), QStringLiteral("UNSUPPORTED")};
    QVERIFY(statuses.contains(probe.overallStatus()));
    for (const QString &section : expected) {
        QVERIFY(!probe.sectionTitle(section).isEmpty());
        QVERIFY(statuses.contains(probe.sectionStatus(section)));
        QVERIFY(!probe.sectionSummary(section).isEmpty());
        QVERIFY(!probe.rows(section).isEmpty());
    }
    const QJsonObject evidence = probe.report().value(QStringLiteral("evidence")).toObject();
    QVERIFY(!evidence.value(QStringLiteral("kernelVersion")).toString().isEmpty());
    QVERIFY(evidence.value(QStringLiteral("loadedModules")).isArray());
    QVERIFY(evidence.value(QStringLiteral("pciDevices")).isArray());
    QVERIFY(evidence.value(QStringLiteral("usbDevices")).isArray());
}

void HardwareProbeTest::exposesConsumerStatusLabelsAndTechnicalRows()
{
    HardwareProbe probe;
    probe.refresh();

    const QStringList labels = {QStringLiteral("Working"), QStringLiteral("Limited"),
                                QStringLiteral("Not detected"), QStringLiteral("Unsupported")};
    QVERIFY(labels.contains(probe.overallDisplayStatus()));
    for (const QString &section : probe.sectionIds())
        QVERIFY(labels.contains(probe.sectionDisplayStatus(section)));

    bool foundTechnicalRow = false;
    for (const QVariant &rowValue : probe.rows(QStringLiteral("graphics")))
        foundTechnicalRow = foundTechnicalRow || rowValue.toMap().value(QStringLiteral("technical")).toBool();
    QVERIFY(foundTechnicalRow);
}

void HardwareProbeTest::exportsPrivacySafeJsonAndText()
{
    HardwareProbe probe;
    probe.refresh();
    QTemporaryDir output;
    QVERIFY(output.isValid());
    QVERIFY(probe.exportJsonToDirectory(output.path()));
    QVERIFY(probe.exportTextToDirectory(output.path()));

    QFile jsonFile(output.filePath(QStringLiteral("moko-hardware-report.json")));
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    const QByteArray json = jsonFile.readAll();
    const QJsonDocument document = QJsonDocument::fromJson(json);
    QVERIFY(document.isObject());
    QCOMPARE(document.object().value(QStringLiteral("schema")).toString(),
             QStringLiteral("org.moko.hardware-report.v1"));

    QFile textFile(output.filePath(QStringLiteral("moko-hardware-report.txt")));
    QVERIFY(textFile.open(QIODevice::ReadOnly));
    const QByteArray text = textFile.readAll();
    QVERIFY(text.contains("MOKO Hardware Report"));
    QVERIFY(text.contains("Loaded modules:"));
    QVERIFY(text.contains("PCI devices:"));
    QVERIFY(text.contains("USB devices:"));

    const QByteArray lower = (json + text).toLower();
    QVERIFY(!lower.contains("product_serial"));
    QVERIFY(!lower.contains("product_uuid"));
    QVERIFY(!lower.contains("macaddress"));
    QVERIFY(!lower.contains("hostname"));
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    HardwareProbeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_hardwareprobe.moc"
