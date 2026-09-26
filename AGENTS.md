# AGENTS.md

## 作業開始前の必須手順（最優先・例外なし）

1. エージェントは、調査、計画、コマンド実行、スキル利用、ファイル編集、コミット、プッシュを始める前に、必ずリポジトリ直下の `.\COMMON-AGENTS.md` を開き、先頭から末尾まで全文を読む。
2. `COMMON-AGENTS.md` はGit管理外のシンボリックリンクである。`git`や既定のignore設定が有効な`rg --files`の検索結果だけで、ファイルが存在しないと判断してはならない。PowerShellでは最初に次を実行する。

```powershell
Get-Content -Raw -LiteralPath .\COMMON-AGENTS.md
```

3. 読み取りに失敗した場合、出力が省略された場合、または末尾まで読めたことを確認できない場合は、一切の作業を開始せず、パスとシンボリックリンク先を確認して全文を再取得する。必要なら分割して末尾まで読む。
4. 全文を読了するまで、ローカル `AGENTS.md` だけを根拠に作業を続けてはならない。読了後は `COMMON-AGENTS.md` を最優先の指針とし、読了直後の最初の進捗報告で全文を読了したことを明示する。
   このファイルでは `esp32-codex-usage` 固有の補足だけを記載する。

## 目的
- esp32-2432s028r ili9341 esp-wroom-32 tft lcd 開発ボード上のLCDモニタにCodexの5時間制限プログレスバーとパーセンテージ、週間制限プログレスバーとパーセンテージを表示する
- OpenAIの公式ドキュメントや既存Github実装などを調査して実装する
- 見つからなければOpenAIのCodex ページにログインし、公式資産をde-minify、またはリクエストをキャプチャするなどして解析する
- Codex Desktop App自体の実装も参考に使えるかも知れない
- ログイン、認証保持を自前実装する
- ESP32側で認証を保持するようにする
- 初期設定以降のPCまたはスマホの常時接続を不要にする
- USBは給電のみにし、Wi-Fiで定期更新する仕組みにする
- LCDディスプレイは0〜59分のスライダーと0〜24時間のスライダーで設定した時間で自動消灯するにする。0分0時間は自動消灯オフ（常時点灯）と同じ。取得間隔は60〜600秒（60秒刻み）のスライダーで変えられるようにする。設定タブはスクロールとドラッグ操作に対応する
- 消灯中も自動更新は続け、画面タッチで消灯から復帰する仕様にする（消灯後は最初のタッチのみを復帰のみに使い、操作と混同しない）
- ESP側にタブを作りESP側で随時消灯時間の切り替えを可能にする
- BOOTボタンのクリックで画面を180度反転できるようにする。反転時はタッチ座標も追従させる。

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
