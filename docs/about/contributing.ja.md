[English](contributing.md)

# 開発に参加する

PPM は、オープンソースのソフトウェアです。
ソースコードの修正、翻訳、マニュアルの改善など、どんな形の協力も歓迎します。
協力していただいた内容は、[ライセンス](license.ja.md) のとおり、BSD 3-Clause License で公開されます。

## 進め方

PPM のリポジトリは、GitHub の [opentoonz/ppm](https://github.com/opentoonz/ppm) です。

1. 大きな変更をするときは、先に [Issue](reporting-issues.ja.md) で相談してください。考え方の違いで、作業がむだにならないようにするためです。
2. リポジトリをフォークして、作業用のブランチで変更します。
3. ソースコードを変えたときは、整形してからコミットします（下の「コードの書き方」を参照）。
4. `main` ブランチに向けて、プルリクエストを出します。

## プルリクエストのお願い

プルリクエストは、レビューする人が内容を理解できて、はじめて取り込めます。
AI を使って開発する人が増え、たくさんの変更をまとめたプルリクエストを手軽に作れるようになりました。
レビューを進めやすくするために、次のことにご協力ください。

- **1つの改良につき、1つのプルリクエストにしてください。** 関係のない修正が混ざっていると、一部だけを取り込むことができません。
- **変更が大きくなったときは、分けられないか考えてください。** たとえば、下準備のリファクタリングと、機能の追加を、別のプルリクエストにします。
- **何を変えたかだけでなく、なぜ必要なのかと、設計の意図を書いてください。** 制作のどんな場面で役に立つのか、ほかの方法と比べてなぜこの方法にしたのか、といったことです。
  AI を使って書いたコードの場合は、使ったプロンプトも添えてもらえると、意図が伝わりやすくなります。
- **機能を変えたり追加したりしたときは、マニュアルも直してください。** 直していない場合は、そのことをプルリクエストに書いてください（下の「マニュアルへの協力」を参照）。

プルリクエストを作ると、説明欄に、これらの項目を書くためのテンプレートが入ります。
すべてを完璧にそろえる必要はありません。迷ったときは、プルリクエストや Issue で気軽に相談してください。

AI のコーディングツールを使って開発するときは、リポジトリ直下の `AGENTS.md` を読ませてください。このリポジトリの決まり（変更の範囲、コードの書き方、マニュアルの更新など）をまとめています。Claude Code は、`CLAUDE.md` を通して自動で読みます。

## ソースからのビルド

### Windows で必要なもの

| もの | 内容 |
|---|---|
| OS | Windows 10 / 11（64ビット） |
| コンパイラ | Visual Studio 2022（「C++ によるデスクトップ開発」） |
| CMake | 3.5 以上 |
| Qt | Qt 6（MSVC 2022 64-bit 版）。開発では Qt 6.12 を使っています。インストールするときに、追加のライブラリの **Qt 5 Compatibility Module** も選んでください。 |

使う Qt のモジュールは、Qt Widgets、Qt Network、Qt Core5Compat、Qt Concurrent、Qt QML、Qt Linguist Tools です。

### Windows でのビルド

リポジトリのフォルダで、次のように実行します。
`QTDIR` には、Qt をインストールしたフォルダ（`bin` や `lib` があるフォルダ）を指定します。
環境変数 `QTDIR` を設定しておくこともできます。

```bash
cmake -S sources -B build -G "Visual Studio 17 2022" -A x64 -DQTDIR=C:/Qt/6.12.0/msvc2022_64
```

```bash
cmake --build build --config Release
```

`build/Release/ppm.exe` ができます。
`build` フォルダにできる `ppm.sln` を Visual Studio で開いて、ビルドすることもできます。

### Windows で動かす

`ppm.exe` を動かすには、Qt の DLL と、`conf.ini` が必要です。

1. Qt の `windeployqt` で、必要な DLL を `ppm.exe` の隣にコピーします。

   ```bash
   C:/Qt/6.12.0/msvc2022_64/bin/windeployqt.exe build/Release/ppm.exe
   ```

2. `ppm.exe` の隣に `conf.ini` を作り、リポジトリの `stuff` フォルダを Stuff フォルダとして指定します（[インストール](../getting-started/installation.ja.md) を参照）。

   ```ini
   [General]
   StuffFolderPath=../../stuff
   ```

`stuff` フォルダには、サンプルのプロジェクトとデータがあります。
ビルドした PPM の動作は、まず [チュートリアル：カットを1つ最後まで進める](../getting-started/tutorial.ja.md) の手順で確かめてください。

### macOS でのビルドと起動

ソースからのビルドは、**macOS 15 以降、Apple Silicon（arm64）、Qt 6.12** を対象とします。Intel Mac はサポート対象外です。Xcode 16 以降（コマンドラインツールを含む）、CMake、macOS 用 Qt、**Qt SVG**、**Qt 5 Compatibility Module** をインストールしてください。CMake は使用する Qt が対応するバージョンを選びます。

リポジトリのフォルダで、`QTDIR` を Qt のインストール先に合わせて実行します。

```bash
export QTDIR="$HOME/Qt/6.12.0/macos"
"$QTDIR/bin/qt-cmake" -S sources -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DQTDIR="$QTDIR"
cmake --build build --parallel
```

`build/PPM.app` ができます。deployment target は Qt の既定値を使いますが、PPM のサポート対象は macOS 15 以降です。 macOS がダークモードでも、PPM はライト表示を使います。

ユーザーごとの設定フォルダを作ります。

```bash
mkdir -p "$HOME/Library/Application Support/PPM"
```

`~/Library/Application Support/PPM/conf.ini` を作り、Stuff フォルダの絶対パスを指定します。次の例のパスを、チェックアウト先に合わせて置き換えてください。

```ini
[General]
StuffFolderPath=/absolute/path/to/ppm/stuff
```

`open build/PPM.app` で起動します。インストールした Qt のライブラリを参照するソースビルドなので、Qt のインストール先を残してください。単体で配布できるアプリではありません。生成される `Contents/Resources/qt.conf` は、その Qt のプラグインを参照します。Xcode など複数の構成に対応するジェネレーターでは、`--config Release` を付けてビルドし、`build/Release/PPM.app` を起動します。どのビルドも同じユーザー用の設定フォルダを使います。

相対パスの `StuffFolderPath` は、アプリの場所や作業ディレクトリに関係なく `~/Library/Application Support/PPM` を起点に解決します。`/Volumes/anime/PPM/stuff` のようなローカルの絶対パスも使えます。ユーザーごとの設定は `~/Library/Application Support/PPM/usersettings.ini` に保存されます。[Stuff フォルダ](../administration/stuff-folder.ja.md) と [環境設定](../using/preferences.ja.md) も参照してください。

macOS のソースビルドは、起動と設定の読み書きに対応しています。外部アプリの選択・起動、ZIP 操作、Windows と macOS が混在する共有フォルダでの運用は、このビルドではサポートしていません。評価には Stuff フォルダと制作データのテスト用コピーを使ってください。サンプルプロジェクトを開くだけでも進捗ファイルが更新されることがあります。

### 起動に関するテスト

構成時のコマンドに `-DPPM_BUILD_TESTS=ON` を追加してビルドし、次を実行します。

```bash
ctest --test-dir build -C Release -R '^ppm_(configuration|sceneinfotag|confirmation)_tests$' --output-on-failure
```

設定ファイルの検索、Stuff フォルダの相対・絶対パス、ユーザー設定の保存先、終了時のカット情報タグの破棄、承認・工程内リテイク操作の確定とキャンセルを検証します。GitHub Actions は、Qt 6.12 を使って Windows x64 と macOS arm64 でビルドし、これらのテストを実行します。実際の画面の起動と制作現場のファイルサーバーでの動作は、別途確認が必要です。

### CI アプリパッケージのダウンロード

**Build** ワークフローが成功したら、リポジトリの **Actions** タブでその実行結果を開き、**Artifacts** からダウンロードします。アーティファクト名にはプラットフォームとソースのコミットが含まれ、14 日間保存されます。ダウンロードには GitHub へのログインが必要です。

ダウンロードしたアーティファクトの ZIP を展開し、中のアプリ用アーカイブも展開します。

- `ppm-windows-x64.zip`: `PPM/ppm.exe`、Qt の実行用ライブラリ、同梱の `stuff` フォルダを指定する `conf.ini` が入っています。Visual C++ ランタイム DLL は `ppm.exe` と同じフォルダにあります。
- `ppm-macos-arm64.tar.gz`: Qt のフレームワークとプラグインを含む `PPM/PPM.app` が入っています。tar アーカイブは実行権限とフレームワークのシンボリックリンクを保持します。上記の手順で `~/Library/Application Support/PPM/conf.ini` を作り、展開した `PPM/stuff` フォルダの絶対パスを指定してください。アプリは ad-hoc 署名付きですが、公証はされていません。信頼できるビルドが macOS にブロックされた場合は、**システム設定 > プライバシーとセキュリティ > このまま開く** で許可します。

どちらにもサンプルデータ、ライセンス、英語・日本語マニュアル、ソースのコミットを示す `BUILD.txt` が含まれます。評価用の開発ビルドなので、データのコピーを使ってください。上記の macOS の機能制限は、これらのパッケージにも適用されます。

CI は専用のステージングフォルダに Qt を配置し、macOS の依存ライブラリのパスと署名を検証します。その後、`PATH` に Qt を含めずにパッケージ内のアプリを起動し、5 秒間動作を続けることを確認します。この起動確認は最後にプロセスを終了させるもので、画面操作や制作ワークフローの検証は行いません。

### コードの書き方

プルリクエストを出す前に、変更した C++ のコードに、必ず clang-format をかけてください。
整形の設定は `sources/.clang-format` にあります。

`sources` フォルダで、次のように `beautification.sh` を実行します（Git Bash などで実行します）。

```bash
cd sources
./beautification.sh
```

`main` ブランチから変更した `.cpp`、`.h` ファイルに、まとめて clang-format がかかります。
clang-format は、Visual Studio に含まれているものや、LLVM のものを使えます。PATH の通った場所に置いてください。

画面に表示する文は、英語で書き、`tr()` で囲みます。日本語の訳は、翻訳ファイルに入れます（下を参照）。

## 翻訳への協力

PPM の画面の言語は、翻訳ファイルで追加できます。
翻訳ファイルの作り方は、[UI の言語と翻訳ファイル](../administration/translations.ja.md) の「言語を追加する」で説明しています。

- 翻訳を PPM に取り込んでほしいときは、翻訳した `.ts` ファイル（`sources/translations/PPM_言語コード.ts`）をプルリクエストで送ってください。新しい言語のときは、`sources/CMakeLists.txt` の `LANGUAGES` にも言語コードを足します。
- 今ある翻訳（日本語）の誤りや、わかりにくい訳の指摘も歓迎します。

## マニュアルへの協力

このマニュアルは、リポジトリの `docs` フォルダにある Markdown のファイルです。

- 各ページに、日本語版（`〜.ja.md`）と英語版（`〜.md`）があります。どちらかを直したときは、もう一方も合わせて直してください。片方しか直せない場合は、プルリクエストにそう書いてもらえれば、もう一方はこちらで直します。
- 画面のキャプチャは `docs/images/ja` に置きます。英語版のページも、日本語版のキャプチャを使います。
- マニュアルは、PPM の今の動きのとおりに書きます。動きがおかしいと思った点は、マニュアルを変えるのではなく、[Issue](reporting-issues.ja.md) で報告してください。

---

[← 目次に戻る](../index.ja.md)
