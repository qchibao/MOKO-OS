#include "filemodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class FileModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void navigatesAndTracksHistory();
    void performsConfirmedFileOperations();
    void rejectsUnsafeNamesAndDeleteTokens();
};

void FileModelTest::navigatesAndTracksHistory()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(QStringLiteral("child")));

    FileModel model(nullptr, root.path());
    QCOMPARE(model.currentPath(), QFileInfo(root.path()).canonicalFilePath());
    QVERIFY(!model.canGoBack());
    QVERIFY(model.navigateTo(QStringLiteral("child")));
    QVERIFY(model.canGoBack());
    QVERIFY(model.goBack());
    QVERIFY(model.canGoForward());
    QVERIFY(model.goForward());
    QCOMPARE(QFileInfo(model.currentPath()).fileName(), QStringLiteral("child"));
}

void FileModelTest::performsConfirmedFileOperations()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QFile source(root.filePath(QStringLiteral("note.txt")));
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("moko\n"), 5);
    source.close();

    FileModel model(nullptr, root.path());
    QVERIFY(model.createFolder(QStringLiteral("Archive")));

    int sourceRow = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.data(model.index(row), FileModel::NameRole).toString() == QStringLiteral("note.txt"))
            sourceRow = row;
    }
    QVERIFY(sourceRow >= 0);
    QVERIFY(model.renameEntry(sourceRow, QStringLiteral("renamed.txt")));

    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.data(model.index(row), FileModel::NameRole).toString() == QStringLiteral("renamed.txt"))
            sourceRow = row;
    }
    QVERIFY(model.stageCopy(sourceRow));
    QVERIFY(model.navigateTo(QStringLiteral("Archive")));
    QVERIFY(model.paste());
    QVERIFY(QFileInfo::exists(QDir(model.currentPath()).filePath(QStringLiteral("renamed.txt"))));

    int copiedRow = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.data(model.index(row), FileModel::NameRole).toString() == QStringLiteral("renamed.txt"))
            copiedRow = row;
    }
    QVERIFY(copiedRow >= 0);
    QSignalSpy confirmationSpy(&model, &FileModel::deleteConfirmationRequested);
    const QString token = model.requestDelete(copiedRow);
    QVERIFY(!token.isEmpty());
    QCOMPARE(confirmationSpy.count(), 1);
    QVERIFY(model.confirmDelete(token));
    QVERIFY(!QFileInfo::exists(QDir(model.currentPath()).filePath(QStringLiteral("renamed.txt"))));
}

void FileModelTest::rejectsUnsafeNamesAndDeleteTokens()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QFile source(root.filePath(QStringLiteral("keep.txt")));
    QVERIFY(source.open(QIODevice::WriteOnly));
    source.close();

    FileModel model(nullptr, root.path());
    QVERIFY(!model.createFolder(QStringLiteral("../escape")));
    const QString token = model.requestDelete(0);
    QVERIFY(!token.isEmpty());
    QVERIFY(!model.confirmDelete(QStringLiteral("wrong-token")));
    QVERIFY(QFileInfo::exists(root.filePath(QStringLiteral("keep.txt"))));
    model.cancelDelete();
    QVERIFY(!model.confirmDelete(token));
    QVERIFY(QFileInfo::exists(root.filePath(QStringLiteral("keep.txt"))));
}

QTEST_GUILESS_MAIN(FileModelTest)

#include "test_filemodel.moc"
