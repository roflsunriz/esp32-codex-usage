#include "codex-client.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>
#include <time.h>

#include <memory>

#include "trusted-roots.h"

namespace usage {
namespace {
constexpr const char* kIssuer = "https://auth.openai.com";
constexpr const char* kClientId = "app_EMoamEEZ73f0CkXaXp7hrann";
constexpr size_t kMaxBody = 32768;

class LimitedBody : public Stream {
 public:
  String text;
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  size_t write(const uint8_t* data, size_t size) override {
    if (text.length() + size > kMaxBody) return 0;
    return text.concat(reinterpret_cast<const char*>(data), size) ? size : 0;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};

int request(const String& url, const String& body, const Tokens* tokens, bool form,
            JsonDocument& output) {
  WiFiClientSecure tls;
  tls.setCACert(kTrustedRoots);
  tls.setHandshakeTimeout(12);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(12000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(tls, url)) return -1;
  http.setUserAgent("codex-cli");
  if (tokens) {
    http.addHeader("Authorization", "Bearer " + tokens->access);
    if (tokens->account.length()) http.addHeader("ChatGPT-Account-Id", tokens->account);
  }
  http.addHeader("Accept", "application/json");
  int code;
  if (body.length()) {
    http.addHeader("Content-Type", form ? "application/x-www-form-urlencoded" : "application/json");
    code = http.POST(body);
  } else
    code = http.GET();
  if (code > 0) {
    LimitedBody buffer;
    const int declared = http.getSize();
    const int received = declared > static_cast<int>(kMaxBody) ? -1 : http.writeToStream(&buffer);
    const auto parsed = deserializeJson(output, buffer.text);
    const bool invalid = declared > static_cast<int>(kMaxBody) || received < 0 || parsed;
#ifdef USAGE_DIAGNOSTICS
    if (invalid) {
      Serial.printf("HTTP_PARSE status=%d declared=%d received=%d length=%u parse=%s heap=%u\n",
                    code, declared, received, static_cast<unsigned>(buffer.text.length()),
                    parsed.c_str(), ESP.getFreeHeap());
    }
#endif
    if (invalid && code >= 200 && code < 300) code = -2;
  }
  http.end();
  return code;
}

String encoded(const String& value) {
  String result;
  constexpr char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = value[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
        c == '_' || c == '.' || c == '~')
      result += static_cast<char>(c);
    else {
      result += '%';
      result += hex[c >> 4];
      result += hex[c & 15];
    }
  }
  return result;
}

bool jwtPayload(const String& token, JsonDocument& payload) {
  const int first = token.indexOf('.');
  const int second = token.indexOf('.', first + 1);
  if (first < 1 || second <= first || second - first > 16000) return false;
  String value = token.substring(first + 1, second);
  value.replace('-', '+');
  value.replace('_', '/');
  while (value.length() % 4) value += '=';
  const size_t capacity = value.length() * 3 / 4 + 1;
  std::unique_ptr<unsigned char[]> decoded(new (std::nothrow) unsigned char[capacity]);
  size_t size = 0;
  if (!decoded || mbedtls_base64_decode(decoded.get(), capacity, &size,
                                        reinterpret_cast<const unsigned char*>(value.c_str()),
                                        value.length()) != 0)
    return false;
  return !deserializeJson(payload, decoded.get(), size);
}

String failure(int code) {
  if (code == -2) return "応答形式を確認できません";
  if (code < 0) return "TLS/通信失敗。時刻とWi-Fiを確認";
  if (code == 401) return "認証期限切れ。再ログインしてください";
  if (code == 403) return "アクセス拒否。アカウント設定を確認";
  if (code == 429) return "要求が多すぎます。自動再試行します";
  return "通信エラー HTTP " + String(code);
}
}  // namespace

bool CodexClient::beginLogin(DeviceLogin& login) {
  JsonDocument requestBody, response;
  requestBody["client_id"] = kClientId;
  String body;
  serializeJson(requestBody, body);
  lastStatus = request(String(kIssuer) + "/api/accounts/deviceauth/usercode", body, nullptr, false,
                       response);
  if (lastStatus != 200) {
    error = failure(lastStatus);
    return false;
  }
  DeviceLogin next;
  next.id = response["device_auth_id"].as<String>();
  next.code = response["user_code"].as<String>();
  if (next.code.isEmpty()) next.code = response["usercode"].as<String>();
  const unsigned long interval = response["interval"].is<const char*>()
                                     ? strtoul(response["interval"].as<const char*>(), nullptr, 10)
                                     : response["interval"].as<unsigned long>();
  next.intervalMs = constrain(interval, 5UL, 60UL) * 1000;
  next.startedAt = millis();
  if (next.id.isEmpty() || next.id.length() > 512 || next.code.isEmpty() ||
      next.code.length() > 32) {
    error = "認証コードの形式が不正です";
    return false;
  }
  login = next;
  error = "";
  return true;
}

PollResult CodexClient::pollLogin(const DeviceLogin& login, Tokens& tokens) {
  if (static_cast<uint32_t>(millis() - login.startedAt) >= 900000) {
    error = "認証コードが期限切れです。再発行してください";
    return PollResult::Failed;
  }
  JsonDocument requestBody, response;
  requestBody["device_auth_id"] = login.id;
  requestBody["user_code"] = login.code;
  String body;
  serializeJson(requestBody, body);
  lastStatus =
      request(String(kIssuer) + "/api/accounts/deviceauth/token", body, nullptr, false, response);
  if (lastStatus == 403 || lastStatus == 404) return PollResult::Pending;
  if (lastStatus != 200) {
    error = failure(lastStatus);
    return PollResult::Failed;
  }
  const String code = response["authorization_code"].as<String>();
  const String verifier = response["code_verifier"].as<String>();
  if (code.isEmpty() || verifier.isEmpty()) {
    error = "認証応答が不正です";
    return PollResult::Failed;
  }
  body = "grant_type=authorization_code&code=" + encoded(code) +
         "&redirect_uri=" + encoded(String(kIssuer) + "/deviceauth/callback") +
         "&client_id=" + encoded(kClientId) + "&code_verifier=" + encoded(verifier);
  response.clear();
  lastStatus = request(String(kIssuer) + "/oauth/token", body, nullptr, true, response);
  if (lastStatus != 200) {
    error = failure(lastStatus);
    return PollResult::Failed;
  }
  return decodeTokens(response.as<JsonVariantConst>(), tokens, false) ? PollResult::Complete
                                                                      : PollResult::Failed;
}

bool CodexClient::decodeTokens(JsonVariantConst value, Tokens& tokens, bool refreshing) {
  Tokens next = refreshing ? tokens : Tokens{};
  next.access = value["access_token"].as<String>();
  if (value["refresh_token"].is<const char*>()) next.refresh = value["refresh_token"].as<String>();
  JsonDocument claims;
  if (next.access.isEmpty() || next.access.length() > 12000 || next.refresh.isEmpty() ||
      next.refresh.length() > 4096 || !jwtPayload(next.access, claims)) {
    error = "認証トークンの形式が不正です";
    return false;
  }
  next.expiresAt = claims["exp"].as<int64_t>();
  const String account = claims["https://api.openai.com/auth"]["chatgpt_account_id"].as<String>();
  if (account.length()) next.account = account;
  if (value["id_token"].is<const char*>()) {
    claims.clear();
    if (jwtPayload(value["id_token"].as<String>(), claims)) {
      const String idAccount =
          claims["https://api.openai.com/auth"]["chatgpt_account_id"].as<String>();
      if (idAccount.length()) next.account = idAccount;
    }
  }
  if (next.expiresAt <= static_cast<int64_t>(time(nullptr)) || next.account.length() > 128) {
    error = "認証期限またはアカウント情報が不正です";
    return false;
  }
  tokens = next;
  error = "";
  return true;
}

bool CodexClient::refresh(Tokens& tokens) {
  refreshRejected = false;
  JsonDocument requestBody, response;
  requestBody["client_id"] = kClientId;
  requestBody["grant_type"] = "refresh_token";
  requestBody["refresh_token"] = tokens.refresh;
  String body;
  serializeJson(requestBody, body);
  lastStatus = request(String(kIssuer) + "/oauth/token", body, nullptr, false, response);
  if (lastStatus != 200) {
    String code = response["error"].is<const char*>() ? response["error"].as<String>()
                                                      : response["error"]["code"].as<String>();
    refreshRejected = lastStatus == 401 || code == "invalid_grant" ||
                      code == "refresh_token_expired" || code == "refresh_token_reused" ||
                      code == "refresh_token_invalidated";
    error = refreshRejected ? "再ログインが必要です" : failure(lastStatus);
    return false;
  }
  return decodeTokens(response.as<JsonVariantConst>(), tokens, true);
}

bool CodexClient::fetchUsage(const Tokens& tokens, Reading& reading) {
  JsonDocument response;
  lastStatus = request("https://chatgpt.com/backend-api/wham/usage", "", &tokens, false, response);
  if (lastStatus != 200) {
    error = failure(lastStatus);
    return false;
  }
  if (!parseUsage(response.as<JsonVariantConst>(), reading)) {
    error = "使用量の応答形式が不正です";
    return false;
  }
  error = "";
  return true;
}
}  // namespace usage
