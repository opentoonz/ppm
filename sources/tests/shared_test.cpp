#include "actionconfirmationdialog.h"
#include "configurationpaths.h"
#include "mywindow.h"
#include "projectsettings_common.h"
#include "projectsettingsdialog.h"
#include "sharedfiles.h"
#include "usersettings.h"

#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QInputDialog>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

class SharedTest : public QObject {
  Q_OBJECT
  // An explicit test root allows the same actions to run on a mounted share.
  QTemporaryDir root{qEnvironmentVariable("PPM_TEST_VOLUME", QDir::tempPath()) +
                     "/ppm-shared-XXXXXX"};
  QTemporaryDir settings;
  QString confPath;
  bool ownsConfig   = false;
  bool ownsUserFile = false;
  QString stored    = QStringLiteral(R"(\\Studio\Anime)");
  PathMappings defaults;

  static bool write(const QString& path, const QByteArray& data) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
  }

private slots:
  void initTestCase() {
    QVERIFY(root.isValid());
    QVERIFY(settings.isValid());
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QFileInfo(settings.path()).fileName());
    QString config = ConfigurationPaths::configurationDirectory(
        QCoreApplication::applicationDirPath());
    QVERIFY(QDir().mkpath(config));
    confPath = config + "/conf.ini";
    QVERIFY2(!QFileInfo::exists(confPath),
             "Expected isolated test executable/configuration directory");
    ownsConfig = true;
    QVERIFY(!QFileInfo::exists(ConfigurationPaths::userSettingsFilePath()));
    ownsUserFile = true;
    QSettings(confPath, QSettings::IniFormat)
        .setValue("StuffFolderPath", root.path());
    QVERIFY(QDir().mkpath(root.filePath("projects")));
    defaults.rows = {{stored, root.path()},
                     {root.path(), root.filePath("double-mapped")}};
#ifdef Q_OS_WIN
    QString name = "defaultusersettings_windows.ini";
#else
    QString name = "defaultusersettings_macos.ini";
