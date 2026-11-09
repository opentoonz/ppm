PPM - CI test build

[English]

This package contains PPM, Qt runtime libraries, sample data, licenses, and the
English and Japanese manuals in docs/. BUILD.txt identifies the source commit.
Use a copy of your data when testing. This is a development build, not a release.

Windows x64:
  Extract the entire ZIP and open PPM/ppm.exe. conf.ini selects the included
  stuff folder. The required Visual C++ runtime DLLs are next to ppm.exe.

macOS 15 or later, Apple Silicon:
  Extract the tar.gz archive (tar -xzf ppm-macos-arm64.tar.gz).
  Create ~/Library/Application Support/PPM/conf.ini with these contents, replacing
  the example with the absolute path to the extracted stuff folder:

    [General]
    StuffFolderPath=/absolute/path/to/PPM/stuff

  Open PPM/PPM.app. The app is ad-hoc signed and not notarized. If macOS blocks
  this trusted build, use System Settings > Privacy & Security > Open Anyway.
  Use Command+, to open Preferences and select your drawing and timesheet apps.
  Mixed Windows/macOS production shares are outside the supported scope.

Configuration details and limitations: docs/about/contributing.md
License: LICENSE and stuff/LICENSE/

[日本語]

このパッケージには PPM、Qt の実行用ライブラリ、サンプルデータ、ライセンス、
docs/ 内の英語・日本語マニュアルが含まれます。ソースのコミットは BUILD.txt に
記載しています。テストにはデータのコピーを使ってください。正式リリースでは
なく、開発中のビルドです。

Windows x64:
  ZIP 全体を展開して PPM/ppm.exe を起動します。conf.ini は同梱の stuff フォルダを
  指定しています。必要な Visual C++ ランタイム DLL は ppm.exe と同じフォルダにあります。

macOS 15 以降、Apple Silicon:
  tar.gz を展開します（tar -xzf ppm-macos-arm64.tar.gz）。
  ~/Library/Application Support/PPM/conf.ini を作り、上の例の StuffFolderPath を、
  展開した stuff フォルダの絶対パスに置き換えて記入してください。
  PPM/PPM.app を起動します。アプリは ad-hoc 署名付きですが、公証はされていません。
  信頼できるビルドが macOS にブロックされた場合は、「システム設定」>
  「プライバシーとセキュリティ」>「このまま開く」で許可します。
  Command+, で環境設定を開き、作画用アプリとタイムシート用アプリを選べます。
  Windows と macOS が混在する制作共有フォルダでの運用はサポート対象外です。

設定方法と制限: docs/about/contributing.ja.md
ライセンス: LICENSE と stuff/LICENSE/
