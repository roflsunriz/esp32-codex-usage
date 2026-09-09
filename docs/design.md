# 設計

## 目的と境界

ESP32-2432S028Rが、Codexの5時間制限と週間制限を自律的に表示し、Wi-Fi経由で約60秒ごとに更新する。初期設定とブラウザデバイス認証だけはPCまたはスマートフォンを使うが、その後の常時接続は要求しない。初期設定APは15分で自動停止し、接続タブから再開できる。

対象ボードはESP32-WROOM-32、4 MBフラッシュ、ILI9341、XPT2046、2.8インチ320×240 LCDである。表示とタッチは別SPI配線なので、LovyanGFXで別SPIホストを明示する。

## 構成

```text
main.cpp
  ├─ CodexUsageDisplay / LovyanGFX
  ├─ DisplayState ── 消灯・復帰・操作の状態遷移
  └─ app-model ── Snapshot / Command
                   │
                   └─ network.cpp（FreeRTOS worker）
                        ├─ Wi-Fi / 初期設定AP / WebServer
                        ├─ ConfigStore ── ESP32 NVS
                        └─ CodexClient
                             ├─ device authorization
                             ├─ OAuth token refresh
                             └─ wham usage JSON
```

## 表示層

[include/board-display.h](../include/board-display.h) は、LovyanGFX 1.2.28の `LGFX_Device` をボード固有設定で構成する。TFTはGPIO14/12/13/15/2、backlightはGPIO21、タッチはGPIO25/39/32/33/36である。TFTは `SPI2_HOST`、タッチは `SPI3_HOST`、TFTのリセットはボードEN/RST接続として `-1` にする。

ILI9341は内部240×320として設定し、`offset_rotation=1`で初期表示を横向き320×240にする。タッチのraw範囲はX 300–3900、Y 3700–200、タッチ側のバスは表示と共有しない。backlight PWMは低周波1200 Hzを使い、消灯は輝度0で行う。

[include/display-state.h](../include/display-state.h) はドライバから独立した状態機械である。消灯期限を過ぎると `awake=false` になり、消灯中の最初の押下は復帰だけに消費する。リリース後の次の立ち上がりだけをUI操作として返す。タイマー差分はunsignedのラップアラウンドを利用する。

GPIO0のBOOTボタンは起動後の単押しで表示回転を切り替える。回転は0度/180度を設定値として保存し、LovyanGFXの表示回転とタッチ座標変換へ同じ状態を反映する。設定データはversion 1からversion 2へ移行し、旧データは回転なし（`displayFlipped=false`）で補完する。起動時にBOOTが押されている場合はESP32 ROMの書き込みモードを優先し、ランタイム操作とは混同しない。

描画はWi-Fi開始前に確保した320×240の8-bit Sprite上で完成させ、1回の転送でLCDへ反映する。約76.8 KBを使い、全画面を消してから要素を描く途中経過が見えるちらつきを防ぐ。

