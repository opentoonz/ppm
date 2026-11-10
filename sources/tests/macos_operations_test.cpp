#include "platform.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

namespace {
bool writeFile(const QString& path, const QByteArray& contents) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly) &&
         file.write(contents) == contents.size();
}

QByteArray readFile(const QString& path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}  // namespace

class MacOperationsTest : public QObject {
  Q_OBJECT
private slots:
  void openDocumentInApplication() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString bundle = temp.filePath("Document Receiver.app");
    const QString file   = temp.filePath("原画 ' [01].clip");
    QVERIFY(writeFile(file, "document"));
    // A native document-open handler records the path that actually arrived.
    const QString handler = R"(on open receivedFiles
      set marker to (POSIX path of item 1 of receivedFiles) & ".opened"
      set output to open for access (POSIX file marker) with write permission
      write "opened" to output
      close access output
    end open)";
    QCOMPARE(
        QProcess::execute("/usr/bin/osacompile", {"-o", bundle, "-e", handler}),
        0);
    // The native file picker returns the bundle path with a trailing slash.
    QVERIFY(Platform::openFile(bundle + "/", file));
    QTRY_COMPARE_WITH_TIMEOUT(readFile(file + ".opened"), QByteArray("opened"),
                              10000);
    const QString invalid = temp.filePath("Missing Executable.app");
    QVERIFY(QDir().mkdir(invalid));
    QVERIFY(!Platform::openFile(invalid, file));
  }

  void openSceneWithArguments_data() {
    QTest::addColumn<bool>("bundle");
    QTest::addColumn<QStringList>("options");
    QTest::newRow("executable") << false << QStringList{"-layout", "two words"};
    QTest::newRow("bundle") << true << QStringList{"-layout", "two words"};
    QTest::newRow("bundle-without-options") << true << QStringList{};
  }

  void openSceneWithArguments() {
    QFETCH(bool, bundle);
    QFETCH(QStringList, options);
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QString program          = temp.filePath("Test App.app");
    const QString executable = program + "/Contents/MacOS/capture";
    QVERIFY(QDir().mkpath(QFileInfo(executable).absolutePath()));
    QVERIFY(writeFile(program + "/Contents/Info.plist",
                      "<plist version=\"1.0\"><dict>"
                      "<key>CFBundlePackageType</key><string>APPL</string>"
                      "<key>CFBundleIdentifier</"
                      "key><string>org.opentoonz.ppm.capture</string>"
                      "<key>CFBundleExecutable</key><string>capture</string></"
                      "dict></plist>"));
    QVERIFY(writeFile(executable,
                      "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$0.args\"\n"));
    QVERIFY(QFile::setPermissions(
        executable, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    if (!bundle) program = executable;
    const QString file = temp.filePath("原画 ' [01].tnz");
    QVERIFY(writeFile(file, "scene"));
    QVERIFY(Platform::openFile(program, file, Platform::OpenMode::CommandLine,
                               options));
    const QByteArray expected =
        (options + QStringList{file}).join('\n').toUtf8() + '\n';
    QTRY_COMPARE_WITH_TIMEOUT(readFile(executable + ".args"), expected, 10000);
  }

  void zipFolderContents() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString folder = temp.filePath("原画 [01]");
    const QString entry  = "/nested/カット ' [01].txt";
    QVERIFY(QDir().mkpath(folder + "/nested"));
    QVERIFY(writeFile(folder + entry, "drawing"));
    QVERIFY(writeFile(folder + "/.notes", "notes"));
    const QString zip = temp.filePath("原画 [01].zip");
    QVERIFY(Platform::zipFolder(folder, zip));
    // zipfile honors the ZIP filename encoding flag. Apple's extractor also
    // accepts unmarked UTF-8 and would hide a broken Japanese entry name.
    QProcess reader;
    reader.start("python3", {"-c",
                             "import sys, zipfile\n"
                             "with zipfile.ZipFile(sys.argv[1]) as z:\n"
                             "    sys.stdout.buffer.write(z.read(sys.argv[2]))",
                             zip, entry.mid(1)});
    QVERIFY(reader.waitForFinished());
    QCOMPARE(reader.exitCode(), 0);
    QCOMPARE(reader.readAllStandardOutput(), QByteArray("drawing"));

    QVERIFY(QFile::remove(folder + entry));
    QVERIFY(Platform::zipFolder(folder, zip));
    const QString replacement = temp.filePath("replacement");
    QCOMPARE(
        QProcess::execute("/usr/bin/ditto", {"-x", "-k", zip, replacement}), 0);
    QVERIFY(!QFileInfo::exists(replacement + entry));
    QVERIFY(!QFileInfo::exists(replacement + "/.notes"));

    const QByteArray previous = readFile(zip);
    QVERIFY(!Platform::zipFolder(temp.filePath("missing"), zip));
    QVERIFY(writeFile(folder + "/unreadable", "data"));
    QVERIFY(QFile::setPermissions(folder + "/unreadable", {}));
    if (!QFileInfo(folder + "/unreadable").isReadable())
      QVERIFY(!Platform::zipFolder(folder, zip));
    QCOMPARE(readFile(zip), previous);
  }

  void zipInsideSource() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString folder = temp.filePath("Source");
    QVERIFY(QDir().mkpath(folder + "/nested/.private"));
    QVERIFY(writeFile(folder + "/nested/.private/secret", "hidden"));
    QVERIFY(writeFile(folder + "/.DS_Store", "metadata"));
    QVERIFY(writeFile(folder + "/nested/.notes", "metadata"));
    QVERIFY(writeFile(folder + "/nested/shot1.zip", "keep"));
    QVERIFY(writeFile(folder + "/日本語.txt", "drawing"));
    const QString alias = temp.filePath("alias");
    QVERIFY(QFile::link(folder + "/nested", alias));
    const QString zip = alias + "/shot[1].zip";
    for (int pass = 0; pass < 2; ++pass) {
      QVERIFY(Platform::zipFolder(folder, zip));
      QProcess reader;
      reader.start(
          "python3",
          {"-c",
           "import sys, zipfile\n"
           "with zipfile.ZipFile(sys.argv[1]) as z:\n"
           "    assert sorted(z.namelist()) == "
           "['nested/', 'nested/shot1.zip', '日本語.txt'], z.namelist()\n"
           "    assert z.testzip() is None",
           zip});
      QVERIFY(reader.waitForFinished());
      QVERIFY2(reader.exitCode() == 0,
               reader.readAllStandardError().constData());
    }
  }

  void rejectUnsafeZipDestination() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString folder = temp.filePath("Source");
    QVERIFY(QDir().mkdir(folder));
    const QString target = folder + "/unwritten.zip";
    const QString link   = temp.filePath("link.zip");
    QVERIFY(QFile::link(target, link));
    QVERIFY(!Platform::zipFolder(folder, link));
    QVERIFY(!QFileInfo::exists(target));
  }
};

QTEST_GUILESS_MAIN(MacOperationsTest)
#include "macos_operations_test.moc"
