# Codex使用量モニター

ESP32-2432S028R（ILI9341 LCD、XPT2046タッチ）で、Codexの5時間制限と週間制限を表示するファームウェアです。Wi-Fiの初期設定、ブラウザデバイス認証、トークン更新、使用量取得、LCDの自動消灯までESP32側で動作します。

## 利用開始

配布ファイルは [v0.1.0-beta.1](https://github.com/roflsunriz/esp32-codex-usage/releases/tag/v0.1.0-beta.1) から取得できます。

先に [更新・書き込み手順](how-to-update.md) に従って元ファームウェアを退避し、このファームウェアを導入してください。初回の認証承認と更新トークンの実更新は未検証です。確認済みの範囲は [verification.md](verification.md) に記載しています。

1. 初回だけUSBで電源を入れます。LCDは最初に「使用量」タブを表示します。
2. LCDの「接続」タブを開き、表示された `Codex-Usage-XXXX` とランダムなWPA2パスワードを確認します。
3. スマートフォンまたはPCをESP32のAPへ接続し、ブラウザで `http://192.168.4.1` を開きます。
4. 自宅などの2.4 GHz Wi-FiのSSIDとパスワードを入力して保存します。ESP32のAPはインターネットへ中継しません。
5. ChatGPTのセキュリティ設定でデバイスコード認証を有効にしたうえで、Web画面のログインを押し、認証コードを控えます。スマートフォンをインターネットへ接続できるWi-Fiまたはモバイル回線へ切り替え、表示された公式認証ページでコードを承認します。
6. ESP32はAPから切断された後も認証完了をポーリングします。承認後はトークンをNVSへ保存し、約60秒ごとに使用量を更新します。

初期設定用APは15分で自動停止し、「接続」タブから再開できます。初期設定後はUSBを給電専用にでき、スマートフォンやPCを常時接続する必要はありません。

## LCDの操作

LCDは横向き320×240で、次の3タブを持ちます。

- **使用量**: 5時間制限と週間制限のプログレスバー、パーセンテージ、更新状態。
- **設定**: 自動消灯を15秒、30秒、1分、2分、5分、10分、30分、1時間、2時間から選択。
- **接続**: Wi-Fi状態、AP情報、認証コード、認証ページ、IPアドレス、再設定・再ログイン・更新。

サービスの応答に5時間ウィンドウが含まれない場合は `--` と表示します。週間値や別モデルの追加クォータを5時間の代わりには使いません。

消灯後の最初のタッチは復帰専用です。タッチを離すまでボタン操作として扱わず、リリース後の次のタッチから操作できます。消灯中もネットワーク更新は続きます。

本体のBOOTボタンを単押しすると、表示を180度回転します。回転状態はNVSへ保存され、次回起動にも引き継がれます。消灯中は点灯して反転します。起動時にBOOTを押し続ける操作はESP32の書き込みモードに使われるため、起動後に操作してください。

## ビルドプロファイル

通常の出荷用ビルドは `cyd` です。

```powershell
$env:PYTHONUTF8 = "1"
pio run -e cyd
```

`cyd-diagnostics` は合成タッチ、仮想時刻、画面状態取得を行う診断専用プロファイルです。USBシリアル操作を含むため配布しません。

ネイティブテストは次で実行します。

```powershell
$env:PYTHONUTF8 = "1"
python scripts/test-native.py
```

検証件数、実機で確認した範囲、初回認証に残る確認事項は [verification.md](verification.md) にまとめています。初期設定Web画面は、Node.js 22以降とGoogle Chromeを導入した環境で `python scripts/test-setup-ui.py` により再検証できます。

## 実装と依存関係

本番環境は、pioarduinoのPlatformIO platform 55.03.311（Arduino core 3.3.11、ESP-IDF 5.5.5）を使うArduino ESP32です。旧Arduino core 2.0.17はWebServerの既知の修正（GHSA-8cmm-3887-r32j、GHSA-5476-9jjq-563m）が不足するため使用しません。

- `lovyan03/LovyanGFX@1.2.28`
- `bblanchon/ArduinoJson@7.4.3`
- ネイティブテスト: `throwtheswitch/Unity@2.6.1`
- 開発ツール: `platformio==6.1.19`、`esptool==5.4.0`

PlatformIOのキャッシュは `platformio.ini` の `core_dir = .local/platformio` に分離しています。開発ツールはグローバルPythonへ入れず、プロジェクト外または専用venvで管理してください。

画面のピン設定は [include/board-display.h](include/board-display.h)、表示状態は [include/display-state.h](include/display-state.h)、ネットワーク統合は [src/network.cpp](src/network.cpp)、認証通信は [src/codex-client.cpp](src/codex-client.cpp)、Web設定画面は [web/setup.html](web/setup.html) にあります。

## セキュリティと復旧

通信は同梱ルート証明書でTLS検証します。トークンはNVSへJSON blobとして保存し、4,000バイトを超えるrefresh tokenも保存できるようにしています。物理アクセスがあれば、フラッシュ暗号化なしのNVSを読み出せる可能性があります。

実機へ書き込む前に必ず [how-to-update.md](how-to-update.md) の `scripts/flash-backup.py` で全フラッシュを退避してください。復旧時は同じスクリプトでサイズとSHA-256メタデータを検証してから書き戻します。`erase-all`、`--force`、eFuse変更は行いません。

## ドキュメント

- [更新・復旧手順](how-to-update.md)
- [検証手順と結果](verification.md)
- [設計](docs/design.md)
- [第三者ライセンス](THIRD-PARTY-NOTICES.md)
- [貢献ガイド](CONTRIBUTING.md)
- [セキュリティ](SECURITY.md)
- [サポート](SUPPORT.md)
