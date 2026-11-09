[English](README-en.md)

# SETOLIA

OBS Studio 用セットリストプラグイン

## 主な機能

- 配信時間に同期したタイムスタンプ
- リアルタイム時計表示
- [歌詞検索サイト](https://utaten.com)へのショートカットボタン
- 設定済みブラウザソース追加ボタン
- カスタムテンプレートに対応
- Stream Deck からの操作に対応

## インストール方法

Windows、MacOS、Linux 用のバイナリは、[リリース](https://github.com/Yagami-Huki/setolia/releases)ページで入手可能です。

また、[Booth](https://logicbox.booth.pm/items/7541386) にて最新の Windows 用のインストーラーを入手可能です。

Windows 版は OBS 33 の新しいプラグイン構造と、OBS 32 以前の構造の両方に対応しています。

### Windows(zip で手動インストールする場合)

zip の中身を `C:\ProgramData\obs-studio\plugins\setolia\` に展開してください。展開後は次のような構成になります。

```
C:\ProgramData\obs-studio\plugins\setolia\
├── setolia.dll          (OBS 33 以降が読み込む)
├── bin\64bit\setolia.dll (OBS 32 以前が読み込む)
└── data\
```

2 つの `setolia.dll` は同じファイルです。OBS 33 以降は直下の dll を読み込み、`bin\64bit` にある方はスキップします。

### Windows(ポータブル版の OBS)

- OBS 33 以降:`<OBS>\plugins\setolia\setolia.dll` と `<OBS>\plugins\setolia\data\`(ポータブル版では `C:\ProgramData` のプラグインは読み込まれません)
- OBS 32 以前:`<OBS>\obs-plugins\64bit\setolia.dll` と `<OBS>\data\obs-plugins\setolia\`

### 旧バージョンからの更新時の注意

以前のバージョンを `C:\Program Files\obs-studio\obs-plugins\64bit\setolia.dll` に置いていた場合、OBS 32 以前ではプラグインが重複して読み込まれることがあります。インストーラーは旧ファイルを検出すると削除するかを確認します。zip で手動インストールする場合は、旧ファイル(`obs-plugins\64bit\setolia.dll` と `data\obs-plugins\setolia\`)を削除してください。

## 使用方法

[Booth](https://logicbox.booth.pm/items/7541386) にてユーザーマニュアルを公開しているため、こちらをご参照ください。

### OBS 33 以降をお使いの場合

OBS 33 では、ローカルファイルとして読み込んだブラウザソースから `127.0.0.1` への通信がブロックされるため、テンプレートはプラグイン内の HTTP サーバー(`http://127.0.0.1:8080/templates/...`)から配信するようになりました。

以前のバージョンで追加したテンプレートのブラウザソースは、OBS の起動時およびシーンコレクションの切り替え時に自動で URL 方式に変換されるため、テンプレートを再追加する必要はありません。

## ライセンスと謝辞

このソフトウェアは、GPL-3.0 ライセンスのもとで公開されています。

このプロジェクトは、[OBS Project](https://github.com/obsproject) によって作成された [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) を使用して開発されました。

また、このソフトウェアに付属する標準のテンプレートは、MIT ライセンスのもとで公開されています。
