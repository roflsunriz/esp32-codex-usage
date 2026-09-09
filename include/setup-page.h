#pragma once
#include <Arduino.h>

static const char kSetupPage[] PROGMEM = R"ESP32SP_78a0e317(
<!doctype html>
<html lang="ja">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="color-scheme" content="light">
  <title>Codex Usage Monitor — 初期設定</title>
  <style>:root{color-scheme: light; --canvas: #eef5f6; --surface: #ffffff; --surface-soft: #f6fafb; --ink: #11242d; --muted: #5b6e76; --line: #d6e2e5; --accent: #0f7185; --accent-strong: #07566a; --accent-soft: #dff3f5; --success: #166534; --success-soft: #e8f6ed; --danger: #a31846; --danger-soft: #fff0f3; --warning: #855d08; --warning-soft: #fff8df; --shadow: 0 18px 45px rgba(28, 64, 76, 0.1); font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; font-synthesis: none;}
*, *::before, *::after{box-sizing: border-box;}
html{min-width: 320px; background: var(--canvas);}
body{min-width: 320px; min-height: 100vh; margin: 0; color: var(--ink); background: radial-gradient(circle at 8% 0%, rgba(198, 234, 237, 0.75), transparent 30rem), var(--canvas); line-height: 1.55;}
a{color: var(--accent-strong);}
button, input, select{font: inherit;}
button{min-height: 44px; border: 0; cursor: pointer;}
button:disabled{cursor: wait; opacity: 0.62;}
:focus-visible{outline: 3px solid rgba(13, 116, 133, 0.35); outline-offset: 3px;}
.site-header{display: flex; align-items: center; justify-content: space-between; gap: 1rem; width: min(calc(100% - 2rem), 1100px); margin: 0 auto; padding: 1.25rem 0;}
.brand{display: inline-flex; align-items: center; gap: 0.65rem; color: var(--ink); font-size: 0.94rem; font-weight: 700; letter-spacing: 0.01em; text-decoration: none;}
.brand-mark{display: inline-grid; width: 2rem; height: 2rem; place-items: center; border: 1px solid #9ecfd4; border-radius: 0.75rem; color: var(--accent-strong); background: var(--accent-soft); font-size: 1.1rem; line-height: 1;}
.language-toggle{padding: 0.5rem 0.78rem; border: 1px solid var(--line); border-radius: 999px; color: var(--accent-strong); background: rgba(255, 255, 255, 0.8); font-size: 0.86rem; font-weight: 700;}
.language-toggle:hover{border-color: #9ecfd4; background: var(--surface);}
.shell{width: min(calc(100% - 2rem), 1100px); margin: 0 auto; padding: 1.75rem 0 4rem;}
.intro{max-width: 760px; margin-bottom: 1.8rem;}
.eyebrow{margin: 0 0 0.55rem; color: var(--accent-strong); font-size: 0.78rem; font-weight: 800; letter-spacing: 0.12em; text-transform: uppercase;}
h1, h2, h3, p{margin-top: 0;}
h1{max-width: 720px; margin-bottom: 0.9rem; font-size: clamp(1.9rem, 4vw, 3.35rem); line-height: 1.08; letter-spacing: -0.04em;}
.lead{max-width: 680px; margin-bottom: 0; color: var(--muted); font-size: clamp(1rem, 2vw, 1.12rem);}
.card, .state-panel{border: 1px solid rgba(192, 214, 218, 0.9); border-radius: 1.25rem; background: rgba(255, 255, 255, 0.94); box-shadow: var(--shadow);}
.state-panel{margin-bottom: 1rem; padding: clamp(1rem, 3vw, 1.45rem);}
.section-heading, .card-heading{display: flex; align-items: flex-start; justify-content: space-between; gap: 1rem;}
.section-heading{margin-bottom: 1rem;}
.section-heading h2, .card-heading h2{margin-bottom: 0.25rem; font-size: 1.15rem; line-height: 1.25;}
.section-heading p, .card-heading p{margin-bottom: 0; color: var(--muted); font-size: 0.9rem;}
.state-grid{display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 0.7rem;}
.state-item{min-width: 0; padding: 0.85rem 0.95rem; border: 1px solid var(--line); border-radius: 0.85rem; background: var(--surface-soft);}
.state-label{display: block; margin-bottom: 0.25rem; color: var(--muted); font-size: 0.78rem; font-weight: 700;}
.state-value{display: block; overflow-wrap: anywhere; font-size: 0.96rem; font-weight: 800;}
.state-value[data-state="yes"]{color: var(--success);}
.state-value[data-state="no"]{color: var(--danger);}
.state-value[data-state="unknown"]{color: var(--warning);}
.server-status{min-height: 1.55em; margin: 0.95rem 0 0; color: var(--muted); font-size: 0.9rem;}
.message{margin: 0 0 1rem; padding: 0.78rem 0.95rem; border: 1px solid var(--line); border-radius: 0.8rem; color: var(--muted); background: var(--surface); font-size: 0.9rem;}
.message[data-tone="success"]{border-color: #b6dfc0; color: var(--success); background: var(--success-soft);}
.message[data-tone="error"]{border-color: #f0b9c8; color: var(--danger); background: var(--danger-soft);}
.message[data-tone="warning"]{border-color: #ecd88e; color: var(--warning); background: var(--warning-soft);}
.card-grid{display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 1rem;}
.card{min-width: 0; padding: clamp(1rem, 3vw, 1.45rem);}
.card-heading{margin-bottom: 1.15rem;}
.step{flex: 0 0 auto; min-width: 2.2rem; padding: 0.3rem 0.45rem; border-radius: 0.55rem; color: var(--accent-strong); background: var(--accent-soft); font-size: 0.74rem; font-weight: 800; text-align: center;}
.form-stack{display: grid; gap: 1rem;}
.field{display: grid; gap: 0.4rem;}
.field label, .field-label{font-size: 0.9rem; font-weight: 750;}
input, select{width: 100%; min-height: 46px; padding: 0.66rem 0.75rem; border: 1px solid #bccfd3; border-radius: 0.7rem; color: var(--ink); background: var(--surface);}
input::placeholder{color: #7b8d93;}
input:disabled, select:disabled{color: var(--muted); background: #edf3f4;}
.helper{margin: -0.5rem 0 0; color: var(--muted); font-size: 0.82rem;}
.button-row{display: flex; flex-wrap: wrap; align-items: center; gap: 0.7rem;}
.button{display: inline-flex; align-items: center; justify-content: center; min-width: 9rem; padding: 0.65rem 1rem; border-radius: 0.7rem; font-weight: 800; text-align: center;}
.button-primary{color: #fff; background: var(--accent);}
.button-primary:hover:not(:disabled){background: var(--accent-strong);}
.button-secondary{border: 1px solid var(--line); color: var(--accent-strong); background: var(--surface);}
.button-secondary:hover:not(:disabled){border-color: #9ecfd4; background: var(--accent-soft);}
.button-link{display: inline-flex; align-items: center; min-height: 44px; padding: 0.55rem 0; font-weight: 750;}
.device-code-box{display: grid; gap: 0.45rem; margin: 1rem 0; padding: 0.85rem; border: 1px dashed #a9cbd0; border-radius: 0.85rem; background: #f1fafb;}
.device-code-label{color: var(--muted); font-size: 0.78rem; font-weight: 800;}
.device-code{min-width: 0; overflow-wrap: anywhere; color: var(--accent-strong); font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: clamp(1.1rem, 3vw, 1.45rem); font-weight: 800; letter-spacing: 0.11em; user-select: all;}
.device-code[data-empty="true"]{color: var(--muted); font-family: inherit; font-size: 0.88rem; font-weight: 650; letter-spacing: normal;}
.security-note{margin: 1rem 0 0; padding-top: 0.9rem; border-top: 1px solid var(--line); color: var(--muted); font-size: 0.82rem;}
.timeout-card{grid-column: 1 / -1;}
.timeout-layout{display: grid; grid-template-columns: minmax(0, 1fr) auto; align-items: end; gap: 1rem;}
.timeout-layout .button{min-width: 12rem;}
[hidden]{display: none !important;}
@media (max-width: 760px){.card-grid{grid-template-columns: 1fr;}
.timeout-card{grid-column: auto;}
}
@media (max-width: 560px){.site-header, .shell{width: min(calc(100% - 1rem), 1100px);}
.site-header{padding-top: 0.8rem;}
.shell{padding-top: 1.25rem;}
.state-grid{grid-template-columns: 1fr;}
.timeout-layout{grid-template-columns: 1fr; align-items: stretch;}
.timeout-layout .button{width: 100%;}
}
@media (max-width: 380px){.brand{font-size: 0.85rem;}
h1{font-size: 1.8rem;}
.button-row, .button-row .button, .button-row .button-link{width: 100%;}
}
@media (prefers-reduced-motion: reduce){*, *::before, *::after{scroll-behavior: auto !important; transition-duration: 0.001ms !important; animation-duration: 0.001ms !important;}
}
</style>
</head>
<body>
  <header class="site-header">
    <a class="brand" href="/" data-i18n-aria-label="brandAriaLabel">
      <span class="brand-mark" aria-hidden="true">◒</span>
      <span data-i18n="appName">Codex Usage Monitor</span>
    </a>
    <button id="language-toggle" class="language-toggle" type="button" data-i18n="languageButton" data-i18n-aria-label="languageSwitchAria">English</button>
  </header>

  <main class="shell">
    <section class="intro" aria-labelledby="page-title">
      <p class="eyebrow" data-i18n="eyebrow">Initial setup</p>
      <h1 id="page-title" data-i18n="title">Codex 使用量を表示する準備</h1>
      <p class="lead" data-i18n="subtitle">このページで Wi‑Fi とログインを一度設定すると、以後は PC やスマートフォンを常時接続しなくても、ESP32 が定期的に使用量を更新します。</p>
    </section>

    <section class="state-panel" aria-labelledby="state-heading">
      <div class="section-heading">
        <div>
          <h2 id="state-heading" data-i18n="stateHeading">現在の状態</h2>
          <p data-i18n="stateDescription">設定後もこの画面で接続と認証の進み具合を確認できます。</p>
        </div>
      </div>
      <div class="state-grid">
        <div class="state-item">
          <span class="state-label" data-i18n="wifiStateLabel">Wi‑Fi</span>
          <strong id="wifi-state" class="state-value" data-state="unknown">確認中…</strong>
        </div>
        <div class="state-item">
          <span class="state-label" data-i18n="loginStateLabel">Codex ログイン</span>
          <strong id="login-state" class="state-value" data-state="unknown">確認中…</strong>
        </div>
        <div class="state-item">
          <span class="state-label" data-i18n="deviceStateLabel">デバイス</span>
          <strong id="device-state" class="state-value" data-state="unknown">確認中…</strong>
        </div>
      </div>
      <p id="server-status" class="server-status" role="status" aria-live="polite">状態を取得しています。</p>
    </section>

    <p id="global-message" class="message" role="status" aria-live="polite" hidden></p>

    <div class="card-grid">
      <section class="card" aria-labelledby="wifi-heading">
        <div class="card-heading">
          <div>
            <h2 id="wifi-heading" data-i18n="wifiHeading">Wi‑Fi を設定</h2>
            <p data-i18n="wifiIntro">2.4GHz 帯の Wi‑Fi に接続します。</p>
          </div>
          <span class="step" aria-hidden="true">01</span>
        </div>

        <form id="wifi-form" class="form-stack" novalidate>
          <div class="field">
            <label for="ssid" data-i18n="ssidLabel">Wi‑Fi 名（SSID）</label>
            <input id="ssid" name="ssid" type="text" maxlength="32" autocomplete="off" required data-i18n-placeholder="ssidPlaceholder" placeholder="例: MyHome-2G">
          </div>
          <div class="field">
            <label for="password" data-i18n="passwordLabel">Wi‑Fi パスワード</label>
            <input id="password" name="password" type="password" maxlength="64" autocomplete="off" data-i18n-placeholder="passwordPlaceholder" placeholder="オープンネットワークなら空欄">
          </div>
          <p class="helper" data-i18n="passwordHelp">入力内容はこの送信だけに使い、ブラウザーには保存しません。</p>
          <div class="button-row">
            <button id="wifi-submit" class="button button-primary" type="submit" data-i18n="saveWifi">Wi‑Fi を保存</button>
          </div>
          <p id="wifi-message" class="message" role="status" aria-live="polite" hidden></p>
        </form>
      </section>

      <section class="card" aria-labelledby="login-heading">
        <div class="card-heading">
          <div>
            <h2 id="login-heading" data-i18n="loginHeading">Codex にログイン</h2>
            <p data-i18n="loginIntro">ログインを開始するとデバイスコードが表示されます。</p>
          </div>
          <span class="step" aria-hidden="true">02</span>
        </div>

        <div class="device-code-box">
          <span class="device-code-label" data-i18n="deviceCodeLabel">デバイスコード</span>
          <code id="device-code" class="device-code">—</code>
          <button id="copy-code" class="button button-secondary" type="button" data-i18n="copyCode" disabled>コードをコピー</button>
        </div>

        <div class="button-row">
          <button id="login-submit" class="button button-primary" type="button" data-i18n="startLogin">ログインを開始</button>
          <a class="button-link" href="https://auth.openai.com/codex/device" target="_blank" rel="noopener noreferrer" data-i18n="authLink">公式のデバイス認証ページを開く</a>
        </div>
        <p id="login-message" class="message" role="status" aria-live="polite" hidden></p>
        <p class="security-note" data-i18n="loginHelp">コードを控え、この端末をインターネットにつながるWi-Fiまたはモバイル回線へ切り替えて認証してください。ESP32はそのまま認証完了を待ちます。初期設定後のPC・スマートフォンの常時接続は不要です。</p>
        <p class="security-note" data-i18n="credentialsNotice">ChatGPTのパスワードや認証トークンは入力しません。ChatGPTへのログインは公式ページで行います。</p>
      </section>

      <section class="card timeout-card" aria-labelledby="timeout-heading">
        <div class="card-heading">
          <div>
            <h2 id="timeout-heading" data-i18n="timeoutHeading">画面の自動消灯</h2>
            <p data-i18n="timeoutIntro">操作がない時間を選ぶと、その時間後に LCD が消灯します。消灯中もデータ更新は続き、画面に触れると復帰します。</p>
          </div>
          <span class="step" aria-hidden="true">03</span>
        </div>

        <form id="timeout-form" class="timeout-layout">
          <div class="field">
            <label for="timeout" data-i18n="timeoutLabel">消灯まで</label>
            <select id="timeout" name="value">
              <option value="15000" data-i18n="timeout15">15 秒</option>
              <option value="30000" data-i18n="timeout30">30 秒</option>
              <option value="60000" data-i18n="timeout60">1 分</option>
              <option value="120000" data-i18n="timeout120">2 分</option>
              <option value="300000" data-i18n="timeout300">5 分</option>
              <option value="600000" data-i18n="timeout600">10 分</option>
              <option value="1800000" data-i18n="timeout1800">30 分</option>
              <option value="3600000" data-i18n="timeout3600">1 時間</option>
              <option value="7200000" data-i18n="timeout7200">2 時間</option>
            </select>
          </div>
          <button id="timeout-submit" class="button button-primary" type="submit" data-i18n="saveTimeout">消灯時間を保存</button>
        </form>
        <p id="timeout-message" class="message" role="status" aria-live="polite" hidden></p>
      </section>
    </div>
  </main>

  <script>
    (() => {
      'use strict';

      const SETUP_NONCE = '__SETUP_NONCE__';
      const TIMEOUTS = [15000, 30000, 60000, 120000, 300000, 600000, 1800000, 3600000, 7200000];

      const copy = {
        ja: {
          documentTitle: 'Codex Usage Monitor — 初期設定',
          appName: 'Codex Usage Monitor',
          brandAriaLabel: 'Codex Usage Monitor の初期設定',
          languageButton: 'English',
          languageSwitchAria: '英語表示に切り替える',
          eyebrow: '初期設定',
          title: 'Codex 使用量を表示する準備',
          subtitle: 'このページで Wi‑Fi とログインを一度設定すると、以後は PC やスマートフォンを常時接続しなくても、ESP32 が定期的に使用量を更新します。',
          stateHeading: '現在の状態',
          stateDescription: '設定後もこの画面で接続と認証の進み具合を確認できます。',
          wifiStateLabel: 'Wi‑Fi',
          loginStateLabel: 'Codex ログイン',
          deviceStateLabel: 'デバイス',
          stateChecking: '確認中…',
          connected: '接続済み',
          disconnected: '未接続',
          authenticated: '認証済み',
          unauthenticated: '未認証',
          deviceReady: '利用可能',
          deviceWaiting: '待機中',
          serverChecking: '状態を取得しています。',
          serverUnavailable: '状態を取得できません。接続を確認して再試行します。',
          wifiHeading: 'Wi‑Fi を設定',
          wifiIntro: '2.4GHz 帯の Wi‑Fi に接続します。',
          ssidLabel: 'Wi‑Fi 名（SSID）',
          ssidPlaceholder: '例: MyHome-2G',
          passwordLabel: 'Wi‑Fi パスワード',
          passwordPlaceholder: 'オープンネットワークなら空欄',
          passwordHelp: '入力内容はこの送信だけに使い、ブラウザーには保存しません。',
          saveWifi: 'Wi‑Fi を保存',
          savingWifi: '接続を開始しています…',
          wifiSuccess: 'Wi‑Fi 設定を送信しました。接続状態を確認しています。',
          wifiRequired: 'Wi‑Fi 名を入力してください。',
          wifiFailure: 'Wi‑Fi 設定に失敗しました。',
          loginHeading: 'Codex にログイン',
          loginIntro: 'ログインを開始するとデバイスコードが表示されます。下の公式ページを開いてコードを入力してください。',
          deviceCodeLabel: 'デバイスコード',
          noDeviceCode: 'ログイン開始後にコードが表示されます',
          copyCode: 'コードをコピー',
          copiedCode: 'コードをコピーしました。',
          copyFailure: 'コードをコピーできませんでした。表示されたコードを使ってください。',
          startLogin: 'ログインを開始',
          startingLogin: 'ログインを準備しています…',
          loginSuccess: 'ログインを開始しました。デバイスコードを確認してください。',
          loginFailure: 'ログインの開始に失敗しました。',
          authLink: '公式のデバイス認証ページを開く',
          loginHelp: 'コードを控え、この端末をインターネットにつながるWi-Fiまたはモバイル回線へ切り替えて認証してください。ESP32はそのまま認証完了を待ちます。初期設定後のPC・スマートフォンの常時接続は不要です。',
          credentialsNotice: 'ChatGPTのパスワードや認証トークンは入力しません。ChatGPTへのログインは公式ページで行います。',
          timeoutHeading: '画面の自動消灯',
          timeoutIntro: '操作がない時間を選ぶと、その時間後に LCD が消灯します。消灯中もデータ更新は続き、画面に触れると復帰します。',
          timeoutLabel: '消灯まで',
          timeout15: '15 秒',
          timeout30: '30 秒',
          timeout60: '1 分',
          timeout120: '2 分',
          timeout300: '5 分',
          timeout600: '10 分',
          timeout1800: '30 分',
          timeout3600: '1 時間',
          timeout7200: '2 時間',
          saveTimeout: '消灯時間を保存',
          savingTimeout: '消灯時間を保存しています…',
          timeoutSuccess: '消灯時間を保存しました。',
          timeoutFailure: '消灯時間の保存に失敗しました。',
          invalidTimeout: '消灯時間を選び直してください。',
          serverFailure: 'デバイスから有効な応答を受け取れませんでした。',
          httpFailure: 'デバイスがリクエストを受け付けませんでした。',
          copiedUnavailable: 'このブラウザーではコピー機能を利用できません。'
        },
        en: {
          documentTitle: 'Codex Usage Monitor — Initial setup',
          appName: 'Codex Usage Monitor',
          brandAriaLabel: 'Codex Usage Monitor initial setup',
          languageButton: '日本語',
          languageSwitchAria: 'Switch to Japanese',
          eyebrow: 'Initial setup',
          title: 'Prepare to show your Codex usage',
          subtitle: 'Set up Wi‑Fi and login once here. After that, the ESP32 keeps updating usage without a PC or phone staying connected.',
          stateHeading: 'Current status',
          stateDescription: 'Use this page to follow the connection and authentication progress.',
          wifiStateLabel: 'Wi‑Fi',
          loginStateLabel: 'Codex login',
          deviceStateLabel: 'Device',
          stateChecking: 'Checking…',
          connected: 'Connected',
          disconnected: 'Not connected',
          authenticated: 'Authenticated',
          unauthenticated: 'Not authenticated',
          deviceReady: 'Ready',
          deviceWaiting: 'Waiting',
          serverChecking: 'Reading device status.',
          serverUnavailable: 'Device status is unavailable. Retrying while you check the connection.',
          wifiHeading: 'Set up Wi‑Fi',
          wifiIntro: 'Connect to a 2.4 GHz Wi‑Fi network.',
          ssidLabel: 'Wi‑Fi name (SSID)',
          ssidPlaceholder: 'Example: MyHome-2G',
          passwordLabel: 'Wi‑Fi password',
          passwordPlaceholder: 'Leave blank for an open network',
          passwordHelp: 'The values are used only for this submission and are not saved in the browser.',
          saveWifi: 'Save Wi‑Fi',
          savingWifi: 'Starting the connection…',
          wifiSuccess: 'Wi‑Fi settings sent. Checking the connection status.',
          wifiRequired: 'Enter a Wi‑Fi name.',
          wifiFailure: 'Could not save the Wi‑Fi settings.',
          loginHeading: 'Log in to Codex',
          loginIntro: 'Start login to receive a device code. Open the official page below and enter the code there.',
          deviceCodeLabel: 'Device code',
          noDeviceCode: 'The code will appear after login starts',
          copyCode: 'Copy code',
          copiedCode: 'The code was copied.',
          copyFailure: 'Could not copy the code. Use the code shown here.',
          startLogin: 'Start login',
          startingLogin: 'Preparing login…',
          loginSuccess: 'Login started. Check the device code.',
          loginFailure: 'Could not start login.',
          authLink: 'Open the official device authorization page',
          loginHelp: 'Note the code, switch this phone or computer to an internet-connected Wi-Fi network or mobile data, then authorize it. The ESP32 keeps waiting for approval. No PC or phone needs to stay connected after setup.',
          credentialsNotice: 'Do not enter your ChatGPT password or authentication token here. Sign in to ChatGPT on the official page.',
          timeoutHeading: 'Automatic display sleep',
          timeoutIntro: 'Choose how long the LCD stays on without interaction. Updates continue while it is off, and a touch wakes it.',
          timeoutLabel: 'Turn off after',
          timeout15: '15 seconds',
          timeout30: '30 seconds',
          timeout60: '1 minute',
          timeout120: '2 minutes',
          timeout300: '5 minutes',
          timeout600: '10 minutes',
          timeout1800: '30 minutes',
          timeout3600: '1 hour',
          timeout7200: '2 hours',
          saveTimeout: 'Save sleep time',
          savingTimeout: 'Saving sleep time…',
          timeoutSuccess: 'Sleep time saved.',
          timeoutFailure: 'Could not save the sleep time.',
          invalidTimeout: 'Choose a valid sleep time.',
          serverFailure: 'The device returned an invalid response.',
          httpFailure: 'The device did not accept the request.',
          copiedUnavailable: 'Copying is unavailable in this browser.'
        }
      };

      const elements = {
        languageToggle: document.getElementById('language-toggle'),
        wifiState: document.getElementById('wifi-state'),
        loginState: document.getElementById('login-state'),
        deviceState: document.getElementById('device-state'),
        serverStatus: document.getElementById('server-status'),
        globalMessage: document.getElementById('global-message'),
        wifiForm: document.getElementById('wifi-form'),
        ssid: document.getElementById('ssid'),
        password: document.getElementById('password'),
        wifiSubmit: document.getElementById('wifi-submit'),
        wifiMessage: document.getElementById('wifi-message'),
        loginSubmit: document.getElementById('login-submit'),
        copyCode: document.getElementById('copy-code'),
        deviceCode: document.getElementById('device-code'),
        loginMessage: document.getElementById('login-message'),
        timeoutForm: document.getElementById('timeout-form'),
        timeout: document.getElementById('timeout'),
        timeoutSubmit: document.getElementById('timeout-submit'),
        timeoutMessage: document.getElementById('timeout-message')
      };

      let language = 'ja';
      let pollInFlight = false;
      let timeoutPending = null;
      let timeoutRevision = 0;
      let stateError = false;
      let state = {
        connected: null,
        authenticated: null,
        status: '',
        deviceCode: '',
        timeoutMs: null
      };

      const translate = (key) => {
        return (copy[language] && copy[language][key]) || copy.ja[key] || key;
      };

      const isTimeout = (value) => TIMEOUTS.includes(Number(value));

      const safeErrorText = (error, fallbackKey) => {
        if (error && typeof error.message === 'string' && error.message.trim()) {
          return error.message.trim();
        }
        return translate(fallbackKey);
      };

      function applyTranslations() {
        document.documentElement.lang = language;
        document.title = translate('documentTitle');
        document.querySelectorAll('[data-i18n]').forEach((node) => {
          node.textContent = translate(node.getAttribute('data-i18n'));
        });
        document.querySelectorAll('[data-i18n-placeholder]').forEach((node) => {
          node.setAttribute('placeholder', translate(node.getAttribute('data-i18n-placeholder')));
        });
        document.querySelectorAll('[data-i18n-aria-label]').forEach((node) => {
          node.setAttribute('aria-label', translate(node.getAttribute('data-i18n-aria-label')));
        });
        renderState();
        renderDeviceCode();
        renderMessages();
      }

      function setMessage(element, message, tone) {
        if (!element) {
          return;
        }
        if (!message) {
          element.hidden = true;
          element.textContent = '';
          element.removeAttribute('data-tone');
          return;
        }
        element.hidden = false;
        element.textContent = message;
        element.dataset.tone = tone || 'info';
      }

      const messages = {
        wifi: null,
        login: null,
        timeout: null,
        global: null
      };

      function renderMessages() {
        const render = (element, message) => {
          if (!message) {
            setMessage(element, '');
            return;
          }
          setMessage(element, message.key ? translate(message.key) : message.text, message.tone);
        };
        render(elements.wifiMessage, messages.wifi);
        render(elements.loginMessage, messages.login);
        render(elements.timeoutMessage, messages.timeout);
        render(elements.globalMessage, messages.global);
      }

      function setMessageState(name, keyOrText, tone, isText) {
        messages[name] = keyOrText
          ? { key: isText ? null : keyOrText, text: isText ? keyOrText : '', tone: tone || 'info' }
          : null;
        renderMessages();
      }

      function stateText(value, yesKey, noKey) {
        if (value === true) {
          return { text: translate(yesKey), state: 'yes' };
        }
        if (value === false) {
          return { text: translate(noKey), state: 'no' };
        }
        return { text: translate('stateChecking'), state: 'unknown' };
      }

      function renderState() {
        const wifi = stateText(state.connected, 'connected', 'disconnected');
        const login = stateText(state.authenticated, 'authenticated', 'unauthenticated');
        const device = state.authenticated === true
          ? { text: translate('deviceReady'), state: 'yes' }
          : state.authenticated === false
            ? { text: translate('deviceWaiting'), state: 'unknown' }
            : { text: translate('stateChecking'), state: 'unknown' };

        [
          [elements.wifiState, wifi],
          [elements.loginState, login],
          [elements.deviceState, device]
        ].forEach(([element, value]) => {
          element.textContent = value.text;
          element.dataset.state = value.state;
        });

        elements.serverStatus.textContent = stateError
          ? translate('serverUnavailable')
          : state.status || translate('serverChecking');

        if (timeoutPending === null && isTimeout(state.timeoutMs)) {
          elements.timeout.value = String(state.timeoutMs);
        }
      }

      function renderDeviceCode() {
        const code = typeof state.deviceCode === 'string' ? state.deviceCode.trim() : '';
        elements.deviceCode.textContent = code || translate('noDeviceCode');
        elements.deviceCode.dataset.empty = code ? 'false' : 'true';
        elements.copyCode.disabled = !code;
      }

      function validateState(payload) {
        if (!payload || typeof payload !== 'object' || Array.isArray(payload)) {
          throw new Error(translate('serverFailure'));
        }
        return {
          connected: typeof payload.connected === 'boolean' ? payload.connected : null,
          authenticated: typeof payload.authenticated === 'boolean' ? payload.authenticated : null,
          status: typeof payload.status === 'string' ? payload.status : '',
          deviceCode: typeof payload.deviceCode === 'string' ? payload.deviceCode : '',
          timeoutMs: isTimeout(payload.timeoutMs) ? Number(payload.timeoutMs) : null
        };
      }

      async function readJson(response) {
        let payload = null;
        try {
          payload = await response.json();
        } catch (error) {
          throw new Error(translate('serverFailure'));
        }
        if (!response.ok) {
          const serverError = payload && typeof payload.error === 'string' ? payload.error : '';
          throw new Error(serverError || translate('httpFailure'));
        }
        if (!payload || payload.ok === false) {
          const serverError = payload && typeof payload.error === 'string' ? payload.error : '';
          throw new Error(serverError || translate('httpFailure'));
        }
        return payload;
      }

      async function pollState() {
        if (pollInFlight) {
          return;
        }
        pollInFlight = true;
        const timeoutRevisionAtRequest = timeoutRevision;
        try {
          const response = await fetch('/api/state', {
            method: 'GET',
            headers: { Accept: 'application/json' },
            cache: 'no-store',
            credentials: 'same-origin'
          });
          const nextState = validateState(await readJson(response));
          if (timeoutRevisionAtRequest !== timeoutRevision) {
            nextState.timeoutMs = timeoutPending !== null ? timeoutPending : state.timeoutMs;
          }
          state = nextState;
          stateError = false;
          if (timeoutPending !== null && nextState.timeoutMs === timeoutPending) {
            timeoutPending = null;
          }
          renderState();
          renderDeviceCode();
        } catch (error) {
          stateError = true;
          renderState();
        } finally {
          pollInFlight = false;
        }
      }

      async function postForm(path, values) {
        const hasValues = values && typeof values === 'object';
        const request = {
          method: 'POST',
          headers: {
            Accept: 'application/json',
            'X-Setup-Key': SETUP_NONCE
          },
          cache: 'no-store',
          credentials: 'same-origin'
        };
        if (hasValues) {
          request.headers['Content-Type'] = 'application/x-www-form-urlencoded;charset=UTF-8';
          request.body = new URLSearchParams(values).toString();
        }
        return readJson(await fetch(path, request));
      }

      function setBusy(form, busy) {
        if (!form) {
          return;
        }
        if (form.matches && form.matches('button')) {
          form.disabled = busy;
          form.setAttribute('aria-busy', busy ? 'true' : 'false');
          return;
        }
        form.setAttribute('aria-busy', busy ? 'true' : 'false');
        form.querySelectorAll('button').forEach((button) => {
          button.disabled = busy;
        });
      }

      async function saveWifi(event) {
        event.preventDefault();
        const ssid = elements.ssid.value;
        if (!ssid.trim()) {
          setMessageState('wifi', 'wifiRequired', 'error');
          elements.ssid.focus();
          return;
        }
        setMessageState('wifi', 'savingWifi', 'info');
        setBusy(elements.wifiForm, true);
        try {
          await postForm('/api/wifi', { ssid, password: elements.password.value });
          setMessageState('wifi', 'wifiSuccess', 'success');
          await pollState();
        } catch (error) {
          setMessageState('wifi', safeErrorText(error, 'wifiFailure'), 'error', true);
        } finally {
          setBusy(elements.wifiForm, false);
        }
      }

      async function startLogin() {
        setMessageState('login', 'startingLogin', 'info');
        setBusy(elements.loginSubmit, true);
        try {
          await postForm('/api/login');
          setMessageState('login', 'loginSuccess', 'success');
          await pollState();
        } catch (error) {
          setMessageState('login', safeErrorText(error, 'loginFailure'), 'error', true);
        } finally {
          setBusy(elements.loginSubmit, false);
        }
      }

      async function copyDeviceCode() {
        const code = typeof state.deviceCode === 'string' ? state.deviceCode.trim() : '';
        if (!code) {
          return;
        }
        if (!navigator.clipboard || typeof navigator.clipboard.writeText !== 'function') {
          setMessageState('login', 'copiedUnavailable', 'warning');
          return;
        }
        try {
          await navigator.clipboard.writeText(code);
          setMessageState('login', 'copiedCode', 'success');
        } catch (error) {
          setMessageState('login', 'copyFailure', 'warning');
        }
      }

      async function saveTimeout(event) {
        event.preventDefault();
        const value = Number(elements.timeout.value);
        if (!isTimeout(value)) {
          setMessageState('timeout', 'invalidTimeout', 'error');
          return;
        }
        timeoutRevision += 1;
        timeoutPending = value;
        renderState();
        setMessageState('timeout', 'savingTimeout', 'info');
        setBusy(elements.timeoutForm, true);
        try {
          await postForm('/api/timeout', { value: String(value) });
          if (timeoutPending === value) {
            state.timeoutMs = value;
            timeoutPending = null;
            renderState();
          }
          setMessageState('timeout', 'timeoutSuccess', 'success');
          await pollState();
        } catch (error) {
          if (timeoutPending === value) {
            timeoutPending = null;
          }
          renderState();
          setMessageState('timeout', safeErrorText(error, 'timeoutFailure'), 'error', true);
        } finally {
          setBusy(elements.timeoutForm, false);
        }
      }

      elements.languageToggle.addEventListener('click', () => {
        language = language === 'ja' ? 'en' : 'ja';
        applyTranslations();
      });
      elements.wifiForm.addEventListener('submit', saveWifi);
      elements.loginSubmit.addEventListener('click', startLogin);
      elements.copyCode.addEventListener('click', copyDeviceCode);
      elements.timeoutForm.addEventListener('submit', saveTimeout);

      applyTranslations();
      pollState();
      window.setInterval(pollState, 3000);
    })();
  </script>
</body>
</html>
)ESP32SP_78a0e317";
