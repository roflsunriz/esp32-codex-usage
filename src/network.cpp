#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <time.h>

#include <atomic>

#include "app-model.h"
#include "codex-client.h"
#include "config-store.h"
#include "connection-watchdog.h"
#include "display-state.h"
#include "setup-page.h"

namespace usage {
namespace {
SemaphoreHandle_t snapshotMutex;
QueueHandle_t commands;
Snapshot published;
Snapshot current;
Settings settings;
Tokens tokens;
ConfigStore store;
CodexClient client;
DeviceLogin login;
WebServer server(IPAddress(192, 168, 4, 1), 80);
String setupNonce;
bool storageReady = false;
bool loginPending = false;
bool requestLogin = false;
bool requestRefresh = true;
bool wifiChanged = false;
uint32_t setupStarted = 0;
ConnectionWatchdog connectionWatchdog;
uint32_t lastAttempt = 0;
uint32_t lastPoll = 0;
uint32_t retryDelay = 60000;
bool hadWifi = false;
std::atomic<uint16_t> disconnectReason{0};

void publish() {
  current.authenticated = !tokens.refresh.isEmpty();
  current.connected = WiFi.status() == WL_CONNECTED;
  current.timeoutMs = settings.timeoutMs;
  current.displayFlipped = settings.displayFlipped;
  current.wifiDisconnectReason = disconnectReason.load();
  if (xSemaphoreTake(snapshotMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    published = current;
    xSemaphoreGive(snapshotMutex);
  }
}

String randomText() {
  char text[25];
  snprintf(text, sizeof(text), "%08lx%08lx%08lx", static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
  return text;
}

void reply(int status, const String& error = "") {
  JsonDocument document;
  document["ok"] = status == 200;
  if (!error.isEmpty()) document["error"] = error;
  String body;
  serializeJson(document, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(status, "application/json; charset=utf-8", body);
}

bool permitted(bool writing) {
  const IPAddress remote = server.client().remoteIP();
  const IPAddress local = WiFi.softAPIP();
  if (!current.setupActive || remote[0] != local[0] || remote[1] != local[1] ||
      remote[2] != local[2]) {
    reply(403, "本体の設定用Wi-Fiに接続してください");
    return false;
  }
  // Reject DNS rebinding and cross-origin writes; browsers cannot read this nonce cross-origin.
  const String host = server.hostHeader();
  if (host != "192.168.4.1" && host != "192.168.4.1:80") {
    reply(403, "http://192.168.4.1 から開いてください");
    return false;
  }
  if (writing && server.header("X-Setup-Key") != setupNonce) {
    reply(403, "設定ページを再読み込みしてください");
    return false;
  }
  return true;
}

void configureServer() {
  const char* headers[] = {"X-Setup-Key"};
  server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, [] {
    if (!permitted(false)) return;
    String page(kSetupPage);
    page.replace("__SETUP_NONCE__", setupNonce);
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("X-Frame-Options", "DENY");
    server.sendHeader("Content-Security-Policy",
                      "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
                      "frame-ancestors 'none'; form-action 'self'");
    server.send(200, "text/html; charset=utf-8", page);
  });
  server.on("/api/state", HTTP_GET, [] {
    if (!permitted(false)) return;
    JsonDocument document;
    document["connected"] = WiFi.status() == WL_CONNECTED;
    document["authenticated"] = !tokens.refresh.isEmpty();
    document["status"] = current.status;
    document["deviceCode"] = current.deviceCode;
    document["timeoutMs"] = settings.timeoutMs;
    document["displayFlipped"] = settings.displayFlipped;
    String body;
    serializeJson(document, body);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json; charset=utf-8", body);
  });
  server.on("/api/wifi", HTTP_POST, [] {
    if (!permitted(true)) return;
    const String ssid = server.arg("ssid"), password = server.arg("password");
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 64 ||
        (!password.isEmpty() && password.length() < 8)) {
      reply(400, "SSIDは1〜32バイト、パスワードは8〜64文字（オープンネットワークは空欄）です");
      return;
    }
    Settings next = settings;
    next.ssid = ssid;
    next.password = password;
    if (!storageReady || !store.saveSettings(next)) {
      reply(500, "本体への保存に失敗しました");
      return;
    }
    settings = next;
    wifiChanged = true;
    current.status = "Wi-Fi接続を開始します";
    reply(200);
  });
  server.on("/api/login", HTTP_POST, [] {
    if (!permitted(true)) return;
    if (WiFi.status() != WL_CONNECTED) {
      reply(409, "先にWi-Fi接続を完了してください");
      return;
    }
    if (!loginPending) requestLogin = true;
    reply(200);
  });
  server.on("/api/timeout", HTTP_POST, [] {
    if (!permitted(true)) return;
    const String raw = server.arg("value");
    char* end = nullptr;
    const unsigned long value = strtoul(raw.c_str(), &end, 10);
    DisplayState check;
    if (raw.isEmpty() || !end || *end || !check.setTimeout(value, 0)) {
      reply(400, "一覧から消灯時間を選んでください");
      return;
    }
    Settings next = settings;
    next.timeoutMs = value;
    if (!storageReady || !store.saveSettings(next)) {
      reply(500, "設定を保存できません");
      return;
    }
    settings = next;
    reply(200);
  });
  server.onNotFound([] { reply(404, "ページがありません"); });
}

void beginSetup() {
  if (current.setupActive) {
    setupStarted = millis();
    return;
  }
  setupNonce = randomText();
  current.apPassword = randomText().substring(0, 12);
  current.apName = "Codex-Usage-" + randomText().substring(0, 4);
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                         IPAddress(255, 255, 255, 0))) {
    current.status = "設定用Wi-Fiのアドレスを設定できません";
    return;
  }
  if (!WiFi.softAP(current.apName.c_str(), current.apPassword.c_str(), 1, false, 2)) {
    current.status = "設定用Wi-Fiを開始できません";
    return;
  }
  current.address = "http://192.168.4.1";
  current.setupActive = true;
  setupStarted = millis();
  server.begin();
}

void endSetup() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  current.setupActive = false;
  current.apPassword = "";
  current.apName = "";
  current.address = "";
  setupNonce = "";
}

