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
- LCDディスプレイは15s, 30s, 1m, 2m, 5m, 10m, 30m, 1h, 2hから設定を選べるようにして設定時間で自動消灯するにする
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
- 描画は320×240の8-bit SpriteをWi-Fi開始前に確保し、完成フレームを一度だけLCDへ転送する。実LCDへ直接全消去→各部描画を繰り返すと操作のたびにちらつくため、描画先の変更時はこの経路を維持する。
- Linux CIのChromeは親終了後も子プロセスが一時プロファイルへ書き込む場合がある。`scripts/test-setup-ui.py` は専用セッションで起動し、プロセスグループを終了してから期限付きで削除する。Windows専用GPU起動オプションをLinuxへ適用するとSIGTRAPで起動できなかったため、OS分岐を維持する。
