#pragma once
#ifndef USERSETTINGS_H
#define USERSETTINGS_H

#include "pathmapping.h"
#include <QString>
#include <QStringList>
#include <QList>
#include <QCoreApplication>

class UserSettings {  // singleton

  Q_DECLARE_TR_FUNCTIONS(UserSettings)

  // user profiles
  QString m_tvpBinaryPath;
  QString m_cspBinaryPath;
  QString m_xdtsViewerBinaryPath;
  QString m_openToonzBinaryPath;
  QString m_openToonzArgument;
  // [USER_NAME]マクロの代入値の上書き設定。空文字列なら既定値（OSのユーザ名）を使う
  QString m_userNameOverride;

  QString m_stuffFolderPath;
  PathMappings m_defaultMappings, m_mappings, m_activeMappings;
  bool m_inheritMappings = true;

  // UIの言語（翻訳ファイルPPM_[languageCode].qmの[languageCode]。例："ja"）。
  // 空文字列の場合は翻訳なし（英語）。言語を変更した場合、次回の起動時に反映される
  QString m_languageCode;

  UserSettings();

public:
  static UserSettings* instance();

  QString getTvpBinaryPath() { return m_tvpBinaryPath; }
  void setTvpBinaryPath(const QString&);

  QString getCspBinaryPath() { return m_cspBinaryPath; }
  void setCspBinaryPath(const QString&);

  QString getXdtsViewerBinaryPath() { return m_xdtsViewerBinaryPath; }
  void setXdtsViewerBinaryPath(const QString&);

  QString getOpenToonzBinaryPath() { return m_openToonzBinaryPath; }
  void setOpenToonzBinaryPath(const QString&);
  QString getOpenToonzArgument() { return m_openToonzArgument; }
  void setOpenToonzArgument(const QString&);

  // [USER_NAME]マクロに実際に代入される値。上書き設定が入力されていればそれを、
  // 空ならOSのシステムユーザ名（USER環境変数、無ければUSERNAME環境変数）を返す
  QString userName() const;
  // UserSettingsPopupの入力欄に表示する、上書き設定そのもの（空文字列＝未設定）
  QString userNameOverride() const { return m_userNameOverride; }
  void setUserNameOverride(const QString&);

  QString stuffFolderPath() const { return m_stuffFolderPath; }

  const PathMappings& pathMappings() const { return m_activeMappings; }
  const PathMappings& configuredMappings() const { return m_mappings; }
  const PathMappings& defaultMappings() const { return m_defaultMappings; }
  bool inheritsMappings() const { return m_inheritMappings; }
  void setPathMappings(const PathMappings& mappings, bool inherit);

  QString getLicenseFolderPath() const;

  // 実行ファイルのパス設定（TVPaint等）が、実在するかを確認し、無ければ警告を表示する。
  // 警告を翻訳して表示できるよう、翻訳ファイルのインストール後に呼ぶ
  void warnAboutMissingBinaries();

  QString languageCode() const { return m_languageCode; }
  void setLanguageCode(const QString&);
  // 翻訳ファイルを置くフォルダ（Stuffフォルダの下の"loc"）
  QString translationFolderPath() const;
  // 言語に対応する翻訳ファイルのパス（<loc>/PPM_[languageCode].qm）。存在するとは限らない
  QString translationFilePath(const QString& languageCode) const;
  // locフォルダにある翻訳ファイルの、languageCodeの一覧（ファイル名の昇順）
  QStringList availableLanguageCodes() const;
  // OSの表示言語のうち、翻訳ファイルがある言語のlanguageCode。無ければ空文字列（英語）
  QString systemLanguageCode() const;

  void saveListIndices(const QString& prjName, const QList<int>&,
                       const QList<int>&, const QList<int>&);
  void loadListIndices(const QString& prjName, QList<int>&, QList<int>&,
                       QList<int>&);

  void saveCurrentProjectName(const QString&);
  QString loadCurrentProjectName(const QString&) const;

  // プロジェクトごとに選択中の話数（Episode）を保持する。空文字列は「All
  // Episodes」を表す
  void saveCurrentEpisode(const QString& prjName, const QString& episode);
  QString loadCurrentEpisode(const QString& prjName) const;

  void saveWindowGeometry(const QByteArray&);
  QByteArray loadWindowGeometry() const;

  // MyWindow左パネルの表示モード（true=フローチャート、false=一覧）。プロジェクト非依存
  void saveLeftPanelFlowchartMode(bool isFlowchart);
  bool loadLeftPanelFlowchartMode() const;

  // MyWindowの左右パネルQSplitter（mainSplitter）の分割位置。QSplitter::saveState()/restoreState()の
  // 結果をそのまま保存する。プロジェクト非依存
  void saveMainSplitterState(const QByteArray&);
  QByteArray loadMainSplitterState() const;

  // カット一覧（MyTable）の列の表示/非表示。columnKeyは列を識別する固定の文字列
  // （列インデックスではない。列構成が変わっても既存ユーザーの設定がずれないようにするため）。
  // プロジェクト非依存
  void saveColumnVisible(const QString& columnKey, bool visible);
  bool loadColumnVisible(const QString& columnKey, bool defaultVisible) const;

  // カット一覧のThumbnail列のサイズ（true=大）。プロジェクト非依存
  void saveThumbnailSizeLarge(bool isLarge);
  bool loadThumbnailSizeLarge() const;
};

#endif
