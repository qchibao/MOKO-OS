#include "filemodel.h"

#include <QDir>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class FileModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void navigatesAndTracksHistory();
    void performsConfirmedFileOperations();
    void interoperatesWithSystemFileClipboard();
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

void FileModelTest::interoperatesWithSystemFileClipboard()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(QStringLiteral("destination")));
    QFile source(root.filePath(QStringLiteral("shared.txt")));
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("clipboard\n"), 10);
    source.close();

    FileModel sourceModel(nullptr, root.path());
    int sourceRow = -1;
    for (int row = 0; row < sourceModel.rowCount(); ++row) {
        if (sourceModel.data(sourceModel.index(row), FileModel::NameRole).toString()
            == QStringLiteral("shared.txt")) {
            sourceRow = row;
        }
    }
    QVERIFY(sourceRow >= 0);
    QVERIFY(sourceModel.stageCopy(sourceRow));
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    QVERIFY(mimeData->hasUrls());
    QVERIFY(mimeData->hasFormat(QStringLiteral("x-special/gnome-copied-files")));

    FileModel destinationModel(nullptr, root.filePath(QStringLiteral("destination")));
    QVERIFY(destinationModel.canPaste());
    QCOMPARE(destinationModel.clipboardMode(), QStringLiteral("copy"));
    QVERIFY(destinationModel.paste());
    QVERIFY(QFileInfo::exists(root.filePath(QStringLiteral("destination/shared.txt"))));
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

QTEST_MAIN(FileModelTest)

#include "test_filemodel.moc"