bool persistTokens() {
  if (!storageReady || !store.saveTokens(tokens)) {
    tokens = Tokens{};
    store.clearTokens();
    current.status = "認証を保存できません。再ログインが必要です";
    return false;
  }
  return true;
}

bool refreshTokens() {
  if (client.refresh(tokens)) return persistTokens();
  current.status = client.error;
  if (client.refreshRejected) {
    tokens = Tokens{};
    current.fiveHour = Window{};
    current.weekly = Window{};
    if (!store.clearTokens()) current.status = "認証の削除に失敗。再ログインしてください";
  }
  return false;
}

void updateUsage() {
  lastAttempt = millis();
  requestRefresh = false;
  bool okay = true;
  if (tokens.expiresAt <= static_cast<int64_t>(time(nullptr)) + 120) okay = refreshTokens();
  Reading reading;
  if (okay) {
    okay = client.fetchUsage(tokens, reading);
    if (!okay && client.lastStatus == 401) {
      okay = refreshTokens();
      if (okay) okay = client.fetchUsage(tokens, reading);
    }
  }
  if (okay) {
    current.fiveHour = reading.fiveHour;
    current.weekly = reading.weekly;
    current.updatedAt = millis();
    current.status = "更新済み";
    retryDelay = 60000;
  } else {
    if (!client.error.isEmpty()) current.status = client.error;
    retryDelay = std::min<uint32_t>(retryDelay * 2U, 900000U);
  }
  publish();
}

void processCommand(const Command& command) {
  switch (command.type) {
    case CommandType::Refresh:
      requestRefresh = true;
      break;
    case CommandType::Login:
      if (!loginPending) requestLogin = true;
      break;
    case CommandType::Setup:
      beginSetup();
      break;
    case CommandType::Timeout: {
      DisplayState check;
      if (!check.setTimeout(command.value, millis())) break;
      Settings next = settings;
      next.timeoutMs = command.value;
      if (storageReady && store.saveSettings(next))
        settings = next;
      else
        current.status = "消灯設定を保存できません";
      break;
    }
    case CommandType::FlipDisplay: {
      Settings next = settings;
      next.displayFlipped = !next.displayFlipped;
      if (storageReady && store.saveSettings(next))
        settings = next;
      else
        current.status = "画面の向きを保存できません";
      break;
    }
  }
}

