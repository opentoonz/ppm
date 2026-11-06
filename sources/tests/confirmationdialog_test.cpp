#include "actionconfirmationdialog.h"

#include <QComboBox>
#include <QFile>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class ConfirmationDialogTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName("PPM-confirmation-tests");
  }

  void approveAndRewind_data() {
    QTest::addColumn<QString>("destination");
    QTest::addColumn<WorkflowType>("destinationType");
    QTest::addColumn<QString>("expectedMarker");
    QTest::newRow("director-to-layout") << QString("Layout") << WorkflowType(0)
                                        << QString("0001_r_layout.clip.txt");
    QTest::newRow("director-to-director")
        << QString("Director") << WorkflowType(1)
        << QString("0001_r_director.clip.txt");
  }

  void approveAndRewind() {
    QFETCH(QString, destination);
    QFETCH(WorkflowType, destinationType);
    QFETCH(QString, expectedMarker);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString scene = temp.filePath("scene");
    QVERIFY(QDir().mkpath(scene));
    const QString layout   = scene + "/0001_layout.clip";
    const QString director = scene + "/0001_director.clip";
    QFile drawing(layout);
    QVERIFY(drawing.open(QIODevice::WriteOnly));
    QCOMPARE(drawing.write("drawing"), qint64(7));
    drawing.close();

    // The matching protocol is not the first one in either workflow.
    Workflow first{0, 0, "Layout", Qt::white};
    Workflow next{1, 0, "Director", Qt::white};
    first.protocols = {{Paper, {}, {}},
                       {Digital,
                        scene + "/0001[R]_layout.clip",
                        {{"Approve", {{Step_Rename, {director}}}, {1}}}}};
    next.protocols  = {{Paper, {}, {}},
                       {Digital, scene + "/0001[R]_director.clip", {}}};
    auto* manager   = WorkflowManager::instance();
    manager->setWorkflows({first, next});
    manager->setProjectRootPath(temp.path());
    manager->setInfoFolderPath(temp.filePath("info"));
    manager->setLogRootPath(temp.filePath("logs"));
    FlowGroupInfo group;
    group.idName            = "LO";
    group.takeNotationStyle = FlowGroupInfo::TakeNotation_Rewind;
    manager->setFlowGroupInfos({{0, group}});

    for (bool cancel : {true, false}) {
      ActionConfirmationDialog dialog({0, 1, {layout}, "00", "0001"}, 0, false);
      press(dialog, cancel ? "Cancel" : "OK");
      QCOMPARE(dialog.isCanceled(), cancel);
      QCOMPARE(QFileInfo::exists(layout), cancel);
      QCOMPARE(QFileInfo::exists(director), !cancel);
    }

    RewindDestinationDialog picker(1, 1);
    auto* choices = picker.findChild<QComboBox*>();
    QVERIFY(choices);
    QCOMPARE(choices->count(), 2);
    int index = choices->findText(destination);
    QVERIFY(index >= 0);
    choices->setCurrentIndex(index);
    press(picker, "Select");
    QCOMPARE(picker.destinationWorkflow(), qMakePair(destinationType, 1));

    for (bool cancel : {true, false}) {
      RewindConfirmationDialog dialog({1, 1, {director}, "00", "0001"},
                                      picker.destinationWorkflow(), false);
      press(dialog, cancel ? "Cancel" : "OK");
      QCOMPARE(dialog.isCanceled(), cancel);
      QCOMPARE(QFileInfo::exists(director), cancel);
      QCOMPARE(QFileInfo::exists(scene + "/" + expectedMarker), !cancel);
    }
    QFile archived(scene + "/old/0001_director.clip");
    QVERIFY(archived.open(QIODevice::ReadOnly));
    QCOMPARE(archived.readAll(), QByteArray("drawing"));
    QCOMPARE(manager->currentRewindSuffix("0001"), QString("_r"));
  }

private:
  static void press(QDialog& dialog, const QString& label) {
    for (auto* button : dialog.findChildren<QPushButton*>()) {
      if (button->text() == label) {
        button->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        return;
      }
    }
    QFAIL("Missing confirmation button");
  }
};

QTEST_MAIN(ConfirmationDialogTest)
#include "confirmationdialog_test.moc"
