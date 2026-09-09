#include "config-store.h"

#include <ArduinoJson.h>

#include <memory>

#include "display-state.h"

namespace usage {
bool ConfigStore::begin() { return preferences_.begin("codex-usage", false); }

bool ConfigStore::load(Settings& settings, Tokens& tokens) {
  bool valid = true;
  for (const char* key : {"settings", "tokens"}) {
    if (!preferences_.isKey(key)) continue;
    const size_t length = preferences_.getBytesLength(key);
    if (length == 0 || length > 20000) {
      preferences_.remove(key);
      valid = false;
      continue;
    }
    std::unique_ptr<uint8_t[]> value(new (std::nothrow) uint8_t[length]);
    if (!value || preferences_.getBytes(key, value.get(), length) != length) {
      valid = false;
      continue;
    }
    JsonDocument document;
    const auto error = deserializeJson(document, value.get(), length);
    const bool isSettings = strcmp(key, "settings") == 0;
    const int version = document["version"].is<int>() ? document["version"].as<int>() : 0;
    bool okay = !error && (isSettings ? (version == 1 || version == 2) : version == 1);
    if (okay && strcmp(key, "settings") == 0) {
      Settings next;
      next.ssid = document["ssid"].as<String>();
      next.password = document["password"].as<String>();
      next.timeoutMs = document["timeout"].as<uint32_t>();
      next.displayFlipped = version == 1 ? false : document["flipped"].as<bool>();
      DisplayState state;
      okay = document["ssid"].is<const char*>() && document["password"].is<const char*>() &&
             next.ssid.length() <= 32 && next.password.length() <= 64 &&
             state.setTimeout(next.timeoutMs, 0) &&
             (version == 1 || document["flipped"].is<bool>());
      if (okay) {
        settings = next;
        if (version == 1 && !saveSettings(next)) valid = false;
      }
    } else if (okay) {
      Tokens next;
      next.access = document["access"].as<String>();
      next.refresh = document["refresh"].as<String>();
      next.account = document["account"].as<String>();
      next.expiresAt = document["expires"].as<int64_t>();
      okay = next.access.length() > 0 && next.access.length() <= 12000 &&
             next.refresh.length() > 0 && next.refresh.length() <= 4096 &&
             next.account.length() <= 128 && next.expiresAt > 0;
      if (okay) tokens = next;
    }
    if (!okay) {
      preferences_.remove(key);
      valid = false;
    }
  }
  return valid;
}

bool ConfigStore::saveSettings(const Settings& settings) {
  JsonDocument document;
  document["version"] = 2;
  document["ssid"] = settings.ssid;
  document["password"] = settings.password;
  document["timeout"] = settings.timeoutMs;
  document["flipped"] = settings.displayFlipped;
  String value;
  serializeJson(document, value);
  return preferences_.putBytes("settings", value.c_str(), value.length()) == value.length();
}

bool ConfigStore::saveTokens(const Tokens& tokens) {
  JsonDocument document;
  document["version"] = 1;
  document["access"] = tokens.access;
  document["refresh"] = tokens.refresh;
  document["account"] = tokens.account;
  document["expires"] = tokens.expiresAt;
  String value;
  serializeJson(document, value);
  // NVS commits a blob atomically and supports tokens larger than its 4000-byte string limit.
  return preferences_.putBytes("tokens", value.c_str(), value.length()) == value.length();
}

bool ConfigStore::clearTokens() {
  return !preferences_.isKey("tokens") || preferences_.remove("tokens");
}
}  // namespace usage
