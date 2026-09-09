#include <Arduino.h>

#include "app-model.h"
#include "board-display.h"
#include "boot-button.h"
#include "diagnostic-driver.h"
#include "display-state.h"

namespace {

CodexUsageDisplay gDisplay;
LGFX_Sprite gCanvas(&gDisplay);
DisplayState gDisplayState;
BootButton gBootButton;
usage::Snapshot gSnapshot;

constexpr uint16_t kFrameWidth = 320U;
constexpr uint16_t kFrameHeight = 240U;
constexpr size_t kFrameBufferBytes = static_cast<size_t>(kFrameWidth) * kFrameHeight;
bool gCanvasReady = false;
bool gCanvasFailure = false;

lgfx::LovyanGFX& frameTarget() {
  return gCanvasReady ? static_cast<lgfx::LovyanGFX&>(gCanvas)
                      : static_cast<lgfx::LovyanGFX&>(gDisplay);
}

constexpr uint32_t kSnapshotPeriodMs = 250U;
constexpr uint32_t kStaleAfterMs = 120000U;
constexpr uint8_t kBootPin = 0U;

const uint32_t kTimeoutOptions[] = {
    15000U, 30000U, 60000U, 120000U, 300000U, 600000U, 1800000U, 3600000U, 7200000U,
};

const char* const kTimeoutLabels[] = {
    "15s", "30s", "1m", "2m", "5m", "10m", "30m", "1h", "2h",
};

constexpr size_t kTimeoutOptionCount = sizeof(kTimeoutOptions) / sizeof(kTimeoutOptions[0]);

struct Rect {
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

constexpr Rect kTabRects[] = {
    {0U, 2U, 104U, 28U},
    {106U, 2U, 104U, 28U},
    {212U, 2U, 106U, 28U},
};
constexpr Rect kUsageRefreshRect = {238U, 36U, 74U, 26U};
constexpr Rect kTimeoutRects[] = {
    {6U, 54U, 98U, 30U},  {110U, 54U, 98U, 30U},  {214U, 54U, 98U, 30U},
    {6U, 92U, 98U, 30U},  {110U, 92U, 98U, 30U},  {214U, 92U, 98U, 30U},
    {6U, 130U, 98U, 30U}, {110U, 130U, 98U, 30U}, {214U, 130U, 98U, 30U},
};
constexpr Rect kSettingsActionRects[] = {
    {6U, 190U, 96U, 30U},
    {112U, 190U, 96U, 30U},
    {218U, 190U, 96U, 30U},
};
constexpr Rect kConnectionActionRects[] = {
    {6U, 198U, 96U, 30U},
    {112U, 198U, 96U, 30U},
    {218U, 198U, 96U, 30U},
};

enum class Tab : uint8_t { Usage = 0, Settings = 1, Connection = 2 };

Tab gTab = Tab::Usage;
bool gDirty = true;
bool gLastAwake = false;
uint32_t gLastSnapshotAt = 0U;
String gLocalNotice;
uint32_t gLocalNoticeUntil = 0U;
bool gLastLocalNoticeActive = false;
bool gLastStale = true;
bool gDisplayOrientationKnown = false;
bool gAppliedDisplayFlipped = false;

constexpr uint16_t kBackground = 0x0000;
constexpr uint16_t kPanel = 0x18E3;
constexpr uint16_t kPanelSelected = 0x39E7;
constexpr uint16_t kAccent = 0x07FF;
constexpr uint16_t kGood = 0x07E0;
constexpr uint16_t kWarning = 0xFD20;
constexpr uint16_t kText = 0xFFFF;
constexpr uint16_t kMuted = 0xBDF7;

String percentText(const usage::Window& window) {
  if (!window.available) {
    return String("--");
  }

  String text = String(window.used, 1);
  text += "%";
  return text;
}

bool contains(const Rect& rect, uint16_t x, uint16_t y) {
  return x >= rect.x && y >= rect.y && static_cast<uint32_t>(x - rect.x) < rect.width &&
         static_cast<uint32_t>(y - rect.y) < rect.height;
}

template <size_t N>
int hitIndex(const Rect (&rects)[N], uint16_t x, uint16_t y) {
  for (size_t i = 0U; i < N; ++i) {
    if (contains(rects[i], x, y)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

size_t nextUtf8Index(const String& text, size_t index) {
  const size_t length = text.length();
  if (index >= length) {
    return length;
  }

  const uint8_t first = static_cast<uint8_t>(text[index]);
  size_t width = 1U;
  if ((first & 0x80U) == 0U) {
    return index + 1U;
  }
  if ((first & 0xE0U) == 0xC0U) {
    width = 2U;
  } else if ((first & 0xF0U) == 0xE0U) {
    width = 3U;
  } else if ((first & 0xF8U) == 0xF0U) {
    width = 4U;
  } else {
    return index + 1U;
  }

  if (index + width > length) {
    return index + 1U;
  }
  for (size_t i = 1U; i < width; ++i) {
    if ((static_cast<uint8_t>(text[index + i]) & 0xC0U) != 0x80U) {
      return index + 1U;
    }
  }
  return index + width;
}

String fitText(const String& text, int32_t maxWidth) {
  if (maxWidth <= 0) {
    return String();
  }
  if (frameTarget().textWidth(text) <= maxWidth) {
    return text;
  }

  const String ellipsis = "...";
  if (frameTarget().textWidth(ellipsis) > maxWidth) {
    return String();
  }

  String result;
  size_t index = 0U;
  while (index < text.length()) {
    const size_t next = nextUtf8Index(text, index);
    const String part =
        text.substring(static_cast<unsigned int>(index), static_cast<unsigned int>(next));
    const String candidate = result + part + ellipsis;
    if (frameTarget().textWidth(candidate) > maxWidth) {
      break;
    }
    result += part;
    index = next;
  }
  return result + ellipsis;
}

void drawFittedText(const String& text, uint16_t x, uint16_t y, uint16_t maxWidth,
                    uint16_t color = kText, uint16_t background = kBackground) {
  frameTarget().setTextColor(color, background);
  frameTarget().drawString(fitText(text, maxWidth), x, y);
}

bool localNoticeActive(uint32_t now) {
  return !gLocalNotice.isEmpty() && static_cast<int32_t>(now - gLocalNoticeUntil) < 0;
}

void setLocalNotice(const String& notice, uint32_t now) {
  gLocalNotice = notice;
  gLocalNoticeUntil = now + 3000U;
  gDirty = true;
}

String visibleStatus(uint32_t now) {
  if (gCanvasFailure) {
    return String("描画用メモリ不足。再起動してください");
  }
  return localNoticeActive(now) ? gLocalNotice : gSnapshot.status;
}

bool snapshotIsStale(const usage::Snapshot& snapshot, uint32_t now) {
  return snapshot.updatedAt == 0U ||
         static_cast<uint32_t>(now - snapshot.updatedAt) > kStaleAfterMs;
}

bool windowChanged(const usage::Window& before, const usage::Window& after) {
  return before.available != after.available || before.used != after.used ||
         before.resetsAt != after.resetsAt;
}

bool snapshotDisplayChanged(const usage::Snapshot& before, const usage::Snapshot& after) {
  return windowChanged(before.fiveHour, after.fiveHour) ||
         windowChanged(before.weekly, after.weekly) || before.status != after.status ||
         before.deviceCode != after.deviceCode || before.apName != after.apName ||
         before.apPassword != after.apPassword || before.address != after.address ||
         before.connected != after.connected || before.authenticated != after.authenticated ||
         before.setupActive != after.setupActive || before.timeoutMs != after.timeoutMs ||
         before.displayFlipped != after.displayFlipped;
}

void drawButton(const Rect& rect, const String& label, bool selected = false) {
  const uint16_t fill = selected ? kPanelSelected : kPanel;
  frameTarget().fillRoundRect(rect.x, rect.y, rect.width, rect.height, 4, fill);
  frameTarget().drawRoundRect(rect.x, rect.y, rect.width, rect.height, 4,
                              selected ? kAccent : kMuted);
  const uint16_t maxWidth = rect.width > 12U ? rect.width - 12U : rect.width;
  const String visible = fitText(label, maxWidth);
  const int32_t textWidth = frameTarget().textWidth(visible);
  const uint16_t textX =
      static_cast<uint16_t>(rect.x + (textWidth < rect.width ? (rect.width - textWidth) / 2 : 0));
  const uint16_t textY =
      static_cast<uint16_t>(rect.y + (rect.height > 16U ? (rect.height - 16U) / 2U : 0U));
  frameTarget().setTextColor(kText, fill);
  frameTarget().drawString(visible, textX, textY);
}

void drawTabs() {
  const String labels[] = {String("使用量"), String("設定"), String("接続")};
  const Tab selected = gTab;

  for (uint8_t i = 0U; i < 3U; ++i) {
    drawButton(kTabRects[i], labels[i], static_cast<Tab>(i) == selected);
  }
}

void drawProgress(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                  const usage::Window& window) {
  frameTarget().drawRoundRect(x, y, width, height, 4, kMuted);
  if (!window.available) {
    return;
  }

  float used = window.used;
  if (used < 0.0f) {
    used = 0.0f;
  } else if (used > 100.0f) {
    used = 100.0f;
  }

  const uint16_t innerWidth = width > 4U ? width - 4U : 0U;
  const uint16_t fillWidth = static_cast<uint16_t>(innerWidth * used / 100.0f);
  if (fillWidth > 0U) {
    frameTarget().fillRoundRect(x + 2U, y + 2U, fillWidth, height - 4U, 3,
                                used >= 90.0f ? kWarning : kGood);
  }
}

void drawUsage(uint32_t now) {
  frameTarget().setTextColor(kText, kBackground);
  frameTarget().drawString(String("Codex 使用量"), 8U, 38U);
  drawButton(kUsageRefreshRect, String("更新"));

  frameTarget().setTextColor(kText, kBackground);
  frameTarget().drawString(String("5時間 使用率"), 10U, 70U);
  drawProgress(10U, 88U, 300U, 24U, gSnapshot.fiveHour);
  frameTarget().setTextColor(kAccent, kBackground);
  frameTarget().drawString(percentText(gSnapshot.fiveHour), 256U, 70U);

  frameTarget().setTextColor(kText, kBackground);
  frameTarget().drawString(String("週間 使用率"), 10U, 132U);
  drawProgress(10U, 150U, 300U, 24U, gSnapshot.weekly);
  frameTarget().setTextColor(kAccent, kBackground);
  frameTarget().drawString(percentText(gSnapshot.weekly), 256U, 132U);

  String status = visibleStatus(now);
  if (snapshotIsStale(gSnapshot, now)) {
    status += gSnapshot.updatedAt == 0U ? String("  (未取得)") : String("  (古い)");
  }
  drawFittedText(status, 8U, 210U, 304U, snapshotIsStale(gSnapshot, now) ? kWarning : kMuted);
}

void drawSettings(uint32_t now) {
  frameTarget().setTextColor(kText, kBackground);
  frameTarget().drawString(String("消灯時間"), 8U, 38U);

  for (size_t i = 0U; i < kTimeoutOptionCount; ++i) {
    drawButton(kTimeoutRects[i], String(kTimeoutLabels[i]),
               gDisplayState.timeout() == kTimeoutOptions[i]);
  }

  drawFittedText(String("状態: ") + visibleStatus(now), 8U, 170U, 304U,
                 snapshotIsStale(gSnapshot, now) ? kWarning : kMuted);
  drawButton(kSettingsActionRects[0], String("初期設定"));
  drawButton(kSettingsActionRects[1], String("ログイン"));
  drawButton(kSettingsActionRects[2], String("更新"));
}

void drawConnection(uint32_t now) {
  frameTarget().setTextColor(kText, kBackground);
  frameTarget().drawString(String("接続とセットアップ"), 8U, 38U);

  String line = String("状態: ") + visibleStatus(now);
  frameTarget().setTextColor(gSnapshot.connected ? kGood : kWarning, kBackground);
  drawFittedText(line, 8U, 66U, 304U, gSnapshot.connected ? kGood : kWarning);

  line = String("AP: ");
  line += gSnapshot.apName.length() == 0U ? String("--") : gSnapshot.apName;
  drawFittedText(line, 8U, 91U, 304U);

  line = String("パスワード: ");
  line += gSnapshot.apPassword.length() == 0U ? String("--") : gSnapshot.apPassword;
  drawFittedText(line, 8U, 116U, 304U);

  line = String("認証コード: ");
  line += gSnapshot.deviceCode.length() == 0U ? String("--") : gSnapshot.deviceCode;
  drawFittedText(line, 8U, 141U, 304U);

  line = gSnapshot.deviceCode.length() != 0U
             ? String("auth.openai.com/codex/device")
             : String("アドレス: ") +
                   (gSnapshot.address.length() == 0U ? String("--") : gSnapshot.address);
  drawFittedText(line, 8U, 166U, 304U);

  drawButton(kConnectionActionRects[0], String("初期設定"));
  drawButton(kConnectionActionRects[1], String("ログイン"));
  drawButton(kConnectionActionRects[2], String("更新"));
}

void drawFrame(uint32_t now) {
  if (!gDisplayState.awake()) {
    return;
  }

  frameTarget().fillScreen(kBackground);
  frameTarget().setFont(&fonts::lgfxJapanGothic_16);
  frameTarget().setTextSize(1U);
  frameTarget().setTextWrap(false, false);
  drawTabs();

  switch (gTab) {
    case Tab::Usage:
      drawUsage(now);
      break;
    case Tab::Settings:
      drawSettings(now);
      break;
    case Tab::Connection:
      drawConnection(now);
      break;
  }
  if (gCanvasReady) {
    gCanvas.pushSprite(&gDisplay, 0, 0);
  }
  gDirty = false;
}

bool sendCommand(usage::CommandType type, uint32_t value = 0U) {
  usage::Command command;
  command.type = type;
  command.value = value;
  if (!usage::sendCommand(command)) {
    setLocalNotice(String("通信キューへ送れません"), millis());
    return false;
  }
  gDirty = true;
  return true;
}

void applyTimeout(uint32_t value, uint32_t now) {
  if (!sendCommand(usage::CommandType::Timeout, value)) {
    return;
  }

  // worker がNVSへ保存し、Snapshot.timeoutMsで確認できた後にだけ反映する。
  setLocalNotice(String("消灯設定を保存中"), now);
}

void handleAction(uint16_t x, uint16_t y, uint32_t now) {
  const int tabIndex = hitIndex(kTabRects, x, y);
  if (tabIndex >= 0) {
    gTab = static_cast<Tab>(tabIndex);
    gDirty = true;
    return;
  }

  if (gTab == Tab::Usage) {
    if (contains(kUsageRefreshRect, x, y)) {
      sendCommand(usage::CommandType::Refresh);
    }
    return;
  }

  if (gTab == Tab::Settings) {
    const int timeoutIndex = hitIndex(kTimeoutRects, x, y);
    if (timeoutIndex >= 0) {
      applyTimeout(kTimeoutOptions[static_cast<size_t>(timeoutIndex)], now);
      return;
    }
    const int actionIndex = hitIndex(kSettingsActionRects, x, y);
    if (actionIndex == 0) {
      sendCommand(usage::CommandType::Setup);
    } else if (actionIndex == 1) {
      sendCommand(usage::CommandType::Login);
    } else if (actionIndex == 2) {
      sendCommand(usage::CommandType::Refresh);
    }
    return;
  }

  const int actionIndex = hitIndex(kConnectionActionRects, x, y);
  if (actionIndex == 0) {
    sendCommand(usage::CommandType::Setup);
  } else if (actionIndex == 1) {
    sendCommand(usage::CommandType::Login);
  } else if (actionIndex == 2) {
    sendCommand(usage::CommandType::Refresh);
  }
}

void updateLocalNotice(uint32_t now) {
  const bool active = localNoticeActive(now);
  if (active != gLastLocalNoticeActive) {
    gLastLocalNoticeActive = active;
    gDirty = true;
  }
}

void updateSnapshot(uint32_t now) {
  if (static_cast<uint32_t>(now - gLastSnapshotAt) < kSnapshotPeriodMs) {
    return;
  }
  gLastSnapshotAt = now;
  usage::Snapshot next;
  if (!usage::networkSnapshot(next)) {
    const bool stale = snapshotIsStale(gSnapshot, now);
    if (stale != gLastStale && gDisplayState.awake()) gDirty = true;
    gLastStale = stale;
    return;  // Retain the last confirmed orientation, timeout and reading on a transient lock
             // timeout.
  }
  const bool stale = snapshotIsStale(next, now);
  const bool changed = snapshotDisplayChanged(gSnapshot, next) || stale != gLastStale;
  gLastStale = stale;
  gSnapshot = next;
  bool orientationChanged = false;
  if (!gDisplayOrientationKnown || gAppliedDisplayFlipped != gSnapshot.displayFlipped) {
    // board-display.h の offset_rotation=1 では 0/2 が横長の表裏になる。
    gDisplay.setRotation(gSnapshot.displayFlipped ? 2U : 0U);
    gAppliedDisplayFlipped = gSnapshot.displayFlipped;
    gDisplayOrientationKnown = true;
    orientationChanged = true;
  }
  // 起動時に worker が読み込んだ保存済み設定を画面側にも反映する。
  // 変更がない場合はタイマーをリセットしない。
  bool timeoutChanged = false;
  if (gSnapshot.timeoutMs != gDisplayState.timeout()) {
    if (gDisplayState.setTimeout(gSnapshot.timeoutMs, now)) {
      timeoutChanged = true;
    }
  }
  if ((changed || timeoutChanged || orientationChanged) && gDisplayState.awake()) {
    gDirty = true;
  }
}

void syncAwake() {
  const bool awake = gDisplayState.awake();
  if (awake == gLastAwake) {
    return;
  }

  gLastAwake = awake;
  gDisplay.setBrightness(awake ? 255U : 0U);
  if (awake) {
    gDirty = true;
  }
}

}  // namespace

void setup() {
#ifdef USAGE_DIAGNOSTICS
  usage::diagnostics::begin();
  Serial.println("{\"phase\":\"display-init\"}");
#endif
  const uint32_t now = millis();
  gDisplay.init();
  gCanvas.setColorDepth(8U);
  gCanvasReady = gCanvas.createSprite(kFrameWidth, kFrameHeight) != nullptr;
  gCanvasFailure = !gCanvasReady;
  if (gCanvasReady) {
    gCanvas.setFont(&fonts::lgfxJapanGothic_16);
    gCanvas.setTextSize(1U);
    gCanvas.setTextWrap(false, false);
  }
#ifdef USAGE_DIAGNOSTICS
  Serial.print("{\"phase\":\"framebuffer-ready\",\"ok\":");
  Serial.print(gCanvasReady ? "true" : "false");
  Serial.print(",\"bytes\":");
  Serial.print(gCanvasReady ? kFrameBufferBytes : 0U);
  Serial.println("}");
#endif
  // board-display.h の panel offset_rotation=1 が 320x240 の横長を選ぶ。
  // ここで setRotation(1) を重ねると内部回転が 2 になり縦長になる。
  gDisplay.setBrightness(255U);
  gDisplayState.setTimeout(gDisplayState.timeout(), now);
  pinMode(kBootPin, INPUT_PULLUP);
  gBootButton.begin(digitalRead(kBootPin) == LOW, now);
  gLastAwake = true;
  gLastSnapshotAt = static_cast<uint32_t>(now - kSnapshotPeriodMs);
  usage::startNetwork();
#ifdef USAGE_DIAGNOSTICS
  Serial.println("{\"phase\":\"ready\"}");
#endif
  drawFrame(now);
}

void loop() {
#ifdef USAGE_DIAGNOSTICS
  usage::diagnostics::poll();
  const uint32_t now = usage::diagnostics::now();
#else
  const uint32_t now = millis();
#endif
  gDisplayState.tick(now);

  bool bootPressed = digitalRead(kBootPin) == LOW;
#ifdef USAGE_DIAGNOSTICS
  usage::diagnostics::bootOverride(bootPressed);
#endif
  if (gBootButton.update(bootPressed, now)) {
    gDisplayState.wake(now);
    if (sendCommand(usage::CommandType::FlipDisplay)) {
      setLocalNotice(String("表示向きを切替中"), now);
    }
  }

  uint16_t touchX = 0U;
  uint16_t touchY = 0U;
  bool pressed = gDisplay.getTouch(&touchX, &touchY);
#ifdef USAGE_DIAGNOSTICS
  usage::diagnostics::touchOverride(touchX, touchY, pressed);
  if (usage::diagnostics::redrawRequested()) gDirty = true;
#endif
  if (gDisplayState.touch(pressed, now)) {
    handleAction(touchX, touchY, now);
  }

  syncAwake();
  updateSnapshot(now);
  updateLocalNotice(now);
  if (gDirty && gDisplayState.awake()) {
    drawFrame(now);
  }
#ifdef USAGE_DIAGNOSTICS
  usage::diagnostics::finishFrame(gDisplay, gDisplayState, gSnapshot, gCanvasReady);
#endif
  yield();
}
