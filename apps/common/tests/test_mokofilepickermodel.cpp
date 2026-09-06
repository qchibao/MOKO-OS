#include "mokofilepickermodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

class MokoFilePickerModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesOpenSaveAndFolderSelections();
    void rejectsUnsafeSelections();
};

void MokoFilePickerModelTest::resolvesOpenSaveAndFolderSelections()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkdir(QStringLiteral("reports")));
    QFile source(root.filePath(QStringLiteral("page.html")));
    QVERIFY(source.open(QIODevice::WriteOnly));
    source.write("<p>MOKO</p>");
    source.close();

    MokoFilePickerModel model;
    QVERIFY(model.navigateTo(root.path()));
    int fileRow = -1;
    int folderRow = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QVariantMap entry = model.entry(row);
        if (entry.value(QStringLiteral("name")) == QStringLiteral("page.html"))
            fileRow = row;
        if (entry.value(QStringLiteral("name")) == QStringLiteral("reports"))
            folderRow = row;
    }
    QVERIFY(fileRow >= 0);
    QVERIFY(folderRow >= 0);
    QCOMPARE(model.resolveSelection(QStringLiteral("open"), fileRow, {}),
             QFileInfo(source).absoluteFilePath());
    QCOMPARE(model.resolveSelection(QStringLiteral("folder"), folderRow, {}),
             QFileInfo(root.filePath(QStringLiteral("reports"))).absoluteFilePath());
    QCOMPARE(model.resolveSelection(QStringLiteral("save"), -1,
                                    QStringLiteral("report.json")),
             root.filePath(QStringLiteral("report.json")));
}

void MokoFilePickerModelTest::rejectsUnsafeSelections()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    MokoFilePickerModel model;
    QVERIFY(model.navigateTo(root.path()));
    QVERIFY(model.resolveSelection(QStringLiteral("open"), -1, {}).isEmpty());
    QVERIFY(model.resolveSelection(QStringLiteral("save"), -1,
                                   QStringLiteral("../report.json")).isEmpty());
    QVERIFY(model.resolveSelection(QStringLiteral("unsupported"), -1, {}).isEmpty());
}

QTEST_GUILESS_MAIN(MokoFilePickerModelTest)

#include "test_mokofilepickermodel.moc"
