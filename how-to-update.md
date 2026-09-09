# 更新・復旧手順

この文書は、ESP32のフルフラッシュを退避してからproductionファームウェアを検証し、終了後に元の内容を安全に戻すための手順です。リリース用GitHub Actionsはビルドと成果物公開だけを行い、USBポートへ書き込みません。

## 前提

- Windows PowerShell。
- `requirements-dev.txt` のPlatformIOとesptoolを専用venvへ導入する。
- グローバルPythonへ依存をインストールしない。
- USBシリアルモニターを閉じ、対象ESP32のポート名を実測する。
- バックアップ・認証情報・検証ログはGit管理外の `.local/` に置き、共有しない。

PlatformIOは `platformio.ini` の pioarduino platform 55.03.311（Arduino core 3.3.11、ESP-IDF 5.5.5）を使用します。依存キャッシュは `.local/platformio` に分離されます。

初回の開発環境はリポジトリ直下で作成します。

```powershell
python -m venv .local/venv
.\.local\venv\Scripts\Activate.ps1
python -m pip install -r requirements-dev.txt
pio pkg install -e native
```

Activate.ps1が利用できない環境では、`.local\venv\Scripts\python.exe` と `.local\venv\Scripts\pio.exe` をフルパスで実行できます。WindowsのネイティブテストにはLLVMとVisual Studio C++ Build Tools（Windows SDKを含む）、またはGCCが必要です。ブラウザテストにはNode.js 22以降とGoogle Chromeを使います。

## 1. 全フラッシュをバックアップ

専用スクリプトは既存ファイルを上書きせず、読み出し後に実機との `verify-flash` を実行し、同じ場所へSHA-256とサイズのJSONメタデータを保存します。

```powershell
$env:PYTHONUTF8 = "1"
python scripts/flash-backup.py backup `
  --port COMx `
  --file .local\cyd-original-firmware.bin
```

`cyd-original-firmware.bin.json` が作成されること、4 MB個体ではサイズが4,194,304 bytesになること、esptoolの照合が成功することを確認します。フルイメージにはNVS、Wi-Fi設定、認証情報が含まれる可能性があるため、ログやGitへ内容を出力しません。

## 2. 証明書・Webページ・テスト・productionビルド

```powershell
$env:PYTHONUTF8 = "1"
python scripts/update-ca.py
python scripts/embed-web.py
python scripts/test-native.py
pio run -e cyd
```

`scripts/embed-web.py` は `web/setup.html` から `include/setup-page.h` を生成します。生成後に差分を確認してください。`python scripts/test-native.py` はWindowsでLLVM/MSVCツールチェーンを使います。最新の確認結果は [verification.md](verification.md) を参照してください。`cyd-diagnostics` はUSB診断用で、productionやリリース成果物には使いません。

リリースの `firmware.factory.bin` は初回導入用の結合イメージで、0x0から書き込むとNVS設定領域も上書きします。通常の更新には、上記のPlatformIO uploadを使ってください。`firmware.bin` はアプリ単体で、同じパーティション構成と互換性のあるブートローダーが必要です。

証明書生成、Webページ生成、テスト、ビルドのいずれかが失敗した場合は、実機へ書き込みません。

## 3. 実機へproductionファームウェアを書き込む

バックアップとproductionビルドが成功した後だけ、実測したポートへ書き込みます。

```powershell
pio run -e cyd -t upload --upload-port COMx
pio device monitor -p COMx -b 115200
```

LCDは使用量タブから始まります。接続タブでESP32 APのSSIDとパスワードを確認し、スマートフォンをESP32 APへ接続して `192.168.4.1` を開きます。2.4 GHz Wi-Fiを保存した後、ログインを押してコードを控え、スマートフォンをインターネット回線へ切り替えて公式認証ページで承認します。ESP32は承認を自律的にポーリングします。

起動後のBOOTボタン単押しは表示の180度回転を切り替え、設定はNVSへ保存されます。起動中に押し続けるとESP32の書き込みモードへ入るため、更新前にBOOTを押したまま電源を入れないでください。

## 4. 元ファームウェアへ復元

検証の成功・失敗に関係なく、終了時は元イメージを復元します。メタデータとSHA-256が一致しない場合、スクリプトは書き込みを中止します。

```powershell
$env:PYTHONUTF8 = "1"
python scripts/flash-backup.py restore `
  --port COMx `
  --file .local\cyd-original-firmware.bin
```

出力に書き戻し後の `verify-flash` 成功があることを確認し、再起動後に元の画面とシリアル起動を確認します。事前に `erase-all` を実行せず、`--force` も使いません。Secure Boot、Flash Encryption、Secure Download Modeのエラーが出た場合は停止し、eFuseを変更しません。

## 5. ロールバック時の扱い

書き込み途中で接続が切れた場合は、USBシリアルモニターを閉じ、同じポートと検証済みバックアップで復元を一度だけ再試行します。復元後も起動しない場合は、エラー全文、バックアップメタデータ、esptoolバージョンだけを保全し、フラッシュ内容や認証情報を共有しません。
