#include "browsercontroller.h"
#include "livemarker.h"
#include "mokofilepickermodel.h"

#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTimer>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include <unistd.h>

#include <memory>

namespace {

bool hasUnsafeWebEngineConfiguration(int argc, char *argv[])
{
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        if (argument == QStringLiteral("--no-sandbox")
            || argument.startsWith(QStringLiteral("--no-sandbox="))
            || argument == QStringLiteral("--disable-web-security")
            || argument.startsWith(QStringLiteral("--disable-web-security="))) {
            return true;
        }
    }
    if (qEnvironmentVariableIsSet("QTWEBENGINE_DISABLE_SANDBOX"))
        return true;
    const QString chromiumFlags = qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS");
    return chromiumFlags.contains(
        QRegularExpression(QStringLiteral("(^|\\s)--(no-sandbox|disable-web-security)(=|\\s|$)")));
}

} // namespace

int main(int argc, char *argv[])
{
    if (hasUnsafeWebEngineConfiguration(argc, argv)) {
        qCritical("MOKO Browser refuses to disable the WebEngine sandbox or web security.");
        return 64;
    }
    if (geteuid() == 0) {
        qCritical("MOKO Browser refuses to run as root.");
        return 77;
    }

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("MOKO"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("moko.asia"));
    QGuiApplication::setApplicationName(QStringLiteral("MOKO Browser"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.moko.Browser"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Load a local validation page and exit.")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Save a validation screenshot and exit."),
                      QStringLiteral("path")});
    parser.addOption({QStringLiteral("url"), QStringLiteral("Open an HTTP(S) URL or search query."),
                      QStringLiteral("url")});
    parser.addOption({QStringLiteral("validation-download"),
                      QStringLiteral("Download an HTTPS URL and exit after validation."),
                      QStringLiteral("url")});
    parser.addPositionalArgument(QStringLiteral("urls"), QStringLiteral("HTTP(S) URL to open."),
                                 QStringLiteral("[url]"));
    parser.process(app);

    QString initialInput = parser.value(QStringLiteral("url"));
    if (initialInput.isEmpty() && !parser.positionalArguments().isEmpty())
        initialInput = parser.positionalArguments().constFirst();
    const bool smokeTest = parser.isSet(QStringLiteral("smoke-test"));
    const QString screenshotPath = parser.value(QStringLiteral("screenshot"));
    const QUrl validationDownloadUrl(parser.value(QStringLiteral("validation-download")));
    const bool networkValidation = !validationDownloadUrl.isEmpty();
    if (networkValidation
        && (!validationDownloadUrl.isValid()
            || validationDownloadUrl.scheme().toLower() != QStringLiteral("https")
            || validationDownloadUrl.host().isEmpty())) {
        qCritical("Browser validation downloads must use a valid HTTPS URL.");
        return 65;
    }

    BrowserController browser(initialInput, smokeTest, validationDownloadUrl);
    MokoFilePickerModel filePicker;
    bool qmlWarningsFound = false;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("mokoBrowser"), &browser);
    engine.rootContext()->setContextProperty(QStringLiteral("mokoBrowserHistory"),
                                             browser.historyModel());
    engine.rootContext()->setContextProperty(QStringLiteral("mokoBrowserDownloads"),
                                             browser.downloadModel());
    engine.rootContext()->setContextProperty(QStringLiteral("mokoFilePicker"), &filePicker);
    QObject::connect(&engine, &QQmlEngine::warnings, &app, [&](const QList<QQmlError> &warnings) {
        qmlWarningsFound = qmlWarningsFound || !warnings.isEmpty();
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("MokoBrowser"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return 1;

    writeMokoLiveEvent(QStringLiteral("MOKO_BROWSER_READY sandbox=enabled web_security=enabled uid=%1")
                           .arg(geteuid()));
    writeMokoLiveEvent(QStringLiteral("MOKO_APP_READY app_id=org.moko.Browser state=ready detail=webengine-loaded"));

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (networkValidation) {
        struct ValidationState {
            bool javaScriptPassed = false;
            bool downloadPassed = false;
            bool finishing = false;
        };
        const auto state = std::make_shared<ValidationState>();
        const QPointer<QQuickWindow> validationWindow(window);
        const auto finishIfReady = [state, validationWindow, screenshotPath,
                                    &app, &qmlWarningsFound]() {
            if (state->finishing || !state->javaScriptPassed || !state->downloadPassed)
                return;
            state->finishing = true;
            QTimer::singleShot(500, &app, [validationWindow, screenshotPath,
                                           &app, &qmlWarningsFound]() {
                const bool saved = screenshotPath.isEmpty()
                    || (validationWindow && QFileInfo(screenshotPath).dir().exists()
                        && validationWindow->grabWindow().save(screenshotPath));
                app.exit(qmlWarningsFound || !saved ? 2 : 0);
            });
        };
        QObject::connect(&browser, &BrowserController::javaScriptValidated, &app,
                         [state, finishIfReady](const QUrl &url, bool passed) {
            if (url.scheme().toLower() == QStringLiteral("https") && passed) {
                state->javaScriptPassed = true;
                finishIfReady();
            }
        });
        QObject::connect(browser.downloadModel(), &BrowserDownloadModel::downloadCompleted, &app,
                         [state, finishIfReady](const QString &path, qint64 bytes) {
            state->downloadPassed = bytes > 0 && QFileInfo::exists(path);
            finishIfReady();
        });
        QTimer::singleShot(120000, &app, [state, &app]() {
            if (!state->finishing)
                app.exit(3);
        });
    } else if (smokeTest || !screenshotPath.isEmpty()) {
        QTimer::singleShot(3500, &app, [&, window]() {
            const bool saved = screenshotPath.isEmpty()
                || (window && QFileInfo(screenshotPath).dir().exists()
                    && window->grabWindow().save(screenshotPath));
            app.exit(qmlWarningsFound || !saved ? 2 : 0);
        });
    }
    return app.exec();
}