Wi-Fiの関連付けとIPアドレス取得を区別し、接続試行は30秒、DHCP待ちは120秒の猶予を取る。検証APでは再起動後にDHCPの応答待ちが続いたため、IP未取得時はRFC 2131 §4.1のBROADCASTビットで応答を要求する。`src/dhcp-broadcast.cpp` はSDKの追加オプション処理を保持してこのビットだけを設定し、取得後の更新要求は変更しない。根拠は [RFC 2131](https://www.rfc-editor.org/info/rfc2131/) と `lwip_default_hooks.h` の宣言。SDK更新時はリンクフックと実機再接続を確認する。

## UIとデータ

画面は使用量、設定、接続の3タブで構成する。使用量タブは `Snapshot` の5時間・週間ウィンドウを表示する。設定タブは15秒、30秒、1分、2分、5分、10分、30分、1時間、2時間を許可する。接続タブはWi-Fi状態、AP情報、デバイスコード、IPアドレスを表示する。

[include/usage-data.h](../include/usage-data.h) はレスポンスの `rate_limit.primary_window` と `secondary_window` を、フィールドの順序ではなく `limit_window_seconds` で分類する。18,000秒を5時間、604,800秒を週間として扱い、未知の期間は無視する。負値、NaN、形式不正、同じ期間の重複は拒否する。

## 認証と通信

[src/codex-client.cpp](../src/codex-client.cpp) は次の公式デバイス認証フローを使う。

1. `https://auth.openai.com/api/accounts/deviceauth/usercode` でデバイスコードを取得する。
2. `https://auth.openai.com/api/accounts/deviceauth/token` を一定間隔でポーリングする。
3. 取得した認証コードとverifierを `https://auth.openai.com/oauth/token` へ渡す。
4. access tokenのJWT payloadから期限とアカウントIDを検証する。
5. 期限前にrefresh tokenで更新し、`https://chatgpt.com/backend-api/wham/usage` へBearer tokenでアクセスする。

使用量エンドポイントは公開API契約ではなく、公式ログイン済みクライアントが使うprivate endpointである。レスポンス形式、認証方式、利用可否は変更され得るため、失敗を空データとして隠さず、再ログインや後で再試行できる状態にする。

2026-09-10の実アカウントでは、通常の `rate_limit.primary_window` は604,800秒で、`secondary_window` は未提供だった。5時間（18,000秒）が提供されない場合は画面を `--` / unavailable とし、週間値や `additional_rate_limits` のSpark枠を5時間の代わりに使わない。別クォータを別ウィンドウへ置き換えると、表示値の意味が変わるためである。ESP32自身の初回認証とトークン更新を経た使用量取得も実機で確認した。

トークンとWi-Fi設定は [include/config-store.h](../include/config-store.h) / [src/config-store.cpp](../src/config-store.cpp) でNVS namespace `codex-usage` に保存する。設定はversion 2、トークンはversion 1のJSON blobとし、読み込み時に長さ、期限、タイムアウトを検証する。壊れたエントリは削除して再設定を促す。

TLSは [include/trusted-roots.h](../include/trusted-roots.h) のルート証明書で検証する。証明書は [scripts/update-ca.py](../scripts/update-ca.py) で公式配布元から更新し、対象ホストのTLS接続を確認する。APIキーは使わない。

## 初期設定と自律運転

初期設定ではESP32がランダムなWPA2パスワードの `Codex-Usage-XXXX` APを開始し、LCDの接続タブにSSIDとパスワードを表示する。接続端末は `192.168.4.1` へアクセスして2.4 GHz Wi-Fi情報を登録する。ESP32のAPはインターネットへ中継しないため、ログインを開始した後は端末をインターネット接続へ切り替える。ESP32はデバイスコードの承認を自律的にポーリングする。APは15分で閉じ、設定タブから再開できる。

`network.cpp` のworkerは、次の処理をUI描画と分離して実行する。

- Wi-Fi接続・再接続と時刻同期。
- 初期設定HTTP画面（日本語・英語）。
- デバイス認証の保留・完了・期限切れ。
- 60秒周期の使用量取得と前回値保持。
- UIからの更新、再ログイン、AP再開、消灯時間変更。

## 依存関係と検証

PlatformIOの対象環境は、pioarduino platform 55.03.311、Arduino core 3.3.11、ESP-IDF 5.5.5、`esp32dev`である。旧Arduino core 2.0.17はWebServerの既知の修正（GHSA-8cmm-3887-r32j、GHSA-5476-9jjq-563m）が不足するため採用しない。画面はLovyanGFX 1.2.28、JSONはArduinoJson 7.4.3、ネイティブ状態テストはUnity 2.6.1を使う。実機書き込み前にフルフラッシュを保存し、検証後は `scripts/flash-backup.py restore` で元イメージを0x0へ復元する。Secure Boot/Flash Encryptionに対するeFuse操作は設計対象外である。

通常の出荷用プロファイルは `cyd` で、診断用の `cyd-diagnostics` は合成タッチ・仮想時刻・画面状態取得を行うテスト専用である。診断デモのWebアクセスfixtureはHTTP/UI経路用で、実アカウントのtoken refreshを再現しない。

トークンはNVSのJSON blobとして保存する。NVSの文字列エントリ上限に依存せず、長いrefresh tokenを保存できるようにblob APIを使う。バックアップと認証情報は`.local`などGit管理外に置く。

## 情報源

- デバイス認証の公式案内: https://learn.chatgpt.com/docs/auth
- LovyanGFXのCYD設定例: https://github.com/lovyan03/LovyanGFX/issues/637
- Espressif esptool: https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/basic-commands.html
- 初期調査時の公式ソース取得コミット: `.local/codex-source` の `17e64839eb1e30632eef4a0147862345fccb61cc`
