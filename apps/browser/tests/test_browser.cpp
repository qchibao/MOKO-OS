#include "browserdownloadmodel.h"
#include "browserhistorymodel.h"
#include "browserurl.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

class BrowserTest final : public QObject
{
    Q_OBJECT

private slots:
    void normalizesAddressesAndSearches();
    void persistsSafeHistory();
    void sanitizesDownloadPaths();
};

void BrowserTest::normalizesAddressesAndSearches()
{
    QCOMPARE(normalizedBrowserUrl(QStringLiteral("https://example.com/path")),
             QUrl(QStringLiteral("https://example.com/path")));
    QCOMPARE(normalizedBrowserUrl(QStringLiteral("example.com")),
             QUrl(QStringLiteral("https://example.com")));
    QCOMPARE(normalizedBrowserUrl(QStringLiteral("localhost:8080")),
             QUrl(QStringLiteral("https://localhost:8080")));

    const QUrl search = normalizedBrowserUrl(QStringLiteral("moko browser tabs"));
    QCOMPARE(search.scheme(), QStringLiteral("https"));
    QCOMPARE(search.host(), QStringLiteral("duckduckgo.com"));
    QCOMPARE(QUrlQuery(search).queryItemValue(QStringLiteral("q")),
             QStringLiteral("moko browser tabs"));

    const QUrl unsafe = normalizedBrowserUrl(QStringLiteral("javascript:alert(1)"));
    QCOMPARE(unsafe.scheme(), QStringLiteral("https"));
    QCOMPARE(unsafe.host(), QStringLiteral("duckduckgo.com"));
}

void BrowserTest::persistsSafeHistory()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = temporary.filePath(QStringLiteral("history.json"));

    {
        BrowserHistoryModel history(nullptr, path);
        QSignalSpy countChanged(&history, &BrowserHistoryModel::countChanged);
        history.addVisit(QStringLiteral("Example"),
                         QUrl(QStringLiteral("https://user:secret@example.com/first")));
        history.addVisit(QStringLiteral("Example updated"),
                         QUrl(QStringLiteral("https://example.com/first")));
        history.addVisit(QStringLiteral("Ignored"), QUrl(QStringLiteral("file:///etc/passwd")));
        QCOMPARE(history.rowCount(), 1);
        QVERIFY(countChanged.count() >= 1);
        const QVariantMap entry = history.entry(0);
        QCOMPARE(entry.value(QStringLiteral("title")).toString(), QStringLiteral("Example updated"));
        QCOMPARE(entry.value(QStringLiteral("url")).toUrl().userInfo(), QString());
    }

    BrowserHistoryModel restored(nullptr, path);
    QCOMPARE(restored.rowCount(), 1);
    QCOMPARE(restored.entry(0).value(QStringLiteral("host")).toString(),
             QStringLiteral("example.com"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(!file.readAll().contains("secret"));
}

void BrowserTest::sanitizesDownloadPaths()
{
    QCOMPARE(BrowserDownloadModel::safeFileName(QStringLiteral("../../report.pdf")),
             QStringLiteral("report.pdf"));
    QCOMPARE(BrowserDownloadModel::safeFileName(QStringLiteral("..")),
             QStringLiteral("download"));
    QCOMPARE(BrowserDownloadModel::safeFileName(QStringLiteral("bad\nname.txt")),
             QStringLiteral("bad_name.txt"));

    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString downloads = temporary.filePath(QStringLiteral("Downloads"));
    QVERIFY(BrowserDownloadModel::isPathInside(
        downloads + QStringLiteral("/archive.zip"), downloads));
    QVERIFY(!BrowserDownloadModel::isPathInside(
        temporary.filePath(QStringLiteral("Downloads-copy/archive.zip")), downloads));
    QVERIFY(!BrowserDownloadModel::isPathInside(QStringLiteral("/etc/passwd"), downloads));
}

QTEST_GUILESS_MAIN(BrowserTest)

#include "test_browser.moc"
