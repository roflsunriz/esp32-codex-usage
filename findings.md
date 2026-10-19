# 調査知見

- 初回device authはブラウザー承認が必要。refresh tokenはESP32自身で更新・保存し、PC版Codexと共有して競合更新しない。
- 使用量は非公開安定契約の `https://chatgpt.com/backend-api/wham/usage` を読む。5時間枠を固定せず `limit_window_seconds` で分類し、`additional_rate_limits` を通常枠の欠損補完に使わない。リセット表示は `reset_at` を使い、`reset_after_seconds` は使わない。
- 実機試験前に設定を含む全flashを退避・照合し、秘密とlogは `.local/` だけに置く。eFuse/flash暗号化を変えない。通常更新はNVSを残すためapp領域 `0x10000` だけを書き込む。
- 対象CYD個体は自動resetでdownload modeへ入らない場合がある。BOOT保持+RSTと `esptool --before no-reset --after no-reset` を使い、esptool v5は1接続1commandとする。COM7のCH340運用個体とCOM5のnative USB別個体を取り違えない。
- NVS設定はversioned JSON blobで保存し、4000-byte文字列上限を避ける。rotationはpanel offsetと画面rotationで表し、touchを二重反転しない。touch校正は別NVS blobへ原子的に保存し、失敗時は旧値を残す。
- Windows APでDHCP SELECTINGが続く場合、IP未取得応答へRFC 2131 broadcast要求が必要。`--wrap=dhcp_append_extra_opts` を外さず、SDK更新時に実機再接続を確認する。固定IP/MAC変更で代替しない。
- 描画はWi-Fi開始前に320×240 8-bit Spriteを確保し、完成frameの変化した16行帯だけ転送する。rotation・wake・calibration時は全帯を再送する。直接全消去して部品ごと描くとちらつく。
- 約36KBの設定HTMLを `String` へ複写するとframebuffer確保後に白紙化した。PROGMEM前後+nonceの3片を `setContentLength` 後に逐次送信する。
- 認証承認後の画面遷移はpollingで最大60秒かかる。促しが残ってもすぐ電源を抜かない。未認証/コード待ちだけ接続tabを出す。
- 通常配布は `cyd` のみ。USB診断機能を持つ `cyd-diagnostics` は配布せず、診断後に元flashへ戻す。PlatformIOはrepo内 `.local/platformio` と `PYTHONUTF8=1` を使う。
- Arduino 2.0.17系はWebServer修正不足のため再導入しない。依存versionはREADMEの過去記録でなく現在の `requirements-dev.txt` と `platformio.ini` を正本にする。
- LinuxのChrome UI testはprocess groupを終了してから一時profileを期限付き削除する。Windows専用GPU optionをLinuxへ渡すとSIGTRAPになるためOS分岐を維持する。
