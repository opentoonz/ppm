[English](license.md)

# ライセンス

## PPM のライセンス

PPM は、**BSD 3-Clause License**（三条項 BSD ライセンス）で公開しています。

```
All contributions by Shun Iwasawa:
Copyright (c) 2026, Shun Iwasawa
All rights reserved.

All other contributions:
Copyright (c) 2026, the respective contributors.
All rights reserved.
```

各貢献者は、自分が貢献した部分の著作権を持ちます。誰がどの部分に貢献したかは、Git の履歴に記録されています。

このライセンスのもとで、PPM を自由に使い、改変し、再配布できます。商用の制作でも使えます。
ただし、次の条件があります。

- ソースコードを再配布するときは、上の著作権表示、条件、免責事項を残します。
- 実行ファイルの形で再配布するときは、上の著作権表示、条件、免責事項を、付属の文書などに含めます。
- 著作権者や貢献者の名前を、許可なく、派生物の宣伝や推奨に使ってはいけません。
- PPM は無保証です。PPM を使ったことで生じた損害について、著作権者や貢献者は責任を負いません。

正式な条文は、配布物では Stuff フォルダの `LICENSE/LICENSE.txt` に、ソースコードではリポジトリ直下の `LICENSE` にあります。
PPM の **PPM について**（macOS の **PPM** メニューまたは Windows の歯車メニューから開きます）にも表示されます。
上の説明は要約です。条文の内容が優先されます。

## サードパーティのライセンス（Qt）

PPM は、[Qt](https://www.qt.io/) 6 を使って作られています。
Qt は、**GNU Lesser General Public License（LGPL）バージョン 3** のもとで使っています。

- PPM は、Qt のライブラリを DLL として使っています（動的リンク）。配布物に含まれる `Qt6Core.dll` などの Qt のファイルは、Qt のものです。
- LGPL の条件にしたがって、これらの DLL を、互換性のある別のビルドの Qt に置き換えることができます。
- Qt のソースコードは、Qt の公式サイトから入手できます。
- LGPL の条文は、Stuff フォルダの `LICENSE/LICENSE_qt.txt` にあります。LGPL v3 は GPL v3 に条件を追加する形の条文なので、GPL v3 の条文も `LICENSE/LICENSE_gpl3.txt` に入れています。どちらも、PPM の **PPM について**（macOS の **PPM** メニューまたは Windows の歯車メニューから開きます）に表示されます。

PPM が使っている主な Qt のモジュールは、Qt Core、Qt GUI、Qt Widgets、Qt Network、Qt Concurrent、Qt QML、Qt 5 Core Compatibility APIs です。

## アイコン

PPM の画面のアイコンには、次のものがあります。

- **PPM のために作ったアイコン**：PPM と同じ BSD 3-Clause License です。
- **[Tabler Icons](https://tabler.io/icons) をもとにしたアイコン**：検索、警告、ログ、リテイクのボタンと、一部のステップのアイコンです。Tabler Icons は **MIT License** で公開されています。条文は、Stuff フォルダの `LICENSE/LICENSE_tabler_icons.txt` にあり、**PPM について** にも表示されます。
- **Google スプレッドシートのロゴ**：「Googleスプレッドシートに入力」のステップのアイコンに使っています。Google LLC の商標です。

---

[← 目次に戻る](../index.ja.md)
