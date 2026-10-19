# findings.md

このリポジトリ固有の実現手段と調査知見の正本です。作業前に全文を読み、行動規範は [AGENTS.md](AGENTS.md) と [COMMON-AGENTS.md](COMMON-AGENTS.md) に従ってください。移動元の本文は以下に保持しています。

## 共通方針の適用と具体的な例外

| 項目 | このリポジトリでの扱い |
| --- | --- |
| 対象の共通ルール | 検証の言語別適用と実機条件 |
| 理由と適用範囲 | ESP32/C++ firmwareとしてPlatformIO・Python・ネイティブテストを使う。診断専用cyd-diagnosticsを通常配布に含めない。対象はボード設定、生成ヘッダーと出荷プロファイル。 |
| 同等の品質を保つ手段・検証 | Pythonテスト、ネイティブテスト、Web初期設定UI、cydビルド、生成物差分を検証し、実機認証・描画・復旧の未確認範囲を区別する。根拠: `platformio.ini`、`requirements-dev.txt`、`.github/workflows/ci.yml`、[更新手順](how-to-update.md)。 |

## 文書の役割と適用範囲

- 共通ルールの対象は「ローカル AGENTS.md との関係」「設計と実装方針」「検証」「ドキュメント」。この文書は本リポジトリの構成と既存運用に適用し、共通の行動規範を弱めません。
- ファイル配置・ツール・環境をここに集約する理由は、作業規範と実装上の事実を分け、次の作業者が根拠を確認できるようにするためです。下記の既存手段は同じ対象の設定・CI・検証記録と照合して使います。
- README は利用開始・操作、更新手順は更新と復旧、verification.md は検証手順と実測証跡、専門の設計・研究文書は詳細な根拠を担います。これらの用途は維持し、この文書から参照します。
- 更新・検証の代替は一律のBunコマンドに固定せず、下記の採用技術と既存CIに対応する検査を使います。実行不能や未検証の条件は成功扱いせず verification.md に残します。

## CIの分割境界

- 生成物・Python品質検査、PlatformIO nativeテスト、ESP32 firmware生成は相互の成果物を参照しないため、同じ固定requirementsとPlatformIOキャッシュを使うfail-fast matrixで並列実行する。firmware artifactは`firmware` shardだけが公開する。

<a id="agents-1"></a>

## 実装・検証で確認した事項

