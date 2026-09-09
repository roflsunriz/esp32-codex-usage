#pragma once
#include <Arduino.h>

#include "usage-data.h"

namespace usage {
struct Snapshot {
  Window fiveHour;
  Window weekly;
  String status = "起動中";
  String deviceCode;
  String apName;
  String apPassword;
  String address;
  bool connected = false;
  bool authenticated = false;
  bool setupActive = false;
  uint32_t updatedAt = 0;
  uint32_t timeoutMs = 60000;
  bool displayFlipped = false;
  uint16_t wifiDisconnectReason = 0;
};
enum class CommandType { Refresh, Login, Setup, Timeout, FlipDisplay };
struct Command {
  CommandType type;
  uint32_t value = 0;
};
void startNetwork();
bool networkSnapshot(Snapshot& snapshot);
bool sendCommand(Command command);
}  // namespace usage
