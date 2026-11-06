[English](stuff-folder.md)

# Stuff フォルダ

**Stuff フォルダ**は、PPM が使う設定をまとめて置くフォルダです。
プロジェクトのファイル、翻訳ファイル、リネームスクリプト、環境設定の初期値などが入ります。

Stuff フォルダを共有フォルダに置き、全員の PPM が同じ Stuff フォルダを見るようにすると、プロジェクトの設定を一か所で管理できます（[共有フォルダでの運用](shared-operation.ja.md) を参照）。

## フォルダの構成

```
stuff/
├─ projects/                 … プロジェクトのファイル（.ppmprj）
│  └─ sample_project.ppmprj
├─ loc/                      … 翻訳ファイル（.qm）
│  └─ PPM_ja.qm
├─ rename_scripts/           … リネームスクリプト（.js）
│  └─ genga_to_uchikomi.js
├─ LICENSE/                  … ライセンス文書
├─ sample_data/              … チュートリアル用のサンプルデータ
└─ defaultusersettings.ini   … 環境設定の初期値（任意。導入担当者が作ります）
```

| フォルダ・ファイル | 内容 |
|---|---|
| `projects` | プロジェクトのファイルです。ファイル名（拡張子を除く）が、プロジェクトIDになります。メイン画面の **プロジェクトID** には、このフォルダにあるプロジェクトが並びます（[プロジェクトの保存とテスト](../designing/saving-and-testing.ja.md) を参照）。 |
| `loc` | 画面の言語の翻訳ファイルです（[UI の言語と翻訳ファイル](translations.ja.md) を参照）。 |
| `rename_scripts` | **フォルダ内容をコピー** のステップで使うリネームスクリプトです（[リネームスクリプト](../integrations/rename-scripts.ja.md) を参照）。 |
| `LICENSE` | PPM と、PPM が使っているライブラリ（Qt）のライセンス文書です。**PPM について** の画面に表示されます。消さないでください。 |
| `sample_data` | サンプルプロジェクトが使うカットのファイル、プロジェクト情報、ログです（[チュートリアル](../getting-started/tutorial.ja.md) を参照）。サンプルプロジェクトを使わないなら、`projects` フォルダの `sample_project.ppmprj` と一緒に消してもかまいません。 |
| `defaultusersettings.ini` | 環境設定の初期値です（下の「環境設定の初期値」を参照）。配布物には含まれていないので、必要なら作ります。 |

プロジェクト設定で、ルートフォルダなどのパスを相対パスで書いた場合は、Stuff フォルダを起点としたパスになります（[基本情報](../designing/basics.ja.md) を参照）。

## conf.ini での指定

Windows では、PPM は起動時に `ppm.exe` と同じフォルダにある `conf.ini` を読んで、Stuff フォルダの場所を知ります。

macOS では、`~/Library/Application Support/PPM` に `conf.ini` を置きます。フォルダがなければ作成してください。この設定はユーザーごとに保存され、そのユーザーが使うすべての PPM ビルドで共用します。相対パスはそのフォルダを起点に解決し、絶対パスには Mac からアクセスできるパス（例：`/Volumes/anime/PPM/stuff`）を指定します。ソースビルドの設定方法と制限は、[macOS でのビルドと起動](../about/contributing.ja.md#macos-でのビルドと起動) を参照してください。

```ini
[General]
StuffFolderPath=//fileserver/anime/PPM/stuff
```

- 相対パスで書いた場合は、`conf.ini` のあるフォルダを起点とします。Windows では `ppm.exe` のあるフォルダです。配布物の `conf.ini` は `StuffFolderPath=stuff` で、`ppm.exe` の隣の `stuff` フォルダを指しています。
- パスの区切りには、スラッシュ（`/`）を使ってください。バックスラッシュ（`\`）を使う場合は、`\\` のように2つ重ねて書く必要があります（[インストール](../getting-started/installation.ja.md) を参照）。
- `conf.ini` がない場合や、`StuffFolderPath` が空の場合は、Stuff フォルダが見つからず、プロジェクトを開けません。
- Windows では `conf.ini` は `ppm.exe` ごとに1つです。共有フォルダに置いた1つの `ppm.exe` を全員で使う場合は、`conf.ini` も全員で共有されます。

Stuff フォルダを変えたときは、PPM を起動し直すと反映されます。

## 環境設定の初期値

Stuff フォルダに `defaultusersettings.ini` を置くと、全員の **環境設定** の初期値をまとめて決められます。
たとえば、スタジオの PC に CLIP STUDIO PAINT が同じ場所にインストールされているなら、そのパスを書いておくと、各スタッフが設定しなくて済みます。

```ini
[UserSettings]
LanguageCode=ja
CspBinaryPath=C:/Program Files/CELSYS/CLIP STUDIO 1.5/CLIP STUDIO PAINT/CLIPStudioPaint.exe
TvpBinaryPath=
OpenToonzBinaryPath=
OpenToonzArgument=
XdtsViewerBinaryPath=
```

| キー | 環境設定の項目 |
|---|---|
| `LanguageCode` | **言語**。翻訳ファイル `PPM_ja.qm` の `ja` の部分を書きます。空にすると英語です。 |
| `TvpBinaryPath` | **TVPaint実行ファイルのパス** |
| `CspBinaryPath` | **Clip Studio実行ファイルのパス** |
| `OpenToonzBinaryPath` | **OpenToonz実行ファイルのパス** |
| `OpenToonzArgument` | **OpenToonz起動時の引数** |
| `XdtsViewerBinaryPath` | **XDTS用アプリケーションのパス** |

- 書かなかったキーと、空のキーは、環境設定の既定のまま（空欄）になります。ただし `LanguageCode` は、書かなかった場合、OS の表示言語に合わせます（その言語の翻訳ファイルが無いときは英語）。
- **ユーザー名 [USER_NAME]** は、初期値を決められません。
- パスの区切りには、スラッシュ（`/`）を使ってください。バックスラッシュの扱いは、`conf.ini` と同じです。
- 上のパスは例です。実際のインストール先を確かめて書いてください。

### 初期値が使われる項目

初期値は、各スタッフが **環境設定で変えていない項目** に使われます。
一度変えた項目は、そのスタッフの設定ファイル（`usersettings.ini`）に保存され、それ以降は初期値より優先されます（[環境設定](../using/preferences.ja.md) を参照）。

そのため、`defaultusersettings.ini` を後から書き換えると、まだ変えていないスタッフには、次に PPM を起動したときから新しい値が使われます。
すでに自分で設定したスタッフには、反映されません。

---

[← 目次に戻る](../index.ja.md)