#endif
    QSettings ini(root.filePath(name), QSettings::IniFormat);
    defaults.write(ini);
    ini.sync();
    QCOMPARE(UserSettings::instance()->pathMappings().toLocal(stored),
             root.path());
  }

  void cleanupTestCase() {
    if (ownsConfig) QFile::remove(confPath);
    if (ownsUserFile) QFile::remove(ConfigurationPaths::userSettingsFilePath());
  }

  void matchPrefixes() {
    PathMappings m;
    QCOMPARE(m.toLocal("Z:/anime"), "Z:/anime");
    m.rows = {{"Z:/anime", "/Volumes/Anime"},
              {"z:/anime/cuts", "/Volumes/Anime/cuts"},
              {"//NAS/anime", "/Volumes/Anime"}};
    QCOMPARE(m.toLocal(R"(z:\ANIME\CUTS\A.clip)"),
             "/Volumes/Anime/cuts/A.clip");
    QCOMPARE(m.toLocal("Z:/anime2/cuts"), "Z:/anime2/cuts");
    QCOMPARE(m.toLocal("[ROOT]/cuts"), "[ROOT]/cuts");
    QCOMPARE(m.toLocal("relative/cuts"), "relative/cuts");
    QCOMPARE(m.storedCandidates("/Volumes/Anime/cuts/A.clip").size(), 3);
    m.rows.append(PathMapping{"//NAS/anime/other", "/Volumes/B"});
    QVERIFY(!m.storedCandidates("/Volumes/Anime/other/file")
                 .contains("//NAS/anime/other/file"));
    QCOMPARE(m.toLocal(R"(\\nas\ANIME\01)"), "/Volumes/Anime/01");
  }

  void overrideDefaults() {
    auto* user = UserSettings::instance();
    QVERIFY(user->inheritsMappings());
    user->setPathMappings({}, false);
    QVERIFY(!user->inheritsMappings());
    QSettings ini(ConfigurationPaths::userSettingsFilePath(),
                  QSettings::IniFormat);
    QVERIFY(ini.contains("PathMappings/size"));
    QCOMPARE(PathMappings::read(ini).rows.size(), 0);
    user->setPathMappings(defaults, false);
    QCOMPARE(PathMappings::read(ini).toLocal(stored), root.path());
    user->setPathMappings({}, true);
    QVERIFY(!ini.contains("PathMappings/size"));
    QCOMPARE(user->configuredMappings().toLocal(stored), root.path());
  }

  void projectEditorAndScan() {
    auto* wm = WorkflowManager::instance();
    wm->setProjectRootPath(stored);
    wm->setInfoFolderPath(stored + "/info");
    wm->setLogRootPath(stored + "/logs");
    wm->setSceneThumbnailPath("[ROOT]/[CUTNUMBER].png");
    wm->setUseSequence(false);
    wm->setProjectTitle("Title");
    wm->setPathMacros({{"[HIDDEN]", stored + "/private", {}, true}});
    FlowGroupInfo group;
    group.idName = "Layout";
    wm->setFlowGroupInfos({{0, group}});
    Workflow wf{0, 0, "Layout", Qt::white};
    wf.protocols = {{Digital, "[ROOT]/[CUTNUMBER].clip", {}}};
    wm->setWorkflows({wf});
    QVERIFY(wm->saveProject("shared"));
    QVERIFY(wm->loadProject("shared"));
    refreshTmpSettingsFromProject();
    BasicsPage basics;
    basics.refresh();
    auto folders   = basics.findChildren<FolderPathField*>();
    auto rootField = std::find_if(
        folders.begin(), folders.end(),
        [&](FolderPathField* f) { return f->text() == root.path(); });
    QVERIFY(rootField != folders.end());
    QVERIFY((*rootField)->toolTip().contains(stored));
    auto edits = basics.findChildren<QLineEdit*>();
    auto title = std::find_if(edits.begin(), edits.end(), [](QLineEdit* edit) {
      return edit->text() == "Title";
    });
    QVERIFY(title != edits.end());
    (*title)->setText("Edited title");
    QVERIFY(QMetaObject::invokeMethod(&basics, "onFieldEdited"));
    QVERIFY(saveTmpSettingsToProject(nullptr, "shared"));
    QVERIFY(wm->loadProject("shared"));
    QCOMPARE(wm->projectTitle(), "Edited title");
    QCOMPARE(wm->projectRootPathSetting(), stored);
    QCOMPARE(wm->logRootPathSetting(), stored + "/logs");
    QCOMPARE(wm->getWorkflow(0).protocols.first().conditionPath,
             "[ROOT]/[CUTNUMBER].clip");
    QCOMPARE(wm->decodePath("[ROOT]/drawing", {}, {}),
             root.filePath("drawing"));
    QCOMPARE(wm->decodeObfuscatedPathMacros("[HIDDEN]/x", {}, {}),
             root.filePath("private/x"));
    QCOMPARE(wm->hideObfuscatedPaths(root.filePath("private/x"), {}, {}),
             "[HIDDEN]/x");
    QVERIFY(write(root.filePath("0001.clip"), "drawing"));
    wm->registerCutNames({"0001"}, 0);
    MyTable table;
    table.simpleSearch(0);
    QVERIFY(wm->isInProgress("0001", 0));
    const QString secret = root.filePath("private/cue.clip");
    QVERIFY(QDir().mkpath(root.filePath("private")));
    QVERIFY(write(secret, "hidden drawing"));
    QString actionLog;
    QVariant record;
    StepExecutor_Copy copier({Step_Copy, {stored + "/public.clip"}}, secret, {},
                             "0001", false);
    QVERIFY(copier.execute(actionLog, record));
    QVERIFY(!actionLog.contains(root.filePath("private")));
    QVERIFY(actionLog.contains("[HIDDEN]"));
    // This local path is also the stored prefix of a second rule. The
    // editor must offer both interpretations, including cancel.
    QString input = root.filePath("ambiguous");
    (*rootField)->setText(input);
    QTimer::singleShot(0, [] {
      auto* chooser =
          qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
      QVERIFY(chooser);
      chooser->reject();
    });
    QVERIFY(QMetaObject::invokeMethod(&basics, "onFieldEdited"));
    QCOMPARE(tmpSettings.projectRootPath, stored);
    (*rootField)->setText(input);
    QTimer::singleShot(0, [&] {
      auto* chooser =
          qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
      QVERIFY(chooser);
      const QString chosen = "//Studio/Anime/ambiguous";
      QVERIFY(chooser->comboBoxItems().contains(chosen));
      chooser->setTextValue(chosen);
      chooser->accept();
    });
    QVERIFY(QMetaObject::invokeMethod(&basics, "onFieldEdited"));
    QCOMPARE(tmpSettings.projectRootPath, "//Studio/Anime/ambiguous");
    (*rootField)->setText("//Other/Unmapped");
    QTimer::singleShot(0, [] {
      auto* warning =
          qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      QVERIFY(warning);
      warning->button(QMessageBox::Save)->click();
    });
    QVERIFY(QMetaObject::invokeMethod(&basics, "onFieldEdited"));
    QCOMPARE(tmpSettings.projectRootPath, "//Other/Unmapped");
    QVERIFY((*rootField)->toolTip().contains("//Other/Unmapped"));
    // A hand-entered stored path must round-trip through the local display.
    (*rootField)->setText("//Studio/Anime/New");
    QVERIFY(QMetaObject::invokeMethod(&basics, "onFieldEdited"));
    QCOMPARE((*rootField)->text(), root.filePath("New"));
    QVERIFY(saveTmpSettingsToProject(nullptr, "shared"));
    QVERIFY(wm->loadProject("shared"));
    QCOMPARE(wm->projectRootPathSetting(), "//Studio/Anime/New");
    QCOMPARE(wm->infoFolderPathSetting(), stored + "/info");
    QCOMPARE(wm->sceneThumbnailPath(), "[ROOT]/[CUTNUMBER].png");
  }

  void rewindOnMappedShare() {
    auto* wm = WorkflowManager::instance();
    wm->setProjectRootPath(stored);
    wm->setInfoFolderPath(stored + "/info");
    wm->setLogRootPath(stored + "/logs");
    FlowGroupInfo group;
    group.idName            = "Layout";
    group.takeNotationStyle = FlowGroupInfo::TakeNotation_Rewind;
    wm->setFlowGroupInfos({{0, group}});
    Workflow wf{0, 0, "Layout", Qt::white};
    wf.protocols = {{Digital, "[ROOT]/rewind/[CUTNUMBER][R].clip", {}}};
    wm->setWorkflows({wf});
    QString cue = root.filePath("rewind/0002.clip");
    QVERIFY(QDir().mkpath(root.filePath("rewind")));
    QVERIFY(write(cue, "drawing"));
    RewindConfirmationDialog dialog({0, 0, {cue}, {}, "0002"}, {0, 0}, false);
    for (auto* button : dialog.findChildren<QPushButton*>())
      if (button->text() == "OK") {
        button->click();
        break;
      }
    QVERIFY(!dialog.isCanceled());
    QVERIFY(QFileInfo::exists(root.filePath("rewind/old/0002.clip")));
    QVERIFY(QFileInfo::exists(root.filePath("rewind/0002_r.clip.txt")));
    QVERIFY(!QFileInfo::exists(root.filePath("double-mapped")));
  }

  void recursiveCopyAndUndo() {
    const QString source = root.filePath("Source");
    const QString dest   = root.filePath("Destination");
    QVERIFY(QDir().mkpath(source + "/empty"));
    const QString kana =
        QStringLiteral("ガ").normalized(QString::NormalizationForm_D);
    QVERIFY(write(source + "/" + kana, "drawing"));
    QVERIFY(write(source + "/.notes", "user file"));
    QVERIFY(write(source + "/empty/.DS_Store", "metadata"));
    QVERIFY(write(source + "/._drawing", "metadata"));
    const ActionStep step{Step_Copy, {stored + "/Destination"}};
    StepExecutor_Copy copy(step, source, {}, {}, false);
    QString log;
    QVariant record;
    QVERIFY2(copy.execute(log, record), qPrintable(log));
    const QString actual = SharedFiles::actualPath(dest + "/ガ");
    QVERIFY(QFileInfo(actual).isFile());
    QVERIFY(QFileInfo(dest + "/.notes").isFile());
    QVERIFY(!QFileInfo::exists(dest + "/empty/.DS_Store"));
    QVERIFY(!QFileInfo::exists(dest + "/._drawing"));
    QVERIFY(write(dest + "/empty/.DS_Store", "metadata"));
    QVERIFY(write(dest + "/keep.clip", "later"));
    StepExecutor_Copy undo(step, source, {}, {}, true);
    undo.setCheckSum(record);
    QVERIFY2(undo.execute(log, record), qPrintable(log));
    QVERIFY(QFileInfo(dest + "/keep.clip").isFile());
    QVERIFY(!QFileInfo::exists(dest + "/empty"));
    QVERIFY(!QFileInfo::exists(actual));
    QVERIFY(QDir().mkpath(root.filePath("metadata-only")));
    QVERIFY(write(root.filePath("metadata-only/._drawing"), "metadata"));
    QCOMPARE(SharedFiles::removeEmptyFolder(root.filePath("metadata-only")),
             SharedFiles::Cleanup::Removed);
    QVERIFY(QFileInfo::exists(source + "/" + kana));
    QVERIFY(SharedFiles::insideSource(source, source + "/missing/child"));
    const QString alias = root.filePath("sOuRcE");
    QCOMPARE(SharedFiles::insideSource(source, alias + "/new"),
             QFileInfo(alias).exists());
  }

  void unavailableForeignRoot() {
    auto* wm = WorkflowManager::instance();
#ifdef Q_OS_WIN
    const QString foreign = "/Volumes/Unmapped";
#else
    const QString foreign = "Z:/Unmapped";
#endif
    wm->setProjectRootPath(foreign);
    wm->setLogRootPath(foreign);
    wm->setInfoFolderPath(foreign);
    QCOMPARE(wm->projectRootPathSetting(), foreign);
    QVERIFY(wm->projectRootPath().isEmpty());
    QVERIFY(wm->decodePath("[ROOT]/file", {}, {}).isEmpty());
    QVERIFY(wm->googleCueFilePath("log").isEmpty());
    QString settingsFile, lock;
    wm->getProgressSettingsFilePath(0, settingsFile, lock);
    QVERIFY(settingsFile.isEmpty());
    QVERIFY(lock.isEmpty());
  }

  void equivalentSpellings() {
    const QString composed   = root.filePath("ペ.clip");
    const QString decomposed = root.filePath(
        QStringLiteral("ペ.clip").normalized(QString::NormalizationForm_D));
    QVERIFY(write(decomposed, "a"));
    QVERIFY(
        SharedFiles::sameEntry(SharedFiles::actualPath(composed), decomposed));
    if (!QFileInfo::exists(composed)) {
      QVERIFY(write(composed, "b"));
      QVERIFY(SharedFiles::actualPath(composed).isEmpty());
      QVERIFY(SharedFiles::actualPath(decomposed).isEmpty());
      StepExecutor_Copy copy({Step_Copy, {decomposed}}, composed, {}, {},
                             false);
      QString log;
      QVariant checksum;
      QVERIFY(!copy.execute(log, checksum));
      QVERIFY(log.contains("Ambiguous"));
      QVERIFY(QFileInfo::exists(composed));
      QVERIFY(QFileInfo::exists(decomposed));
    }
  }
};

QTEST_MAIN(SharedTest)
#include "shared_test.moc"
