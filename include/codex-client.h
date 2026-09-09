#pragma once
#include "config-store.h"
#include "usage-data.h"

namespace usage {
struct DeviceLogin {
  String id;
  String code;
  uint32_t intervalMs = 5000;
  uint32_t startedAt = 0;
};
enum class PollResult { Pending, Complete, Failed };
class CodexClient {
 public:
  String error;
  bool beginLogin(DeviceLogin& login);
  PollResult pollLogin(const DeviceLogin& login, Tokens& tokens);
  bool refresh(Tokens& tokens);
  bool fetchUsage(const Tokens& tokens, Reading& reading);
  int lastStatus = 0;
  bool refreshRejected = false;

 private:
  bool decodeTokens(JsonVariantConst value, Tokens& tokens, bool refreshing);
};
}  // namespace usage
