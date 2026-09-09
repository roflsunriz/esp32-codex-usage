#pragma once
#include <Arduino.h>
#include <Preferences.h>

namespace usage {
struct Settings {
  String ssid;
  String password;
  uint32_t timeoutMs = 60000;
  bool displayFlipped = false;
};
struct Tokens {
  String access;
  String refresh;
  String account;
  int64_t expiresAt = 0;
};
class ConfigStore {
 public:
  bool begin();
  bool load(Settings& settings, Tokens& tokens);
  bool saveSettings(const Settings& settings);
  bool saveTokens(const Tokens& tokens);
  bool clearTokens();

 private:
  Preferences preferences_;
};
}  // namespace usage
