#include "screenshotcontroller.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

void writeExecutable(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
             qPrintable(file.errorString()));
    QCOMPARE(file.write(contents), contents.size());
    file.close();
    QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                | QFileDevice::ExeOwner));
}

}

class ScreenshotControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void reportsSuccessfulCapture();
    void reportsCaptureFailure();

private:
    QByteArray m_previousGrim;
    QByteArray m_previousDirectory;
    QByteArray m_previousEvents;
    bool m_hadGrim = false;
    bool m_hadDirectory = false;
    bool m_hadEvents = false;
};

void ScreenshotControllerTest::init()
{
    m_hadGrim = qEnvironmentVariableIsSet("MOKO_GRIM");
    m_hadDirectory = qEnvironmentVariableIsSet("MOKO_SCREENSHOT_DIR");
    m_hadEvents = qEnvironmentVariableIsSet("MOKO_LIVE_LAUNCH_EVENTS");
    m_previousGrim = qgetenv("MOKO_GRIM");
    m_previousDirectory = qgetenv("MOKO_SCREENSHOT_DIR");
    m_previousEvents = qgetenv("MOKO_LIVE_LAUNCH_EVENTS");
}

void ScreenshotControllerTest::cleanup()
{
    const auto restore = [](const char *name, bool existed, const QByteArray &value) {
        if (existed)
            qputenv(name, value);
        else
            qunsetenv(name);
    };
    restore("MOKO_GRIM", m_hadGrim, m_previousGrim);
    restore("MOKO_SCREENSHOT_DIR", m_hadDirectory, m_previousDirectory);
    restore("MOKO_LIVE_LAUNCH_EVENTS", m_hadEvents, m_previousEvents);
}

void ScreenshotControllerTest::reportsSuccessfulCapture()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fakeGrim = directory.filePath(QStringLiteral("grim"));
    writeExecutable(fakeGrim, "#!/bin/sh\nprintf 'moko-png' > \"$1\"\n");
    const QString events = directory.filePath(QStringLiteral("events.log"));
    qputenv("MOKO_GRIM", fakeGrim.toUtf8());
    qputenv("MOKO_SCREENSHOT_DIR", directory.filePath(QStringLiteral("shots")).toUtf8());
    qputenv("MOKO_LIVE_LAUNCH_EVENTS", events.toUtf8());

    ScreenshotController controller;
    QSignalSpy saved(&controller, &ScreenshotController::screenshotSaved);
    QVERIFY(controller.captureFullScreen());
    QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 1, 2000);
    QCOMPARE(controller.state(), QStringLiteral("Saved"));
    QVERIFY(QFileInfo(controller.lastPath()).size() > 0);

    QFile eventFile(events);
    QVERIFY(eventFile.open(QIODevice::ReadOnly));
    const QByteArray eventData = eventFile.readAll();
    QVERIFY(eventData.contains("MOKO_SCREENSHOT state=capturing"));
    QVERIFY(eventData.contains("MOKO_SCREENSHOT state=saved"));
}

void ScreenshotControllerTest::reportsCaptureFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fakeGrim = directory.filePath(QStringLiteral("grim"));
    writeExecutable(fakeGrim, "#!/bin/sh\nexit 7\n");
    qputenv("MOKO_GRIM", fakeGrim.toUtf8());
    qputenv("MOKO_SCREENSHOT_DIR", directory.filePath(QStringLiteral("shots")).toUtf8());

    ScreenshotController controller;
    QSignalSpy failed(&controller, &ScreenshotController::screenshotFailed);
    QVERIFY(controller.captureFullScreen());
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 2000);
    QCOMPARE(controller.state(), QStringLiteral("Failed"));
    QVERIFY(!controller.statusMessage().isEmpty());
}

QTEST_GUILESS_MAIN(ScreenshotControllerTest)

#include "test_screenshotcontroller.moc"
