#include "systemsettings.h"

#include <QGuiApplication>
#include <QTest>

class SystemSettingsTest final : public QObject
{
    Q_OBJECT

private slots:
    void exposesAllRequiredSections();
    void returnsStructuredRealRows();
    void hidesTechnicalRowsUntilDeveloperMode();
};

void SystemSettingsTest::exposesAllRequiredSections()
{
    SystemSettings settings;
    const QStringList sections = settings.sectionIds();
    QCOMPARE(sections.size(), 12);
    for (const QString &expected : {QStringLiteral("about"), QStringLiteral("display"),
                                    QStringLiteral("appearance"), QStringLiteral("date-time"),
                                    QStringLiteral("sound"), QStringLiteral("trackpad"),
                                    QStringLiteral("network"), QStringLiteral("bluetooth"),
                                    QStringLiteral("power"), QStringLiteral("storage"),
                                    QStringLiteral("hardware"),
                                    QStringLiteral("system")}) {
        QVERIFY2(sections.contains(expected), qPrintable(expected));
    }
}

void SystemSettingsTest::hidesTechnicalRowsUntilDeveloperMode()
{
    SystemSettings settings;
    settings.setDeveloperMode(false);

    const auto labelsFor = [&settings](const QString &section) {
        QStringList labels;
        for (const QVariant &rowValue : settings.rows(section))
            labels.append(rowValue.toMap().value(QStringLiteral("label")).toString());
        return labels;
    };

    QVERIFY(!labelsFor(QStringLiteral("sound")).contains(QStringLiteral("Audio service details")));
    QVERIFY(!labelsFor(QStringLiteral("network")).contains(QStringLiteral("Connection service")));
    QVERIFY(!labelsFor(QStringLiteral("system")).contains(QStringLiteral("Qt version")));

    settings.setDeveloperMode(true);
    QVERIFY(labelsFor(QStringLiteral("sound")).contains(QStringLiteral("Audio service details")));
    QVERIFY(labelsFor(QStringLiteral("network")).contains(QStringLiteral("Connection service")));
    QVERIFY(labelsFor(QStringLiteral("system")).contains(QStringLiteral("Qt version")));
}

void SystemSettingsTest::returnsStructuredRealRows()
{
    SystemSettings settings;
    for (const QString &section : settings.sectionIds()) {
        const QVariantList rows = settings.rows(section);
        QVERIFY2(!rows.isEmpty(), qPrintable(section));
        for (const QVariant &rowValue : rows) {
            const QVariantMap row = rowValue.toMap();
            QVERIFY(row.contains(QStringLiteral("label")));
            QVERIFY(row.contains(QStringLiteral("value")));
            QVERIFY(row.contains(QStringLiteral("available")));
            QVERIFY(row.contains(QStringLiteral("writable")));
            QVERIFY(!row.value(QStringLiteral("label")).toString().isEmpty());
            QVERIFY(!row.value(QStringLiteral("value")).toString().isEmpty());
        }
    }
    QVERIFY(!settings.refreshedAt().isEmpty());
    QVERIFY(settings.timeZoneChoices().contains(QStringLiteral("Asia/Ho_Chi_Minh")));
    QVERIFY(!settings.localDateTime().isEmpty());
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    SystemSettingsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_systemsettings.moc"