void networkTask(void*) {
  storageReady = store.begin();
  if (!storageReady)
    current.status = "本体の保存領域を開けません";
  else if (!store.load(settings, tokens))
    current.status = "保存データを修復しました。設定を確認";
  else
    current.status = settings.ssid.isEmpty() ? "接続タブから初期設定" : "Wi-Fi接続中";
  WiFi.persistent(false);
  WiFi.onEvent(
      [](arduino_event_id_t, arduino_event_info_t info) {
        disconnectReason.store(info.wifi_sta_disconnected.reason);
      },
      ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  configureServer();
  if (settings.ssid.isEmpty())
    beginSetup();
  else {
    WiFi.begin(settings.ssid.c_str(), settings.password.c_str());
  }
  configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");
  publish();
  for (;;) {
    const uint32_t now = millis();
    Command command;
    while (xQueueReceive(commands, &command, 0) == pdTRUE) processCommand(command);
    if (current.setupActive) {
      server.handleClient();
      if (static_cast<uint32_t>(now - setupStarted) >= 900000) endSetup();
    }
    if (wifiChanged) {
      WiFi.disconnect(false, false);
      WiFi.begin(settings.ssid.c_str(), settings.password.c_str());
      connectionWatchdog.reset();
      wifiChanged = false;
    }
    const bool connected = WiFi.status() == WL_CONNECTED;
    const bool associated = WiFi.STA.connected();
    const bool reconnectDue = connectionWatchdog.tick(associated, connected, now);
    if (!connected) {
      if (associated) current.status = "IPアドレスを取得中";
      if (!settings.ssid.isEmpty() && reconnectDue) {
        current.status =
            associated ? "IPアドレスの取得を再試行" : "Wi-Fi再接続中。接続タブで設定変更";
        WiFi.disconnect(false, false);
        WiFi.begin(settings.ssid.c_str(), settings.password.c_str());
        connectionWatchdog.reset();
      }
      hadWifi = false;
    } else {
      if (!hadWifi) {
        hadWifi = true;
        requestRefresh = true;
        current.status = tokens.refresh.isEmpty() ? "接続済み。ログインしてください" : "接続済み";
      }
      const bool clockReady = time(nullptr) >= 1735689600;
      if (!clockReady) current.status = "時刻を同期中。NTP接続を確認";
      if (clockReady && requestLogin) {
        requestLogin = false;
        if (client.beginLogin(login)) {
          loginPending = true;
          current.deviceCode = login.code;
          current.status = "ブラウザで認証コードを入力";
          lastPoll = millis();
        } else
          current.status = client.error;
        publish();
      }
      if (clockReady && loginPending &&
          static_cast<uint32_t>(millis() - lastPoll) >= login.intervalMs) {
        lastPoll = millis();
        Tokens next;
        const PollResult result = client.pollLogin(login, next);
        if (result != PollResult::Pending) {
          loginPending = false;
          current.deviceCode = "";
          login = DeviceLogin{};
          if (result == PollResult::Complete) {
            tokens = next;
            if (persistTokens()) {
              current.status = "ログイン完了";
              requestRefresh = true;
            }
          } else
            current.status = client.error;
        }
      }
      if (clockReady && !loginPending && !tokens.refresh.isEmpty() &&
          (requestRefresh || static_cast<uint32_t>(now - lastAttempt) >= retryDelay))
        updateUsage();
    }
    publish();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}  // namespace

void startNetwork() {
  snapshotMutex = xSemaphoreCreateMutex();
  commands = xQueueCreate(8, sizeof(Command));
  if (!snapshotMutex || !commands ||
      xTaskCreatePinnedToCore(networkTask, "usage-network", 16384, nullptr, 1, nullptr, 0) !=
          pdPASS) {
    published.status = "通信処理を開始できません。再起動してください";
  }
}

bool networkSnapshot(Snapshot& result) {
  if (!snapshotMutex) {
    result = published;
    return true;
  }
  if (xSemaphoreTake(snapshotMutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  result = published;
  xSemaphoreGive(snapshotMutex);
  return true;
}

bool sendCommand(Command command) {
  return commands && xQueueSend(commands, &command, 0) == pdTRUE;
}
}  // namespace usage
