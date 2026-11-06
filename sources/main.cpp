#include "mywindow.h"

#include "workflow.h"
#include "googlegateway.h"
#include "usersettings.h"
#include "renamescript.h"
#include <iostream>

// Qt includes
#include <QApplication>
#ifdef Q_OS_MACOS
#include <QStyleHints>
#endif
#include <QTranslator>
#include <QSettings>
// #include <QDir>
#include <QDateTime>
#include <QFileInfo>

//-----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
  qputenv("QT_QPA_PLATFORM", "windows:darkmode=0");
#endif

  QApplication a(argc, argv);
#ifdef Q_OS_MACOS
  a.styleHints()->setColorScheme(Qt::ColorScheme::Light);
#endif
  a.setApplicationName("PPM");
  // バージョン番号は CMakeLists.txt の project() で決める
  a.setApplicationVersion(PPM_VERSION);

  // UIの言語は、ユーザー設定で選ばれたもの。翻訳ファイルはStuffフォルダのlocフォルダにある
  // （言語が空、または翻訳ファイルが見つからない場合は、翻訳なし＝英語のUIで起動する）
  QTranslator tra;
  {
    UserSettings* userSettings = UserSettings::instance();
    const QString languageCode = userSettings->languageCode();
    if (!languageCode.isEmpty()) {
      const QString qmPath = userSettings->translationFilePath(languageCode);
      if (QFileInfo(qmPath).exists() && tra.load(qmPath)) {
        a.installTranslator(&tra);
        // リネームスクリプトの名前・説明に使う言語
        RenameScript::setLanguage(languageCode);
      }
    }
    // 翻訳ファイルのインストール後に確認して、警告を翻訳して表示する
    userSettings->warnAboutMissingBinaries();
  }

  MyWindow w;

  QByteArray geom = UserSettings::instance()->loadWindowGeometry();
  if (!geom.isEmpty()) w.restoreGeometry(geom);

  w.show();
  w.refreshList();

  int ret = a.exec();
  return ret;
}
