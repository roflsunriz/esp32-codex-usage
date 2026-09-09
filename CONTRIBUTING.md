# 貢献ガイド

## 作業前

リポジトリ直下の `COMMON-AGENTS.md` と `AGENTS.md` を読み、`git status --short --branch` で既存変更を確認します。ユーザーや別作業者の差分を戻しません。外部仕様、API、ライブラリ、ボードピンは一次資料で確認します。

ESP32へ書き込む変更では、先にフルフラッシュを退避し、SHA-256を記録します。認証トークン、Cookie、Wi-Fiパスワード、NVSダンプ、フルファームウェアをGitへ追加しません。

## 開発環境

```powershell
$env:PYTHONUTF8 = "1"
python scripts/test-native.py
pio run -e cyd
```

PlatformIOのキャッシュは `.local/platformio` に置かれます。通常のファームウェアは `cyd`、`cyd-diagnostics` は合成入力と画面状態取得を行う診断専用プロファイルです。診断プロファイルに含まれるUSBシリアル操作をリリース手順で使いません。証明書を更新する場合は `python scripts/update-ca.py` を実行し、生成された `include/trusted-roots.h` の差分を確認します。Web設定画面を変更した場合は `python scripts/embed-web.py` と生成ヘッダーの差分確認を行います。

PlatformIO platformはpioarduino 55.03.311、Arduino coreは3.3.11、ESP-IDFは5.5.5です。旧Arduino core 2.0.17に残るWebServerのGHSA-8cmm-3887-r32jとGHSA-5476-9jjq-563mを再導入しないでください。開発ツールはグローバルPythonへ入れず、専用venvで `requirements-dev.txt` を使います。

## 実装方針

- UI状態、表示ドライバ、認証通信、NVS永続化、JSON検証を分離します。
- 認証や使用量APIの失敗を空データとして隠しません。
- タイムアウト、タッチ復帰、JWT期限、HTTPエラー、壊れたNVSをテストします。
- ユーザー向け文言は日本語と英語を考慮し、認証情報を画面やログへ出さない設計にします。
- refresh tokenはNVSのJSON blobで保存し、文字列エントリのサイズ制限を再導入しません。
- ライブラリのバージョンと採用理由を `platformio.ini` と文書で追跡します。

## Pull Request

本文には、目的、変更範囲、検証コマンド、実機検証の有無、未検証の制約、復旧方法を記載します。UI変更では画面サイズ320×240、3タブ、9つの消灯設定、消灯後の最初のタッチ、BOOT単押しによる180度回転、設定schema移行を確認します。認証変更ではトークン値、Cookie、個人情報を添付しません。リリースタグを追加する場合は、CHANGELOGに同じバージョン見出しを先に追加します。ブラウザ認証がCloudflareチャレンジで止まった場合や実機検証が未実施の場合は、成功と記載しません。

コミットメッセージは日本語Conventional Commits形式（例: `feat: 使用量タブを追加`）にします。ブランチ作成や公開操作は依頼範囲を確認してから行います。
