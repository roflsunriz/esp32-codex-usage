# Third-party notices

このファイルは、現在のPlatformIO依存と同梱フォントについて、導入時に確認できるライセンスと著作権表示の所在を記録します。ファームウェア全体を単一のライセンスで再配布できることを意味しません。各依存の原文ライセンスを同梱物の該当パスで確認してください。

| コンポーネント | 用途 | ライセンス・表示 | 確認した場所 / 公式情報 |
|---|---|---|---|
| LovyanGFX 1.2.28 | ILI9341表示、XPT2046タッチ、フォント基盤 | FreeBSD License。配布元のlicense.txtにはAdafruit ILI9341（MIT）、Adafruit GFX（BSD）、TFT_eSPI（FreeBSD）の原 notices も含まれる | `.pio/libdeps/cyd/LovyanGFX/license.txt` / https://github.com/lovyan03/LovyanGFX |
| IPAフォント（IPAexGothic等） | LCDの日本語表示 | IPA Font License Agreement v1.0 | `.pio/libdeps/cyd/LovyanGFX/src/lgfx/Fonts/IPA/IPA_Font_License_Agreement_v1.0.txt` / https://moji.or.jp/ipafont/ |
| ArduinoJson 7.4.3 | 使用量・設定JSON | MIT License（Benoit Blanchon） | `.pio/libdeps/cyd/ArduinoJson/LICENSE.txt` / https://github.com/bblanchon/ArduinoJson |
| Arduino ESP32 core 3.3.11 | ESP32 Arduinoフレームワーク | コンポーネントごとにLGPL-2.1-or-later、Apache-2.0等の表示がある混合物。個別ファイルのSPDX・原文を優先する | `.local/platformio/packages/framework-arduinoespressif32` / https://github.com/espressif/arduino-esp32 |
| pioarduino platform 55.03.311 | PlatformIOのESP32 platform定義 | Apache-2.0（インストール済み `platform.json` の表示） | `.local/platformio/platforms/espressif32/platform.json` / https://github.com/pioarduino/platform-espressif32 |
| Unity 2.6.1 | ネイティブテスト専用 | MIT License | `.pio/libdeps/native/Unity/LICENSE.txt` / https://github.com/ThrowTheSwitch/Unity |

PlatformIO、pioarduino、esptoolなど開発時に取得するツールは、生成したファームウェアへソースとして同梱するコンポーネントとは別に扱います。依存を更新したら、インストール先のライセンスファイルとこの一覧を照合してください。
