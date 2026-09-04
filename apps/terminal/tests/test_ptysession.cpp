#include "ptysession.h"

#include <QSignalSpy>
#include <QTest>

class PtySessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void runsInteractiveShellThroughPty();
    void rejectsMissingExecutable();
};

void PtySessionTest::runsInteractiveShellThroughPty()
{
    PtySession session;
    QByteArray transcript;
    connect(&session, &PtySession::bytesReceived, this,
            [&](const QByteArray &bytes) { transcript.append(bytes); });
    QSignalSpy startedSpy(&session, &PtySession::started);
    QVERIFY(session.start(QStringLiteral("/bin/sh")));
    QTRY_COMPARE_WITH_TIMEOUT(startedSpy.count(), 1, 2000);
    session.resize(100, 32);
    QVERIFY(session.writeBytes(QByteArrayLiteral("printf 'MOKO_PTY_OK\\n'\n")));
    QTRY_VERIFY_WITH_TIMEOUT(transcript.contains("MOKO_PTY_OK"), 5000);
    QVERIFY(session.running());
    QSignalSpy exitedSpy(&session, &PtySession::exited);
    QVERIFY(session.writeBytes(QByteArrayLiteral("exit 7\n")));
    QTRY_VERIFY_WITH_TIMEOUT(!session.running(), 5000);
    QCOMPARE(exitedSpy.count(), 1);
    QCOMPARE(exitedSpy.constFirst().constFirst().toInt(), 7);
}

void PtySessionTest::rejectsMissingExecutable()
{
    PtySession session;
    QSignalSpy errorSpy(&session, &PtySession::errorOccurred);
    QVERIFY(!session.start(QStringLiteral("/definitely/not/a/shell")));
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(!session.running());
}

QTEST_GUILESS_MAIN(PtySessionTest)

#include "test_ptysession.moc"
