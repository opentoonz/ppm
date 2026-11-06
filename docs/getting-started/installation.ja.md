[English](installation.md)

# インストール

PPM はインストーラーを使いません。配布されている ZIP ファイルを展開し、中の `ppm.exe` を起動するだけで使えます。

このページは Windows 向けの配布物を説明しています。macOS のソースビルド（macOS 15 以降、Apple Silicon、Qt 6.12）の手順、設定ファイルの場所、現在の制限は、[macOS でのビルドと起動](../about/contributing.ja.md#macos-でのビルドと起動) を参照してください。

## 動作環境

| 項目 | 内容 |
|---|---|
| OS | Windows 10 / 11（64ビット） |
| ネットワーク | カットのファイルを置いた共有フォルダ（ファイルサーバーや NAS）に、各 PC からアクセスできること |
| 作業用のアプリ（任意） | CLIP STUDIO PAINT、TVPaint Animation、OpenToonz、XDTS を開けるタイムシートアプリなど。PPM の一覧から各カットのファイルを開くときに使います |

PPM 自体は、作業用のアプリがなくても動きます。
作業用のアプリのパスを PPM に設定しなかった場合は、ファイルの拡張子に関連付けられたアプリで開きます（[初回設定](initial-setup.ja.md) を参照）。

## 配布物の構成

<!-- 要確認：配布形式（GitHub Releases の ZIP など）が決まったら、ダウンロード先へのリンクとファイル名を書く -->

配布用の ZIP ファイルを展開すると、次のような構成になっています。

```
PPM/
├─ ppm.exe           … PPM の本体
├─ conf.ini          … Stuff フォルダの場所を指定する設定ファイル
├─ README.txt        … 説明書き（マニュアルへのリンクなど）
├─ Qt6Core.dll など  … 動作に必要なライブラリ（Qt）
├─ platforms/ など   … 同上
└─ stuff/            … Stuff フォルダ（プロジェクトや翻訳ファイルの置き場所）
   ├─ projects/      … プロジェクトファイル（.ppmprj）
   ├─ loc/           … 翻訳ファイル（UI の言語）
   ├─ rename_scripts/… リネームスクリプト
   ├─ sample_data/   … チュートリアル用のサンプルデータ
   └─ LICENSE/       … ライセンス文書
```

インストールの手順は次のとおりです。

1. ZIP ファイルを、任意のフォルダに展開します。
   （例：`C:\Tools\PPM`。書き込みに管理者権限が必要な `C:\Program Files` は避けてください）
2. `ppm.exe` をダブルクリックして起動します。

アンインストールするときは、展開したフォルダを削除します。
ユーザーごとの設定（[初回設定](initial-setup.ja.md) で行う内容）も消したいときは、
`%LOCALAPPDATA%\PPM\usersettings.ini` も削除してください。

## conf.ini と Stuff フォルダ

**Stuff フォルダ**は、PPM が使うプロジェクトファイル・翻訳ファイル・リネームスクリプトなどをまとめたフォルダです。
PPM は起動時に、`ppm.exe` と同じフォルダにある `conf.ini` を読んで、Stuff フォルダの場所を知ります。

```ini
[General]
StuffFolderPath=stuff
```

- 相対パスを書いた場合は、`ppm.exe` のあるフォルダを起点とします。上の例では、`ppm.exe` の隣にある `stuff` フォルダが Stuff フォルダになります。
- 共有フォルダに置いた Stuff フォルダを指定することもできます。
  スタジオの全員が同じ Stuff フォルダを見るようにすると、プロジェクトの設定を一か所で管理できます。
  この場合は、パスの区切りにスラッシュ（`/`）を使うのが簡単です（例：`StuffFolderPath=//fileserver/anime/PPM/stuff`）。
- `conf.ini` の中では、バックスラッシュ（`\`）は次の文字と組み合わせて特殊な文字を表す記号（エスケープ文字）として扱われます。
  たとえば `\t` はタブに変わり、`\P` などそれ以外の多くの組み合わせでは、2文字とも消えてしまいます。
  バックスラッシュを使うときは、`\\` のように2つ重ねて書いてください（例：`StuffFolderPath=\\\\fileserver\\anime\\PPM\\stuff`）。
  値をダブルクォートで囲んでも、この扱いは変わりません。

Stuff フォルダの詳しい構成と、スタジオでの運用方法は、[Stuff フォルダ](../administration/stuff-folder.ja.md) と [共有フォルダでの運用](../administration/shared-operation.ja.md) を参照してください。

---

[← 目次に戻る](../index.ja.md)