- 2026-09-10: 公式 `openai/codex` のコミット `17e64839eb1e30632eef4a0147862345fccb61cc` の `codex-rs/login/src/device_code_auth.rs` と `codex-rs/login/src/auth/manager.rs` を認証仕様の根拠とする。デバイス認証はブラウザでの初回承認が必要で、更新トークンはESP32自身で更新・保存する。PC側のCodexと更新トークンを共有して同時更新しない。
- 使用量は公式クライアントが参照する `https://chatgpt.com/backend-api/wham/usage` の読み取り専用GETを使う。公開安定APIという保証はない。`primary_window` を5時間と決めつけず `limit_window_seconds` で分類する。実アカウントでは通常枠のprimaryが604800秒、secondaryがnullだった。別モデルの `additional_rate_limits` を通常枠の欠損補完に流用しない。
- 実機の元ファームウェアは設定を含むフラッシュ全体を退避し、`verify-flash` で一致を確認してから試験する。試験終了時は全体を書き戻す。バックアップ・認証情報・検証ログは `.local/` 内だけに置き、Git管理しない。eFuseやフラッシュ暗号化設定は変更しない。
- WindowsでPlatformIOを使う際はこのリポジトリの `core_dir = .local/platformio` を使う。日本語Windowsの出力エンコードエラーを防ぐため `PYTHONUTF8=1` を設定する。詳細な構成と復旧手順は `docs/design.md` と `how-to-update.md` を参照する。
- Arduino 2.0.17を含むPlatformIO旧構成にはWebServerの既知脆弱性修正がないため採用しない。`platformio.ini` は修正済みArduino 3.3.11を含むpioarduino 55.03.311を固定している。更新時はEspressif公開アドバイザリと実際に導入されたソースを照合する。
- 通常配布は `cyd` のみ。`cyd-diagnostics` は画面読み戻し・入力試験用のUSB診断経路を含むため配布しない。診断後は元フラッシュを復元する。Windowsの新コンパイラが `Failed to get path name. Error code: 5` で失敗する場合は、ソース不良と混同せず昇格した同一ビルドで確認する。
- NVSの設定はversion 2のJSON blobで、version 1は反転なしとして自動移行する。認証もblobを使い、NVS文字列の4000バイト制限を避ける。LCDの基準はpanel offset_rotation=1、通常rotation=0、上下反転rotation=2。タッチを別途二重反転しない。
- Windowsの検証APでは再起動後にWi-Fi関連付けだけ成功し、DHCPのSELECTINGが続く挙動を実測した。`src/dhcp-broadcast.cpp` でIP未取得時の応答をRFC 2131 §4.1のブロードキャストで要求すると再接続した。`--wrap=dhcp_append_extra_opts` はこの処理に必要。SDK更新時はフックの宣言と実機再接続を再検証し、固定IP・MAC変更で代替しない。
- 描画は320×240の8-bit SpriteをWi-Fi開始前に確保し、完成フレームから変化した領域だけをLCDへ転送する。実LCDへ直接全消去→各部描画を繰り返すと操作のたびにちらつくため、描画先の変更時はこの経路を維持する。
- 2026-09-14、描画と入力はnotifications式のTFT_eSPI、16px Unifont字形、別VSPIの `sensitive-xpt2046` に移行した。8-bit Sprite上で完成フレームを描く方式は維持する。BOOT短押しは回転、1.5秒以上の長押しは2点の位置・押圧感度調整。調整値は既存認証・画面設定blobとは別のNVS `usage-touch` の単一`calib` blobに保存し、失敗時は旧値を残す（`src/notification-display.cpp`）。ビルドとホストテストのみ確認済みで、この入力・描画経路の実機確認は未実施。
- 2026-09-14、再描画は8-bit Spriteの16行帯を比較し、変化した連続帯だけLCDへ転送する（`include/display-diff.h`、`src/main.cpp`）。回転・消灯復帰・タッチ校正では全帯を再転送する。現在の差分転送はホスト/ビルド検証のみで、実機の表示欠け・ちらつきは未確認。
- Linux CIのChromeは親終了後も子プロセスが一時プロファイルへ書き込む場合がある。`scripts/test-setup-ui.py` は専用セッションで起動し、プロセスグループを終了してから期限付きで削除する。Windows専用GPU起動オプションをLinuxへ適用するとSIGTRAPで起動できなかったため、OS分岐を維持する。
- 2026-09-10: 空の認証NVSから公式ページでユーザーがコードを承認し、ESP32自身の交換・保存・使用量取得を確認した。診断版の `refresh-token` はRAMの期限のみ0にして通常更新経路を実行する。実更新3回（再起動後の保存済み更新トークン使用を含む）が成功。初回に一度だけ応答形式エラーがあり原因未確定のため、再現時は秘密情報を出さない `HTTP_PARSE` 診断を確認する。詳細は `verification.md`。
- 2026-09-16、新規ESP32-2432S028R（ESP32-D0WD-V3、MAC 68:09:47:85:a1:3c）の自動リセット回路ではダウンロードモードに入らず、BOOT保持＋RST後に `esptool --before no-reset --after no-reset` で退避・書き込み・照合を行った（`scripts/flash-backup.py` の既定リセットでは接続できない）。元フラッシュは `.local/cyd-original-firmware.bin`（4,194,304バイト、SHA-256 bca8112f22d641d689927217eb223450685ab0b5e022d983ef202d7c8bfdb2b5）へ退避し `verify-flash` で一致を確認した。修正版の再書き込みはNVS校正値を残すためアプリ領域（0x10000）のみ更新した。
- 2026-09-16、初期設定ページ約36KBを `String` へ複写して `server.send` すると、表示フレームバッファ確保後の実機で白紙になった（`/api/state` は応答あり）。`scripts/embed-web.py` をnonce位置でのPROGMEM前半・後半分割出力に変え、`src/network.cpp` で `setContentLength` 後に `sendContent_P`／`sendContent` で3片を順送する。分割再構成の回帰テストは `test/test_embed_web.py` に置く。
- 2026-09-16、新規個体は初期状態で軽いタップに反応しなかったが、BOOT長押し2点調整で校正が完了し閾値・位置とも良好になったため既定値は変更していない。設定ページ到達前の軽操作不能は仕様上の既知の導入手順（how-to-update.mdのBOOT長押し）で解消する。
- 2026-09-17、リセット日時は `wham/usage` の `primary_window`／`secondary_window` の `reset_at`（秒単位UNIX時刻、UTC）を使う。公式 `openai/codex` の `backend-client/src/client.rs` の `map_rate_limit_window` が `reset_at` を `resetsAt` へ写像すること、CDP実測で未認証GETが `{"detail":"Unauthorized"}` になること、公式料金案内が上限とリセット時刻を使用量ダッシュボード参照としていることを根拠とする。`reset_after_seconds` は表示に使わない。使用量タブはJST換算のリセット日時と残り時間（基本「あとX日Y時間」、24時間未満は「あとX時間Y分」）を `include/reset-format.h` で整形し、分の変わり目で再描画する。新規日本語文言は `scripts/generate-font.py` の再生成で字形へ取り込む。自動取得は5分間隔（失敗時は最大15分まで延長、手動更新・Wi-Fi再接続時は即時）。
- 2026-09-17、v0.3.0を運用個体（MAC 68:09:47:85:a1:3c）へアプリ領域（0x10000）のみ書き込み、NVSの認証・校正値を維持した。事前退避 `.local/pre-v0.3.0-firmware.bin` と書き込み後の照合はいずれも `verify-flash` で一致を確認した。この個体はesptool終了のたびにダウンロードモードから抜けるため、接続ごとにBOOT保持＋RSTで再投入する。esptool v5は複数コマンドの単一実行を受け付けないため、読み出し・照合・書き込みは1接続1コマンドで行う。
- 2026-09-26、ブラウザ承認後にLCDの促しが残って見えた報告を受け、v0.4.0の2タブ化が認証済み再ログインの新規コードをLCDから隠すことを確認した。接続タブは未認証時またはコード待ちに表示し、コード待ちの促しに使用量取得の残り秒数を付けない（`include/ui-tabs.h`、`src/main.cpp`、回帰 `test/test_ui_tabs`）。承認後の遷移はポーリング間隔（最大60秒）で自動のため、促しが残っていてもすぐUSBを抜かず1分程度待つ。
- COM7のCH340が運用個体、COM5の native USB（VID_303A:PID_1001）は別個体（MAC 7C:E8:B1:D6:6B:4C）である。対象の取り違えに注意する。
- 2026-09-26、v0.4.1を運用個体へアプリ領域（0x10000）のみ書き込み、NVSの認証・校正値を維持した。事前退避 `.local/pre-v0.4.1-firmware.bin`（4,194,304バイト、SHA-256 aba45be5…626f774）と書き込み後の照合はいずれも `verify-flash` で一致を確認した。公開 `firmware.bin`（SHA-256 8e7bfcd5…54c4ce98026）を照合してから使用した。

