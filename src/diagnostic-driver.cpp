#ifdef USAGE_DIAGNOSTICS
#include "diagnostic-driver.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <lwip/dhcp.h>
#include <lwip/tcpip.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
namespace usage {
namespace diagnostics {
namespace {
constexpr uint32_t kSerialBaud = 921600U;
constexpr uint32_t kMaxAdvanceMs = 7200000U;
constexpr size_t kInputCapacity = 1024U;
constexpr size_t kPollByteBudget = 128U;
constexpr size_t kMaxStatusBytes = 256U;
constexpr size_t kMaxJsonBytes = 1024U;
enum class Pending : uint8_t { None, State, Touch, Advance, Redraw, Physical, Boot, Screen, Error };
struct SyntheticTouch {
  bool active = false;
  uint16_t x = 0U;
  uint16_t y = 0U;
  bool pressed = false;
};
char input[kInputCapacity];
size_t inputLength = 0U;
bool discardingInput = false;
bool started = false;
uint32_t virtualOffset = 0U;
bool redraw = false;
bool frameBuffered = false;
SyntheticTouch syntheticTouch;
bool syntheticBootActive = false;
bool syntheticBootPressed = false;
Pending pending = Pending::None;
char pendingError[64] = "";
void queueError(const char* message) {
  strncpy(pendingError, message ? message : "diagnostic_error", sizeof(pendingError) - 1U);
  pendingError[sizeof(pendingError) - 1U] = '\0';
  pending = Pending::Error;
}
bool readInteger(JsonVariantConst value, uint32_t& result) {
  if (!value.is<JsonInteger>() && !value.is<JsonUInt>()) return false;
  const int64_t number = value.as<int64_t>();
  if (number < 0 || number > 0xFFFFFFFFLL) return false;
  result = static_cast<uint32_t>(number);
  return true;
}
bool readTouchCoordinate(JsonVariantConst value, uint16_t& result) {
  uint32_t number = 0U;
  if (!readInteger(value, number) || number > 65535U) return false;
  result = static_cast<uint16_t>(number);
  return true;
}
bool readBool(JsonVariantConst value, bool& result) {
  if (!value.is<bool>()) return false;
  result = value.as<bool>();
  return true;
}
// Keep diagnostic metadata bounded without cutting a UTF-8 code point in half.
String boundedStatus(const String& source) {
  String result;
  result.reserve(source.length() < kMaxStatusBytes ? source.length() : kMaxStatusBytes);
  size_t index = 0U;
  while (index < source.length() && result.length() < kMaxStatusBytes) {
    const uint8_t first = static_cast<uint8_t>(source[index]);
    size_t width = 1U;
    if ((first & 0x80U) == 0U) {
      width = 1U;
    } else if ((first & 0xE0U) == 0xC0U) {
      width = 2U;
    } else if ((first & 0xF0U) == 0xE0U) {
      width = 3U;
    } else if ((first & 0xF8U) == 0xF0U) {
      width = 4U;
    }
    bool valid = index + width <= source.length() && width > 1U;
    for (size_t i = 1U; valid && i < width; ++i) {
      valid = (static_cast<uint8_t>(source[index + i]) & 0xC0U) == 0x80U;
    }
    if (!valid) width = 1U;
    if (result.length() + width > kMaxStatusBytes) break;
    for (size_t i = 0U; i < width; ++i) result += source[index + i];
    index += width;
  }
  return result;
}
void appendState(JsonDocument& document, lgfx::LGFX_Device& display, const DisplayState& state,
                 const usage::Snapshot& snapshot) {
  document["awake"] = state.awake();
  document["timeoutMs"] = state.timeout();
  document["connected"] = snapshot.connected;
  document["authenticated"] = snapshot.authenticated;
  document["displayFlipped"] = snapshot.displayFlipped;
  document["buffered"] = frameBuffered;
  document["wifiDisconnectReason"] = snapshot.wifiDisconnectReason;
  document["wifiStatus"] = static_cast<int>(WiFi.status());
  document["wifiAssociated"] = WiFi.STA.connected();
  document["uptime"] = millis();
  esp_netif_t* station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (station) {
    esp_netif_dhcp_status_t status;
    if (esp_netif_dhcpc_get_status(station, &status) == ESP_OK) document["dhcpStatus"] = status;
    auto* native = static_cast<struct netif*>(esp_netif_get_netif_impl(station));
    if (native) {
      LOCK_TCPIP_CORE();
      const struct dhcp* lease = netif_dhcp_data(native);
      const int phase = lease ? lease->state : -1;
      const int tries = lease ? lease->tries : 0;
      UNLOCK_TCPIP_CORE();
      document["dhcpPhase"] = phase;
      document["dhcpTries"] = tries;
    }
  }
  document["fiveHourAvailable"] = snapshot.fiveHour.available;
  if (snapshot.fiveHour.available && isfinite(snapshot.fiveHour.used)) {
    document["fiveHourUsed"] = snapshot.fiveHour.used;
  } else {
    document["fiveHourUsed"] = nullptr;
  }
  document["weeklyAvailable"] = snapshot.weekly.available;
  if (snapshot.weekly.available && isfinite(snapshot.weekly.used)) {
    document["weeklyUsed"] = snapshot.weekly.used;
  } else {
    document["weeklyUsed"] = nullptr;
  }
  document["ip"] = WiFi.localIP().toString();
  document["status"] = boundedStatus(snapshot.status);
  document["updatedAt"] = snapshot.updatedAt;
  document["width"] = display.width();
  document["height"] = display.height();
  document["rotation"] = display.getRotation();
  document["freeHeap"] = ESP.getFreeHeap();
  document["brightness"] = display.getBrightness();
}
void sendJson(JsonDocument& document) {
  String encoded;
  serializeJson(document, encoded);
  if (encoded.length() > kMaxJsonBytes - 1U) encoded = "{\"error\":\"response_too_long\"}";
  Serial.write(reinterpret_cast<const uint8_t*>(encoded.c_str()), encoded.length());
  Serial.write('\n');
}
void parseCommand() {
  input[inputLength] = '\0';
  JsonDocument document;
  if (deserializeJson(document, input, inputLength)) {
    inputLength = 0U;
    queueError("invalid_json");
    return;
  }
  inputLength = 0U;
  JsonVariantConst commandValue = document["cmd"];
  if (!commandValue.is<const char*>()) {
    queueError("missing_cmd");
    return;
  }
  const char* command = commandValue.as<const char*>();
  if (strcmp(command, "state") == 0) {
    pending = Pending::State;
  } else if (strcmp(command, "screen") == 0) {
    pending = Pending::Screen;
  } else if (strcmp(command, "redraw") == 0) {
    redraw = true;
    pending = Pending::Redraw;
  } else if (strcmp(command, "physical") == 0) {
    syntheticTouch.active = false;
    syntheticBootActive = false;
    redraw = true;
    pending = Pending::Physical;
  } else if (strcmp(command, "boot") == 0) {
    bool pressed = false;
    if (!readBool(document["pressed"], pressed)) {
      queueError("invalid_boot");
      return;
    }
    syntheticBootActive = true;
    syntheticBootPressed = pressed;
    redraw = true;
    pending = Pending::Boot;
  } else if (strcmp(command, "touch") == 0) {
    uint16_t x = 0U;
    uint16_t y = 0U;
    bool pressed = false;
    if (!readTouchCoordinate(document["x"], x) || !readTouchCoordinate(document["y"], y) ||
        !readBool(document["pressed"], pressed)) {
      queueError("invalid_touch");
      return;
    }
    syntheticTouch.active = true;
    syntheticTouch.x = x;
    syntheticTouch.y = y;
    syntheticTouch.pressed = pressed;
    redraw = true;
    pending = Pending::Touch;
  } else if (strcmp(command, "advance") == 0) {
    uint32_t milliseconds = 0U;
    if (!readInteger(document["ms"], milliseconds) || milliseconds > kMaxAdvanceMs) {
      queueError("invalid_advance");
      return;
    }
    virtualOffset = static_cast<uint32_t>(virtualOffset + milliseconds);
    redraw = true;
    pending = Pending::Advance;
  } else {
    queueError("unknown_cmd");
  }
}
void sendScreen(lgfx::LGFX_Device& display, const DisplayState& state,
                const usage::Snapshot& snapshot) {
  const int32_t width = display.width();
  const int32_t height = display.height();
  const uint64_t rowBytes64 = width > 0 ? static_cast<uint64_t>(width) * 3ULL : 0ULL;
  const uint64_t imageBytes64 = rowBytes64 * (height > 0 ? static_cast<uint64_t>(height) : 0ULL);
  if (width <= 0 || height <= 0 || rowBytes64 > 0xFFFFFFFFULL || imageBytes64 > 0xFFFFFFFFULL) {
    JsonDocument document;
    appendState(document, display, state, snapshot);
    document["error"] = "invalid_screen";
    sendJson(document);
    return;
  }
  const size_t rowBytes = static_cast<size_t>(rowBytes64);
  uint8_t* row = static_cast<uint8_t*>(malloc(rowBytes));
  if (!row) {
    JsonDocument document;
    appendState(document, display, state, snapshot);
    document["error"] = "screen_buffer_unavailable";
    sendJson(document);
    return;
  }
  JsonDocument header;
  appendState(header, display, state, snapshot);
  header["bytes"] = static_cast<uint32_t>(imageBytes64);
  sendJson(header);  // Header line, then exactly bytes RGB data, then a newline.
  for (int32_t y = 0; y < height; ++y) {
    display.readRectRGB(0, y, width, 1, row);
    Serial.write(row, rowBytes);
  }
  free(row);
  Serial.write('\n');
}
}  // namespace
void begin() {
  if (started) return;
  Serial.begin(kSerialBaud);
  inputLength = 0U;
  discardingInput = false;
  virtualOffset = 0U;
  redraw = false;
  syntheticTouch = SyntheticTouch{};
  syntheticBootActive = false;
  syntheticBootPressed = false;
  pending = Pending::None;
  pendingError[0] = '\0';
  started = true;
}
uint32_t now() { return static_cast<uint32_t>(millis() + virtualOffset); }
bool touchOverride(uint16_t& x, uint16_t& y, bool& pressed) {
  if (!syntheticTouch.active) return false;
  if (pressed) {
    syntheticTouch.active = false;
    return false;
  }
  x = syntheticTouch.x;
  y = syntheticTouch.y;
  pressed = syntheticTouch.pressed;
  return true;
}
bool bootOverride(bool& pressed) {
  if (!syntheticBootActive) return false;
  if (pressed) {
    syntheticBootActive = false;
    return false;
  }
  pressed = syntheticBootPressed;
  return true;
}
void poll() {
  if (!started) begin();
  if (pending != Pending::None) return;
  size_t consumed = 0U;
  while (Serial.available() > 0 && consumed++ < kPollByteBudget) {
    const char current = static_cast<char>(Serial.read());
    if (discardingInput) {
      if (current == '\n') {
        discardingInput = false;
        queueError("command_too_long");
        break;
      }
      continue;
    }
    if (current == '\r') continue;
    if (current == '\n') {
      if (inputLength != 0U) parseCommand();
      break;
    }
    if (inputLength + 1U >= kInputCapacity) {
      inputLength = 0U;
      discardingInput = true;
    } else {
      input[inputLength++] = current;
    }
  }
}
bool redrawRequested() {
  const bool result = redraw;
  redraw = false;
  return result;
}
void finishFrame(lgfx::LGFX_Device& display, const DisplayState& state,
                 const usage::Snapshot& snapshot, bool buffered) {
  frameBuffered = buffered;
  if (pending == Pending::None) return;
  const Pending command = pending;
  pending = Pending::None;
  if (command == Pending::Screen) {
    sendScreen(display, state, snapshot);
    return;
  }
  JsonDocument document;
  appendState(document, display, state, snapshot);
  if (command == Pending::Error) document["error"] = pendingError;
  sendJson(document);
}
}  // namespace diagnostics
}  // namespace usage
#endif  // USAGE_DIAGNOSTICS
