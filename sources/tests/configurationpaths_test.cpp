#include "configurationpaths.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

class ConfigurationPathsTest : public QObject {
  Q_OBJECT

#ifdef Q_OS_MACOS
  std::unique_ptr<QTemporaryDir> m_settingsDirectory;
#endif

private slots:
  void init() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QString());
#ifdef Q_OS_MACOS
    // Give each case its own application data directory, cleaned up by RAII.
    const QString parent =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QVERIFY(QDir().mkpath(parent));
    m_settingsDirectory.reset(
        new QTemporaryDir(parent + "/PPM-path-tests-XXXXXX"));
    QVERIFY(m_settingsDirectory->isValid());
    QCoreApplication::setApplicationName(
        QFileInfo(m_settingsDirectory->path()).fileName());
#else
    QCoreApplication::setApplicationName("PPM");
#endif
  }

  void configurationDirectory() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString bundle = root.filePath("PPM 日本語.app/Contents/MacOS");
    QVERIFY(QDir().mkpath(bundle));
#ifdef Q_OS_MACOS
    QCOMPARE(ConfigurationPaths::configurationDirectory(bundle),
             m_settingsDirectory->path());
#else
    QCOMPARE(ConfigurationPaths::configurationDirectory(bundle), bundle);
#endif
    const QString plain = root.filePath("bin");
    QVERIFY(QDir().mkpath(plain));
    const QString nonBundle = root.filePath("plain/Contents/MacOS");
    QVERIFY(QDir().mkpath(nonBundle));
#ifdef Q_OS_MACOS
    QCOMPARE(ConfigurationPaths::configurationDirectory(plain),
             m_settingsDirectory->path());
    QCOMPARE(ConfigurationPaths::configurationDirectory(nonBundle),
             m_settingsDirectory->path());
#else
    QCOMPARE(ConfigurationPaths::configurationDirectory(plain), plain);
    QCOMPARE(ConfigurationPaths::configurationDirectory(nonBundle), nonBundle);
#endif
  }

  void stuffFolder_data() {
    QTest::addColumn<bool>("bundle");
    QTest::addColumn<QString>("path");
    for (bool bundle : {false, true}) {
      const QByteArray prefix = bundle ? "bundle-" : "plain-";
      QTest::newRow(prefix + "missing") << bundle << QString();
      QTest::newRow(prefix + "empty") << bundle << QString("");
      QTest::newRow(prefix + "relative") << bundle << QString("../制作 data");
      QTest::newRow(prefix + "absolute") << bundle << QDir::tempPath();
    }
  }

  void stuffFolder() {
    QFETCH(bool, bundle);
    QFETCH(QString, path);
    QTemporaryDir root;
    QTemporaryDir unrelatedWorkingDirectory;
    QVERIFY(root.isValid());
    QVERIFY(unrelatedWorkingDirectory.isValid());
    const QString executableDir =
        root.filePath(bundle ? "PPM.app/Contents/MacOS" : "bin");
    QVERIFY(QDir().mkpath(executableDir));
    QString configDir = executableDir;
#ifdef Q_OS_MACOS
    configDir = m_settingsDirectory->path();
#endif
    if (!path.isNull()) {
      QSettings settings(configDir + "/conf.ini", QSettings::IniFormat);
      settings.setValue("StuffFolderPath", path);
      settings.sync();
      QCOMPARE(settings.status(), QSettings::NoError);
    }
    const QString oldWorkingDirectory = QDir::currentPath();
    QVERIFY(QDir::setCurrent(unrelatedWorkingDirectory.path()));
    const QString actual = ConfigurationPaths::stuffFolderPath(executableDir);
    QVERIFY(QDir::setCurrent(oldWorkingDirectory));
    const QString expected =
        path.isEmpty() ? QString() : QDir(configDir).absoluteFilePath(path);
    QCOMPARE(actual, QDir::cleanPath(expected));
  }

  void userSettingsLocation() {
#ifdef Q_OS_MACOS
    QCOMPARE(ConfigurationPaths::userSettingsFilePath(),
             m_settingsDirectory->filePath("usersettings.ini"));
    QCOMPARE(
        QFileInfo(ConfigurationPaths::userSettingsFilePath()).absolutePath(),
        ConfigurationPaths::configurationDirectory(
            "/Applications/PPM.app/Contents/MacOS"));
    QCoreApplication::setApplicationName("PPM");
    QCOMPARE(
        ConfigurationPaths::configurationDirectory(QString()),
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
            "/PPM");
#else
    QCOMPARE(ConfigurationPaths::userSettingsFilePath(),
             QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
                 "/usersettings.ini");
#endif
  }
};

QTEST_GUILESS_MAIN(ConfigurationPathsTest)
#include "configurationpaths_test.moc"
