#include "hardwareprobe.h"
#include "livemarker.h"
#include "mokofilepickermodel.h"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSysInfo>
#include <QTimer>

#include <unistd.h>

namespace {

QString safeMarkerValue(QString value)
{
    for (QChar &character : value) {
        if (!character.isLetterOrNumber() && character != u'.' && character != u'-'
            && character != u'_') {
            character = u'_';
        }
    }
    return value.left(100);
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("MOKO"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("moko.asia"));
    QGuiApplication::setApplicationName(QStringLiteral("MOKO Hardware Diagnostics"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.moko.HardwareDiagnostics"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Load the QML UI and exit.")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Save a validation screenshot and exit."),
                      QStringLiteral("path")});
    parser.addOption({QStringLiteral("export-directory"),
                      QStringLiteral("Override the report directory for validation."),
                      QStringLiteral("path")});
    parser.process(app);
    const bool smokeTest = parser.isSet(QStringLiteral("smoke-test"));
    const QString screenshotPath = parser.value(QStringLiteral("screenshot"));
    const bool validationMode = smokeTest || !screenshotPath.isEmpty();
    if (geteuid() == 0 && !validationMode) {
        qCritical("MOKO Hardware Diagnostics refuses to run as root.");
        return 77;
    }

    HardwareProbe probe;
    MokoFilePickerModel filePicker;
    if (parser.isSet(QStringLiteral("export-directory")))
        probe.setExportDirectory(parser.value(QStringLiteral("export-directory")));
    QObject::connect(&probe, &HardwareProbe::exportWritten, &app,
                     [](const QString &format, const QString &, qint64 bytes) {
        writeMokoLiveEvent(QStringLiteral("MOKO_HW_EXPORT format=%1 state=written bytes=%2 uid=%3")
                               .arg(format).arg(bytes).arg(geteuid()));
    });

    bool qmlWarningsFound = false;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("mokoHardware"), &probe);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoFilePicker"), &filePicker);
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
        qmlWarningsFound = qmlWarningsFound || !warnings.isEmpty();
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("MokoHardwareDiagnostics"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // Let Wayland map the window before the synchronous read-only hardware scan begins.
    QTimer::singleShot(350, &app, [&probe]() {
        probe.refresh();
        writeMokoLiveEvent(QStringLiteral("MOKO_HW_REPORT state=ready overall=%1 manufacturer=%2 model=%3 architecture=%4 renderer=%5 writable_disk_detected=%6")
                               .arg(probe.overallStatus(), safeMarkerValue(probe.manufacturer()),
                                    safeMarkerValue(probe.model()), safeMarkerValue(QSysInfo::currentCpuArchitecture()),
                                    safeMarkerValue(probe.activeRenderer()))
                               .arg(probe.writableDiskDetected() ? 1 : 0));
        writeMokoLiveEvent(QStringLiteral("MOKO_APP_READY app_id=org.moko.HardwareDiagnostics state=ready detail=report-collected"));
    });

    if (validationMode) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QTimer::singleShot(1800, &app, [&, window]() {
            const bool saved = screenshotPath.isEmpty()
                || (window && QFileInfo(screenshotPath).dir().exists()
                    && window->grabWindow().save(screenshotPath));
            app.exit(qmlWarningsFound || !saved ? 2 : 0);
        });
    }
    return app.exec();
}