移動元: `AGENTS.md`。

<a id="readme-1"></a>

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

移動元: `README.md`。

<a id="readme-2"></a>

## 実装と依存関係

本番環境は、pioarduinoのPlatformIO platform 55.03.311（Arduino core 3.3.11、ESP-IDF 5.5.5）を使うArduino ESP32です。旧Arduino core 2.0.17はWebServerの既知の修正（GHSA-8cmm-3887-r32j、GHSA-5476-9jjq-563m）が不足するため使用しません。

- `bodmer/TFT_eSPI@2.5.43`
- `bblanchon/ArduinoJson@7.4.3`
- ネイティブテスト: `throwtheswitch/Unity@2.6.1`
- 開発ツール: `platformio==6.1.19`、`esptool==5.4.0`

PlatformIOのキャッシュは `platformio.ini` の `core_dir = .local/platformio` に分離しています。開発ツールはグローバルPythonへ入れず、プロジェクト外または専用venvで管理してください。

画面とタッチ入力は [include/notification-display.h](include/notification-display.h)、表示状態は [include/display-state.h](include/display-state.h)、ネットワーク統合は [src/network.cpp](src/network.cpp)、認証通信は [src/codex-client.cpp](src/codex-client.cpp)、Web設定画面は [web/setup.html](web/setup.html) にあります。

移動元: `README.md`。

## 根拠と検証記録

- [README.md](README.md): 利用者向けの導入と操作。
- [verification.md](verification.md): 検証手順、実測結果、未確認事項。
- [how-to-update.md](how-to-update.md): 更新・復旧の実手順。

### 依存ツールの確認時点

上記READMEから移した開発ツールの版は旧検証時点の記録です。2026-10-16の同期後の `requirements-dev.txt` は `platformio==6.2.0`、`esptool==5.5.0`、`starlette==1.7.0` を指定しています。導入・更新にはこの設定を正とし、旧記録の固定値を再導入しないでください。
